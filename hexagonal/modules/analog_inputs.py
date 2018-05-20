#!/usr/bin/python
# -*- coding: utf-8 -*-

import logging
import requests
import paho.mqtt.publish as publish
import socket


# TODO
# - make `update` non-blocking

class AnalogInputs(object):
    def __init__(self, inputs_no=2, mqtt_broker="localhost", evok_api="http://localhost:8080"):
        self.log = logging.getLogger("AnalogInputs")
        self.mqtt_broker = mqtt_broker
        self.values = [False for i in range(0, inputs_no)]
        self.evok_api = evok_api
        self.log.info("Inputs initialized")

    def get_input(self, pin):
        try:
            evok_api_endpoint = self.evok_api + "/json/analoginput/" + str(pin)
            r = requests.get(evok_api_endpoint)
            if r.status_code == 200:
                return bool(r.json()['data']['value'])
        except socket.error:
            pass

    def update(self):
        for i in range(len(self.values)):
            value = self.get_input(i + 1)
            if value != self.values[i]:
                topic = "unipi/analoginput/" + str(i + 1)
                publish.single(topic, value, hostname=self.mqtt_broker)
                self.values[i] = value
                self.log.info("Changed analog input '{}' to {}%".format(i + 1, value))


if __name__ == "__main__":
    import time
    logging.basicConfig(level=logging.DEBUG)
    ai = AnalogInputs(2, "192.168.2.2", "http://localhost:8080")
    while True:
        ai.update()
        time.sleep(0.5)
