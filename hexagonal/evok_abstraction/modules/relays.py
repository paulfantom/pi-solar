#!/usr/bin/python
# -*- coding: utf-8 -*-

import logging
import requests
import paho.mqtt.client as mqtt
import socket


class Relay(object):
    def __init__(self, pin, evok_api_endpoint):
        self._state = None
        self.api = evok_api_endpoint

    def set_state(self, state):
        if self._state == state:
            return
        data = {"value": str(int(self._state))}
        while True:
            try:
                r = requests.post(self.evok_api_endpoint, json=data)
                if r.status_code == 200:
                    break
            except socket.error:
                return False
        self._state = state
        return True


class RelayController(object):
    def __init__(self, relays_no=8, mqtt_broker="localhost", evok_api="http://localhost:8080"):
        self.log = logging.getLogger("Relays")
        self.relays = []
        for i in range(0, relays_no):
            api_endpoint = evok_api + "/json/relay/" + str(i + 1)
            self.relays.append(Relay(i, api_endpoint))
        mqttc = mqtt.Client()
        mqttc.on_connect = self.on_connect
        mqttc.on_message = self.on_message
        mqttc.connect(mqtt_broker)
        mqttc.loop_start()
        self.log.info("Relays initialized")

    def set_relay(self, pin, state):
        if self.relays[pin].set_state(state):
            self.log.debug("Setting relay {} to {}".format(pin, state))
        else:
            self.log.critical("Setting relay {} state was unsuccessful".format(pin))

    def on_connect(self, client, userdata, flags, rc):
        client.subscribe("unipi/relay/+")

    def on_message(self, client, userdata, msg):
        self.log.info("MQTT message received: topic '{}', value: '{}'".format(msg.topic, msg.payload))
        relay = str(msg.topic).split('/')[-1]
        state = bool(int(msg.payload))
        self.set_relay(relay, state)


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    c = RelayController(8, "192.168.2.2", "http://localhost:8080")
    while True:
        pass
