#!/usr/bin/python
# -*- coding: utf-8 -*-

import logging
import requests
import paho.mqtt.publish as publish
import socket


# TODO
# - make `update` non-blocking

class DS18B20(object):
    def __init__(self, address="", alias="", mqtt_broker="localhost", evok_api="http://localhost:8080"):
        self.log = logging.getLogger("DS18B20")
        self.mqtt_broker = mqtt_broker
        if alias == "":
            self.alias = address
        else:
            self.alias = alias
        self.value = -99
        self.evok_api_endpoint = evok_api + "/json/sensor/" + address
        self.log.info("DS18B20 ({}) initialized".format(alias))

    def get_value(self):
        try:
            r = requests.get(self.evok_api_endpoint)
            if r.status_code == 200:
                return float(r.json()['data']['value'])
        except socket.error:
            return -99

    def update(self):
        value = self.get_value()
        if value != self.value:
            topic = "unipi/ds18b20/" + self.alias
            publish.single(topic, value, hostname=self.mqtt_broker)
            self.value = value
            self.log.info("Changed ds18b20 '{}' temperature to '{}'".format(self.alias, value))


if __name__ == "__main__":
    import time
    logging.basicConfig(level=logging.DEBUG)
    ds = DS18B20("28E...", "tank", "192.168.2.2", "http://localhost:8080")
    while True:
        ds.update()
        time.sleep(0.5)
