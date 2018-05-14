#!/usr/bin/env python3

import requests
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import time

DEBUG = True
EVOK_API = "http://192.168.2.28:8080"
MQTT_BROKER = "192.168.2.2"
MQTT_PREFIX = "newS"

ADDR_RELAY_CO_CWU = "1"
ADDR_RELAY_SOL = "2"
ADDR_RELAY_PUMP = "3"
ADDR_RELAY_HEAT = "4"
ADDR_RELAY_CIRC = "5"

ADDR_TEMP_HEATER_IN = "28BECABF0600004F"
ADDR_TEMP_HEATER_OUT = "28BECABF0600004F"
ADDR_TEMP_SOLAR_IN = "28BECABF0600004F"
ADDR_TEMP_SOLAR_OUT = "28BECABF0600004F"
ADDR_TEMP_TANK_UP = "28BECABF0600004F"
ADDR_TEMP_INSIDE = "28BECABF0600004F"
ADDR_TEMP_OUTSIDE = "28BECABF0600004F"
ADDR_TEMP_OUTSIDE = "28BECABF0600004F"

ADDR_INPUT_CIRCULATION = "1"
ADDR_INPUT_MANUAL_MODE = "2"

USE_THERMISTOR = True

class Controller():
    temp_sensor_addr_map = {
        "heater_in"  : ADDR_TEMP_HEATER_IN,
        "heater_out" : ADDR_TEMP_HEATER_OUT,
        "solar_in"   : ADDR_TEMP_SOLAR_IN,
        "solar_out"  : ADDR_TEMP_SOLAR_OUT,
        "tank_up"    : ADDR_TEMP_TANK_UP,
        "outside"    : ADDR_TEMP_OUTSIDE,
        "inside"     : ADDR_TEMP_INSIDE
    }
    relays_addr_map = {
        "co_cwu" : ADDR_RELAY_CO_CWU,  # CWU == TRUE
        "solar" : ADDR_RELAY_SOL,
        "pump" : ADDR_RELAY_PUMP,
        "heater" : ADDR_RELAY_HEAT,
        "circulation" : ADDR_RELAY_CIRC
    }
    mqtt_topics_map = {
        "actuators"  : MQTT_PREFIX + "/actuators",
        "circulation": MQTT_PREFIX + "/circulate/pump",
        "heater_in"  : MQTT_PREFIX + "/heater/temp_in",
        "heater_out" : MQTT_PREFIX + "/heater/temp_out",
        "solar_in"   : MQTT_PREFIX + "/solar/temp_in",
        "solar_out"  : MQTT_PREFIX + "/solar/temp_out",
        "solar_up"   : MQTT_PREFIX + "/solar/temp",
        "solar_pump" : MQTT_PREFIX + "/solar/pump",
        "tank_up"    : MQTT_PREFIX + "/tank/temp_up",
        "outside"    : MQTT_PREFIX + "/outside/temp",
        "inside"     : MQTT_PREFIX + "/room/1/temp_current"
    }
    thermistor = USE_THERMISTOR

    def __init__(self, external_room_temp=False):
        self.temperatures = {}
        self.relays = {}
        self.circulation_sensor = False
        self.circulation_last_run = 0
        self.actuators_value = 0 # Backwards compatibility
        self.dual_source = False
        self.external_room_temperature_source = external_room_temp
        self.settings = {
            "circulation": {
                "interval": 3600,
                "time_on": 12
            },
            "tank": {
                "solar_max" : 65.0,
                "heater_max" : 48.0,
                "heater_min" : 44.0
            },
            "heater": {
                "hysteresis": 0.5,
                "critical" : 80.0,
                "expected" : 20.9,
                "schedule" : None,
                "on": (6, 30),
                "off": (22, 0)
            },
            "solar": {
                "critical": 90.00,
                "on" : 8.0,
                "off": 5.0,
                "flow": {
                    "s_min": 30,
                    "s_max": 60,
                    "t_min": 5.0,
                    "t_max": 9.0
                }
            }
        }
        self.manual = False
        for key, _ in self.relays_addr_map.items():
            self.relays[key] = False
        self.update_all()

    def update_all(self):
        self.update_manual_mode()
        self.update_circulation()
        for key, _ in self.temp_sensor_addr_map.items():
            self.update_temperature(key)
        self.update_temperature('solar_up')

    def update_temperature(self, sensor):
        if sensor == "solar_up":
            value = self.get_solar_temperature()
        elif sensor == "inside" and self.external_room_temperature_source:
            return
        else:
            addr = self.temp_sensor_addr_map[sensor]
            r = requests.get(EVOK_API + "/json/sensor/" + addr)
            value = float(r.json()['data']['value'])
            if DEBUG:
                new = input("Provide new value for sensor '{}' or press enter to ack value of '{}'".format(sensor, value))
                if new != "":
                    value = float(new)
        try:
            old = self.temperatures[sensor]
        except KeyError:
            old = None
        if old != value:
            self.temperatures[sensor] = value
            self.mqtt_publish(sensor, value)

    def normalize_thermistor(self, voltage):
        B = 3950 # Thermistor coefficient (in Kelvin)
        T0 = 273.15 + 25  # Room temperature (in Kelvin)
        R0 = 100000 # Thermistor resistance at T0
        VCC = 12 # Voltage applied to resistance bridge
        # All calculations are in Kelvin, returned value is in Celsius
        try:
            resistance = (voltage * R0) / (VCC - voltage)
            inv_T = (1/T0) + (1/B) * log(resistance/R0)
            return 1 / inv_t - 273.15
        except:
            return -99

    def get_solar_temperature(self):
        r = requests.get(EVOK_API + "/json/analoginput/" + str(channel))
        voltage = float(r.json()['data']['value'])
        if self.thermistor:
            return self.normalize_thermistor(voltage)
        else:
            T_min = 0   # 0V
            T_max = 200 # 10V
            try:
                return (T_max - T_min) * voltage / 10 + T_min
            except:
                return -99

    def update_circulation(self):
        r = requests.get(EVOK_API + "/json/input/" + ADDR_INPUT_CIRCULATION)
        self.circulation_sensor = bool(r.json()['data']['value'])

    def update_manual_mode(self):
        r = requests.get(EVOK_API + "/json/input/" + ADDR_INPUT_MANUAL_MODE)
        self.manual = bool(r.json()['data']['value'])

    def set_relay(self, relay, state):
        #if self.relays[relay] == state:
        #    return
        addr = self.relays_addr_map[relay]
        data = {
                 "value": str(int(state))
               }
        for i in range(0,5):
            r = requests.post(EVOK_API + "/json/relay/" + addr, json = data)
            if r.status_code == 200: break
        self.relays[relay] = state
        self.actuators()  # BACKWARDS COMPATIBILITY

    def set_flow(self, value):
        if value < 0:
           value = 0
        if value > 100:
           value = 100
        data = {
                 "value": float(value) / 10
               }
        for i in range(0,5):
            r = requests.post(EVOK_API + "/json/ao/1", json = data)
            if r.status_code == 200: break
        mqtt_publish("solar_pump", value)

    ### HELPER METHODS (unpack old mqtt schema)
    def actuators(self):
        # zawor reg (regulation)|r  # NOT USED ANYMORE
        # zawor 0/1     (on/off)|o 
        # pompa           (pump)|p
        # co/cwu         (water)|w
        # piec          (heater)|h
        # cyrk     (circulation)|c
        # ---rchwpo (8 bit)
        order = [ "circulation", "heater", "co_cwu", "pump", "solar" ]
        value = ""
        for relay in order:
            value += str(int(self.relays[relay]))
        v = int(value, 2)
        if self.actuators_value != v:
            self.actuators_value = v
            self.mqtt_publish("actuators", v)

    ### Settings parser
    def set_setting(self, mqtt_topic, payload):
        splitted = mqtt_topic.split('/')
        if len(splitted) not in (4, 5):
            raise ValueError("Wrong mqtt topic message: " + mqtt_topic)
        major_section = splitted[1]
        minor_section = splitted[3]
        try:
            flow_setting = splitted[4]
        except IndexError:
            pass
        if major_section == "circulate":
            value = int(payload)
            if minor_section == "interval":
                value = int(payload) * 60
        elif major_section == "tank" or major_section == "solar":
            value = float(payload)
        elif major_section == "heater":
            if minor_section in ["on", "off"]:
                h = int(payload[-4:-2])
                m = int(payload[-2:])
                if not 0 <= h < 24: h = 0
                if not 0 <= m < 60: m = 0
                value = (h, m)
            elif minor_section == "schedule":
                value = str(payload)
            else:
                value = float(payload)
        else:
            try:
                value = float(payload)
            except ValueError as e:
                print(e)
                return

        if len(splitted) == 4:
            print("Setting '{}/{}' is changed to {}".format(major_section, minor_section, value))
            self.settings[major_section][minor_section] = value
        elif len(splitted) == 5:
            print("Setting '{}/{}/{}' is changed to {}".format(major_section, minor_section, flow_setting, value))
            self.settings[major_section][minor_section][flow_setting] = value

    ### MQTT SECTION
    def mqtt_publish(self, instance, value):
        print("Changed '{}' to value of '{}' on mqtt topic '{}'".format(instance, value, self.mqtt_topics_map[instance]))
        publish.single(self.mqtt_topics_map[instance], value, hostname=MQTT_BROKER)

    def mqtt_on_connect(self,client, userdata, flags, rc):
        client.subscribe("solarControl/+/settings/#")
        client.subscribe("room/1/temp_real")

    def mqtt_on_message(self,client, userdata, msg):
        topic = str(msg.topic)
        if topic == "room/1/temp_real":
            if self.external_room_temperature_source:
                print("Room temperature set to {} from external source".format(float(msg.payload)))
                self.temperatures['inside'] = float(msg.payload)
        else:
            self.set_setting(topic, msg.payload)

    ### PROGRAM ###
    def calculate_flow(self):
        duty_min = self.settings['solar']['flow']['s_min']
        duty_max = self.settings['solar']['flow']['s_max']
        temp_min = self.settings['solar']['flow']['t_min']
        temp_max = self.settings['solar']['flow']['t_max']
        T2 = self.temperatures['solar_in']
        T3 = self.temperatures['solar_out']
        delta = T2 - T3
        if delta >= temp_max: return duty_max
        # y = ax + b
        a = (duty_max - duty_min) / (temp_max - temp_min)
        b = duty_min - temp_min * a
        return a * delta + b

    def is_schedule(self):
        curr = time.strftime("%H,%M").split(',')
        curr_in_min = curr[0] * 60 + curr[1]
        on = self.settings['heater']['on'][0] * 60 + self.settings['heater']['on'][1]
        off = self.settings['heater']['off'][0] * 60 + self.settings['heater']['off'][1]
        if on <= curr_in_min < off:
            return True
        else:
            return False

    def run_circulation(self):
        state = self.circulation_sensor
        last_run = self.circulation_last_run
        interval = self.settings['circulation']['interval']
        time_on = self.settings['circulation']['time_on']
        curr_time = time.time()
        if curr_time < last_run + interval:
            if state:
                self.set_relay("circulation", True)
        if curr_time > last_run + time_on:
            self.set_relay("circulation", False)
            self.circulation_last_run = curr_time

    def run_solar(self):
        T1 = self.temperatures['solar_up']
        T2 = self.temperatures['solar_in']
        T3 = self.temperatures['solar_out']
        T8 = self.temperatures['tank_up']
        if T1 < self.settings['solar']['critical'] and \
          T3 < T1 and \
          T8 <= self.settings['tank']['solar_max'] and \
          T2 - T3 >= self.settings['solar']['off']:
            if T1 - T3 > self.settings['solar']['on']:
                self.set_relay("solar", True)
                self.set_relay("pump", True)
                flow = self.calculate_flow()
                self.set_flow(flow)
        else:
            self.set_relay("solar", False)
            self.set_relay("pump", False)

    def run_dhw(self):
        T8 = self.temperatures['tank_up']
        if T8 < self.settings['tank']['heater_min']:
            self.set_relay("cwu_co", True)
            self.set_relay("heater", True)
        else:
            if T8 < self.settings['tank']['heater_max']:
                self.set_relay("cwu_co", False)

    def run_home_heat(self):
        T9 = self.temperatures['inside']
        if T9 >= self.settings['heater']['expected'] + self.settings['heater']['hysteresis']/2:
            self.set_relay("heater", False)
        elif T9 <= self.settings['heater']['expected'] - self.settings['heater']['hysteresis']/2:
            self.set_relay("heater", True)

    def run_heater(self):
        if not self.temperatures['heater_out'] < self.settings['heater']['critical'] + 2:
            self.set_relay("heater", False)
        elif self.temperatures['heater_out'] > self.settings['heater']['critical'] - 2:
            if not self.is_schedule:
                self.set_relay("heater", False)
            else:
                self.run_dhw()
                self.run_home_heat()

    def run_once(self):
        if not self.manual:
            self.update_all()
            self.run_circulation()
            self.run_solar()
            self.run_heater()
        else:
            self.update_manual_mode()
            print("You are in manual mode for next 10s")
            time.sleep(10)

    def run(self, interval=1):
        while True:
            try:
                self.run_once()
                time.sleep(interval)
            except Exception as e:
                print(e)


if __name__ == '__main__':
     c = Controller(True)
     mqttc = mqtt.Client()
     mqttc.on_connect = c.mqtt_on_connect
     mqttc.on_message = c.mqtt_on_message
     mqttc.connect(MQTT_BROKER)
     #mqttc.loop_forever()
     mqttc.loop_start()
     c.run(0.5)
