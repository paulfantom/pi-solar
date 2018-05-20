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
        self.log.info("Analog inputs initialized")

    def get_input(self, pin):
        try:
            evok_api_endpoint = self.evok_api + "/json/analoginput/" + str(pin)
            r = requests.get(evok_api_endpoint)
            if r.status_code == 200:
                # Return percent of maximum value, since EVOK doesn't return real voltage
                return float(r.json()['data']['value']) * 10
        except socket.error:
            return -99

    def update(self):
        for i in range(len(self.values)):
            value = self.get_input(i + 1)
            # if value != self.values[i]:
            # Do not update on very low (<0.5%) variations of detected value
            if self.values[i] - 0.5 < value < self.values[i] + 0.5:
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
