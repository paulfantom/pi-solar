#!/usr/bin/python
# -*- coding: utf-8 -*-

import logging
import requests
import paho.mqtt.client as mqtt
import socket


class AnalogOutput(object):
    def __init__(self, mqtt_broker="localhost", evok_api="http://localhost:8080"):
        self.log = logging.getLogger("AnalogOutput")
        self._value = 0
        self.evok_endpoint = evok_api + "/json/ao/1"
        mqttc = mqtt.Client()
        mqttc.on_connect = self.on_connect
        mqttc.on_message = self.on_message
        mqttc.connect(mqtt_broker)
        mqttc.loop_start()
        self.log.info("Output initialized")

    def set_output(self, value):
        if self._value == value:
            self.log.debug("Previous value same as desired")
            return
        data = {"value": float(value) / 10}
        while True:
            try:
                r = requests.post(self.evok_endpoint, json=data)
                if r.status_code == 200:
                    self.log.debug("Setting analog output to {}%".format(value))
                    self._value = value
                    break
            except socket.error:
                self.log.critical("Setting analog output to {}% was unsuccessful".format(value))

    def on_connect(self, client, userdata, flags, rc):
        client.subscribe("unipi/ao")

    def on_message(self, client, userdata, msg):
        self.log.info("MQTT message received: topic '{}', value: '{}'".format(msg.topic, msg.payload))
        value = int(msg.payload)
        if value > 100 or value < 0:
            self.log.warning("Received invalid value of {}".format(value))
        self.set_output(value)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    c = AnalogOutput("192.168.2.2", "http://localhost:8080")
    while True:
        pass
