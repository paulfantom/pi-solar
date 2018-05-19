#!/usr/bin/python
# -*- coding: utf-8 -*-

import logging
import requests
import paho.mqtt.client as mqtt
import paho.mqtt.publish as publish
import socket


# TODO
# - make `update` non-blocking

class Inputs(object):
    def __init__(self, inputs_no=12, mqtt_broker="localhost", evok_api="http://localhost:8080"):
        self.log = logging.getLogger("Inputs")
        self.mqtt_broker = mqtt_broker
        self.states = [False for i in range(0,inputs_no)]
        self.evok_api = evok_api
        self.log.info("Inputs initialized")

    def get_input(self, pin):
        try:
            evok_api_endpoint = self.evok_api + "/json/input/" + str(pin)
            r = requests.get(evok_api_endpoint)
            if r.status_code == 200:
                return bool(r.json()['data']['value'])
        except socket.error:
            pass

    def update(self):
        for i in range(len(self.states)):
            state = self.get_input(i)
            if state != self.states[i]:
                topic = "unipi/input/" + str(i)
                publish.single(topic, state, hostname=self.mqtt_broker)
                self.states[i] = state
                self.log.info("Changed input '{}' to '{}'".format(i, state)


if __name__ == "__main__":
    import time
    logging.basicConfig(level=logging.DEBUG)
    inp = Inputs(12, "192.168.2.2", "http://localhost:8080")
    while True:
        inp.update()
        time.sleep(0.5)
