#!/usr/bin/env python
# -*- coding: utf-8 -*-

import requests
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import time
import math
import logging
import socket

DEBUG = False
EVOK_API = "http://192.168.2.28:8080"
EVOK_API_RETRIES = 5
MQTT_BROKER = "192.168.2.2"
MQTT_PREFIX = "solarControl"

REDUCED_EXCHANGE_TIME = 1800
VOLTAGE_12_RAIL = 12

ADDR_RELAY_CO_CWU = "1"
ADDR_RELAY_SOL = "2"
ADDR_RELAY_PUMP = "3"
ADDR_RELAY_HEAT = "5"
ADDR_RELAY_CIRC = "4"

ADDR_TEMP_HEATER_IN = "28FF4D15151501C6"
ADDR_TEMP_HEATER_OUT = "28FF5AF502150270"
ADDR_TEMP_SOLAR_IN = "28FF0A9171150270"
ADDR_TEMP_SOLAR_OUT = "28FF1A181515019F"
ADDR_TEMP_TANK_UP = "28FF4C30041503A7"
ADDR_TEMP_INSIDE = "28FF89DB06000034"
ADDR_TEMP_OUTSIDE = "287CECBF060000DA"
# ADDR_TEMP_OUTSIDE = "28BECABF0600004F"

ADDR_INPUT_CIRCULATION = 1
ADDR_INPUT_MANUAL_MODE = 3
ADDR_INPUT_STANDALONE_MODE = 2

USE_THERMISTOR = False


class Controller():
    temp_sensor_addr_map = {
        "heater_in":  ADDR_TEMP_HEATER_IN,
        "heater_out": ADDR_TEMP_HEATER_OUT,
        "solar_in":   ADDR_TEMP_SOLAR_IN,
        "solar_out":  ADDR_TEMP_SOLAR_OUT,
        "tank_up":    ADDR_TEMP_TANK_UP,
        "outside":    ADDR_TEMP_OUTSIDE,
        "inside":     ADDR_TEMP_INSIDE
    }
    relays_addr_map = {
        "co_cwu":      ADDR_RELAY_CO_CWU,  # CWU == TRUE
        "solar":       ADDR_RELAY_SOL,
        "pump":        ADDR_RELAY_PUMP,
        "heater":      ADDR_RELAY_HEAT,
        "circulation": ADDR_RELAY_CIRC
    }
    mqtt_topics_map = {
        "actuators":   MQTT_PREFIX + "/actuators",
        "circulation": MQTT_PREFIX + "/circulate/pump",
        "heater_in":   MQTT_PREFIX + "/heater/temp_in",
        "heater_out":  MQTT_PREFIX + "/heater/temp_out",
        "solar_in":    MQTT_PREFIX + "/solar/temp_in",
        "solar_out":   MQTT_PREFIX + "/solar/temp_out",
        "solar_up":    MQTT_PREFIX + "/solar/temp",
        "solar_pump":  MQTT_PREFIX + "/solar/pump",
        "tank_up":     MQTT_PREFIX + "/tank/temp_up",
        "outside":     "outside/temp",
        "inside":      "room/1/temp_current"
    }
    thermistor = USE_THERMISTOR

    def __init__(self, standalone=False):
        self.log = logging.getLogger("SolarController")
        self.temperatures = {}
        self.relays = {}
        self.flow = 0
        self.circulation_last_run = 0
        self.actuators_value = 0  # Backwards compatibility
        self.external_room_temperature_source = True
        self.reduced_exchange_time = 0
        self.standalone = standalone
        self.settings = {
            "circulation": {
                "interval": 3600,
                "time_on": 12
            },
            "tank": {
                "solar_max": 65.0,
                "heater_max": 48.0,
                "heater_min": 44.0
            },
            "heater": {
                "hysteresis": 0.5,
                "critical": 80.0,
                "expected": 21,
                "schedule": None,
                "on": (6, 30),
                "off": (22, 0),
                "forced_heating": False
            },
            "solar": {
                "critical": 90.00,
                "on": 8.0,
                "off": 5.0,
                "flow": {
                    "s_min": 18,
                    "s_max": 30,
                    "t_min": 5.0,
                    "t_max": 9.0
                }
            }
        }
        self.manual = False
        self.relays = dict().fromkeys(self.relays_addr_map, True)
        self.temperatures = dict().fromkeys(self.temp_sensor_addr_map, -99)
        self.temperatures['solar_up'] = -99
        for relay in self.relays:
            self.set_relay(relay, False)
        self.log.debug("Controller initiated")

    def update_sensors(self):
        for key, _ in self.temp_sensor_addr_map.items():
            self.update_temperature(key)
        self.update_temperature('solar_up')

    def update_temperature(self, sensor):
        if sensor == "solar_up":
            value = round(self.get_solar_temperature(), 2)
            if self.temperatures[sensor] - 0.25 < value < self.temperatures[sensor] + 0.25:
                return
            if DEBUG:
                new = raw_input("Provide value for sensor '{}' or ENTER to ack value of '{}': ".format(sensor, value))
                if new != "":
                    value = float(new)
        elif sensor == "inside" and self.external_room_temperature_source:
            return
        else:
            addr = self.temp_sensor_addr_map[sensor]
            r = requests.get(EVOK_API + "/json/sensor/" + addr)
            try:
                value = float(r.json()['data']['value'])
            except KeyError:
                value = 199
                if r.status_code >= 500:
                    self.log.critical("Temperature sensor for {} ({}) is unavailable".format(sensor, addr))
            if DEBUG:
                new = raw_input("Provide value for sensor '{}' or ENTER to ack value of '{}' ".format(sensor, value))
                if new != "":
                    value = float(new)
        try:
            old = self.temperatures[sensor]
        except KeyError:
            old = None
        if old != value:
            self.temperatures[sensor] = value
            self.mqtt_publish(sensor, value)
            self.log.debug("Updated temperature from sensor {} to {} deg".format(sensor, value))

    def normalize_thermistor(self, voltage):
        B = 3950  # Thermistor coefficient (in Kelvin)
        T0 = 273.15 + 25  # Room temperature (in Kelvin)
        R0 = 100000  # Thermistor resistance at T0
        VCC = VOLTAGE_12_RAIL  # Voltage applied to resistance bridge
        R_ref = 100000  # Reference resistor
        # All calculations are in Kelvin, returned value is in Celsius
        MULT = 100000  # Increse precision
        try:
            resistance = (VCC - voltage) * R_ref / voltage
            inv_T = (1 * MULT / T0) + (1 * MULT / B) * math.log(resistance / R0)
            T = 1 * MULT / inv_T - 273.15
            self.log.debug("Calculated thermistor resistance is {} which maps to {} deg C".format(resistance, T))
            return T
        except (ZeroDivisionError, TypeError):
            return -99

    def get_solar_temperature(self):
        r = requests.get(EVOK_API + "/json/analoginput/1")
        voltage = float(r.json()['data']['value'])
        # Get real voltage reading
        voltage = voltage * VOLTAGE_12_RAIL / 12
        if self.thermistor:
            return self.normalize_thermistor(voltage)
        else:
            T_min = 0    # 0V
            T_max = 200  # 10V
            try:
                T = (T_max - T_min) * voltage / VOLTAGE_12_RAIL + T_min
                self.log.debug("Measured voltage is {} which maps to {} deg C".format(voltage, T))
                return T
            except (ZeroDivisionError, TypeError):
                return -99.0

    def get_input_bool(self, addr):
        r = requests.get(EVOK_API + "/json/input/" + str(addr))
        return bool(r.json()['data']['value'])

    def get_circulation(self):
        return self.get_input_bool(ADDR_INPUT_CIRCULATION)

    def get_manual(self):
        return self.get_input_bool(ADDR_INPUT_MANUAL_MODE)

    def get_standalone(self):
        if self.standalone:
            return True
        return self.get_input_bool(ADDR_INPUT_STANDALONE_MODE)

    def set_relay(self, relay, state):
        if self.relays[relay] == state:
            return
        addr = self.relays_addr_map[relay]
        data = {"value": str(int(state))}
        for i in range(0, EVOK_API_RETRIES):
            r = requests.post(EVOK_API + "/json/relay/" + addr, json=data)
            if r.status_code == 200:
                break
            if i == (EVOK_API_RETRIES - 1):
                self.log.critical("Relay state couldn't be changed. No comm to EVOK after {} retries".format(EVOK_API_RETRIES))
                return
        self.log.debug("Changed relay {} state to {}".format(relay, state))
        self.relays[relay] = state
        self.actuators()  # BACKWARDS COMPATIBILITY

    def set_flow(self, value):
        # Flow set to 0, sets Analog Output to minimum value, but sends value of 0 over MQTT
        # This allows to use min and max as maximum setpoints for hardware calibration
        # And allows easier analysis in the future
        no_flow = False
        if value == 0:
            no_flow = True
            value = self.settings['solar']['flow']['s_min']
        data = {"value": float(value) / 10}
        for i in range(0, EVOK_API_RETRIES):
            r = requests.post(EVOK_API + "/json/ao/1", json=data)
            if r.status_code == 200:
                break
            if i == (EVOK_API_RETRIES - 1):
                self.log.critical("Solar circuit flow couldn't be set. No comm to EVOK after {} retries".format(EVOK_API_RETRIES))
                return
        if no_flow:
            value = 0
        if self.flow != value:
            self.flow = value
            self.log.debug("Solar circuit flow set to {}%".format(value))
            self.mqtt_publish("solar_pump", value)

    # HELPER METHODS (unpack old mqtt schema) #
    def actuators(self):
        # zawor reg (regulation)|r  # NOT USED ANYMORE
        # zawor 0/1     (on/off)|o
        # pompa           (pump)|p
        # co/cwu         (water)|w
        # piec          (heater)|h
        # cyrk     (circulation)|c
        # ---rchwpo (8 bit)
        order = ["circulation", "heater", "co_cwu", "pump", "solar"]
        value = ""
        for relay in order:
            value += str(int(self.relays[relay]))
        v = int(value, 2)
        if self.actuators_value != v:
            self.log.debug("Relays are set to following states: {}".format(self.relays))
            self.actuators_value = v
            self.mqtt_publish("actuators", v)

    # Settings parser #
    def set_setting(self, mqtt_topic, payload):
        if self.get_standalone():
            return
        splitted = mqtt_topic.split('/')
        if len(splitted) not in (4, 5):
            raise ValueError("Wrong mqtt topic message: " + mqtt_topic)
        major_section = splitted[1]
        minor_section = splitted[3]
        try:
            flow_setting = splitted[4]
            time_section = flow_setting
        except IndexError:
            flow_setting = None
            time_section = None
        if major_section == "circulate":
            major_section = "circulation"
            value = int(payload)
            if minor_section == "interval":
                value = int(payload) * 60
        elif major_section == "tank" or major_section == "solar":
            value = float(payload)
        elif major_section == "heater":
            if minor_section == "schedule":
                if time_section in ["on", "off"]:
                    h = int(payload[-4:-2])
                    m = int(payload[-2:])
                    if not 0 <= h < 24:
                        h = 0
                    if not 0 <= m < 60:
                        m = 0
                    value = (h, m)
                else:
                    value = str(payload)
            elif minor_section == "heating_on":
                minor_section = "forced_heating"
                value = bool(int(payload))
            else:
                value = float(payload)
        else:
            try:
                value = float(payload)
            except ValueError as e:
                self.log.exception(e)
                return

        if minor_section == "schedule" and time_section is not None:
            self.log.info("Setting '{}/{}/{}' is changed to {}".format(major_section, minor_section, time_section, value))
            self.settings[major_section][minor_section] = value
        elif len(splitted) == 4:
            self.log.info("Setting '{}/{}' is changed to {}".format(major_section, minor_section, value))
            self.settings[major_section][minor_section] = value
        elif len(splitted) == 5:
            self.log.info("Setting '{}/{}/{}' is changed to {}".format(major_section, minor_section, flow_setting, value))
            self.settings[major_section][minor_section][flow_setting] = value

    # MQTT SECTION #
    def mqtt_publish(self, instance, value):
        try:
            publish.single(self.mqtt_topics_map[instance], value, hostname=MQTT_BROKER)
            self.log.info("Changed '{}' to value of '{}' on mqtt topic '{}'".format(instance,
                                                                                    value,
                                                                                    self.mqtt_topics_map[instance]))
        except socket.error:
            self.log.critical("Couldn't send message to MQTT broker at {}".format(MQTT_BROKER))

    def mqtt_on_connect(self, client, userdata, flags, rc):
        client.subscribe("solarControl/+/settings/#")
        client.subscribe("room/1/temp_real")

    def mqtt_on_message(self, client, userdata, msg):
        topic = str(msg.topic)
        self.log.debug("Got new mqtt message on topic '{}' with value of '{}'".format(topic, msg.payload))
        if topic == "room/1/temp_real":
            if self.external_room_temperature_source:
                self.log.info("Room temperature set to {} from external source".format(float(msg.payload)))
                self.temperatures['inside'] = float(msg.payload)
        else:
            self.set_setting(topic, msg.payload)

    # PROGRAM #
    def calculate_flow(self):
        # Flow function:
        # ^ [flow]                        | s_min, ΔT <= T_min
        # |                    flow(ΔT) = | A * ΔT + B, A = (s_max - s_min) / (T_max - T_min), B = s_min - T_min * A
        # |       -----------             | s_max, ΔT >= T_max
        # |      /
        # |     /
        # |____/
        # |                  [ΔT]
        # +------------------->
        duty_min = self.settings['solar']['flow']['s_min']
        duty_max = self.settings['solar']['flow']['s_max']
        temp_min = self.settings['solar']['flow']['t_min']
        temp_max = self.settings['solar']['flow']['t_max']
        T1 = self.temperatures['solar_up']
        T2 = self.temperatures['solar_in']
        T3 = self.temperatures['solar_out']
        # delta = (T1 + T2) / 2 - T3
        delta = T2 - T3
        if delta <= temp_min:
            return duty_min
        elif delta >= temp_max:
            return duty_max
        else:
            # flow(ΔT) = A * ΔT + B
            a = (duty_max - duty_min) / (temp_max - temp_min)
            b = duty_min - temp_min * a
            return a * delta + b

    def is_schedule(self):
        if self.settings['heater']['forced_heating']:
            return True
        curr = time.strftime("%H,%M").split(',')
        curr_in_min = int(curr[0]) * 60 + int(curr[1])
        on = self.settings['heater']['on'][0] * 60 + self.settings['heater']['on'][1]
        off = self.settings['heater']['off'][0] * 60 + self.settings['heater']['off'][1]
        if on <= curr_in_min < off:
            return True
        else:
            return False

    def run_circulation(self):
        last_run = self.circulation_last_run
        interval = self.settings['circulation']['interval']
        time_on = self.settings['circulation']['time_on']
        curr_time = time.time()
        if curr_time > last_run + interval and self.get_circulation():
            self.log.warning("Detected water consumption. Forcing additional circulation.")
            self.circulation_last_run = curr_time
            self.set_relay("circulation", True)
        elif curr_time > last_run + time_on:
            self.set_relay("circulation", False)

    def run_solar(self):
        # TODO: Create PID control loop
        T1 = self.temperatures['solar_up']
        T2 = self.temperatures['solar_in']
        T3 = self.temperatures['solar_out']
        T8 = self.temperatures['tank_up']
        # delta = T2 - T3
        if T1 >= T2: # Experimental starting conditions
            delta = (T1 + T2) / 2 - T3
        else:
            delta = T1 - T3
        if T1 < self.settings['solar']['critical'] and \
           T3 < T1 and \
           T8 <= self.settings['tank']['solar_max']:
            if delta >= self.settings['solar']['off']:
                if T1 - T3 > self.settings['solar']['on']:
                    self.log.warning("Detected optimal solar conditions. Harvesting.")
                    self.set_relay("solar", True)
                    self.set_relay("pump", True)
                flow = self.calculate_flow()
                self.set_flow(flow)
                self.reduced_exchange_time = time.time() + REDUCED_EXCHANGE_TIME
            elif time.time() < self.reduced_exchange_time:
                # Prolong heat exchange for better efficiency
                self.log.debug("Solar: delta T(T2 - T3)[{}] is low, reduced heat exchange".format(delta))
                self.set_flow(self.settings['solar']['flow']['s_min'])
            else:
                self.log.debug("Solar: delta T(T2 - T3)[{}] is too low, switching off".format(delta))
                self.set_relay("solar", False)
                self.set_relay("pump", False)
                self.set_flow(0)
        else:
            if T1 >= self.settings['solar']['critical']:
                self.log.debug("Solar: Critical temperature reached")
            if T3 >= T1:
                self.log.debug("Solar: Heat escape prevention (T_out[{}] >= T_solar[{}])".format(T3, T1))
            if T8 > self.settings['tank']['solar_max']:
                self.log.debug("Solar: Tank is filled with hot water")
            self.set_relay("solar", False)
            self.set_relay("pump", False)
            self.set_flow(0)

    def run_dhw(self):
        T8 = self.temperatures['tank_up']
        if T8 < self.settings['tank']['heater_min']:
            self.log.warning("Heater: Heating water")
            self.set_relay("co_cwu", True)
            self.set_relay("heater", True)
            return True
        elif T8 < self.settings['tank']['heater_max'] and self.relays["co_cwu"]:
            return True
        return False

    def run_home_heat(self):
        T9 = self.temperatures['inside']
        self.set_relay("co_cwu", False)
        if T9 >= self.settings['heater']['expected'] + self.settings['heater']['hysteresis'] / 2:
            self.log.debug("Heater: Room temperature of {} achieved".format(self.settings['heater']['expected']))
            self.set_relay("heater", False)
        elif T9 < self.settings['heater']['expected'] - self.settings['heater']['hysteresis'] / 2:
            self.log.debug("Heater: Room temperature ({}) is too low. Heating.".format(T9))
            self.set_relay("heater", True)

    def run_heater(self):
        if not self.temperatures['heater_out'] < self.settings['heater']['critical']:
            self.set_relay("heater", False)
        elif self.temperatures['heater_out'] < self.settings['heater']['critical'] - 4:
            if not self.is_schedule():
                self.set_relay("heater", False)
                self.set_relay("co_cwu", False)
            elif not self.run_dhw():
                self.run_home_heat()

    def run_once(self):
        if not self.get_manual():
            if self.get_standalone():
                self.external_room_temperature_source = False
                self.settings['heater']['on'] = (6, 30)
                self.settings['heater']['off'] = (22, 0)
                self.settings['heater']['expected'] = 21
            else:
                self.external_room_temperature_source = True
            self.update_sensors()
            self.run_circulation()
            self.run_solar()
            self.run_heater()
        else:
            self.log.critical("You are in manual mode for next 10s")
            time.sleep(10)

    def run(self, interval=1):
        while True:
            try:
                self.run_once()
            except ConnectionError:
                self.log.error("Problems with connecting to EVOK. Retrying in {}".format(interval))
            except Exception as e:
                self.log.exception(e)
            finally:
                time.sleep(interval)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    # logging.basicConfig(level=logging.DEBUG)
    logging.getLogger("urllib3").setLevel(logging.WARNING)
    c = Controller()
    mqttc = mqtt.Client()
    mqttc.on_connect = c.mqtt_on_connect
    mqttc.on_message = c.mqtt_on_message
    # Failover mode (when MQTT Broker is not available at start time):
    connected = False
    c.standalone = True
    while not connected:
        try:
            mqttc.connect(MQTT_BROKER)
            connected = True
            c.standalone = False
        except socket.error:
            c.run_once()
            time.sleep(0.5)

    # Normal operation
    mqttc.loop_start()
    c.run(1)
