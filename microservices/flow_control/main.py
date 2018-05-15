# -*- coding: utf-8 -*-

import sys
import yaml
import json
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO

PIN = 18
NAME = "flow_controller"


def config(path="./config.yaml"):
    with open(path, 'r') as stream:
        try:
            data = yaml.load(stream)
        except yaml.YAMLError as exc:
            print(exc)
            sys.exit(127)
    try:
        broker = data["mqtt_broker"]
        uri = data["mqtt_uri"]
    except Exception as exc:
        print(exc)
        sys.exit(128)

    return broker, uri


def action(client, userdata, message):
    data = json.loads(message.payload)
    value = float(data["value"])
    if value < 0:
        value = 0.0
    elif value > 100:
        value = 100.0
    pwm = GPIO.PWM(PIN, 400)
    pwm.ChangeDutyCycle(value)


if __name__ == "__main__":
    # initialize
    mqtt_broker, uri = config()

    # connect to MQTT
    client = mqtt.Client(NAME)
    client.on_message = action
    client.connect(mqtt_broker)
    client.subscribe(uri, 1)

    # setup GPIO
    GPIO.setwarnings(False)
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(PIN, GPIO.OUT)
    pwm = GPIO.PWM(PIN, 400)
    pwm.start(0)

    # run until disconnected
    client.loop()
