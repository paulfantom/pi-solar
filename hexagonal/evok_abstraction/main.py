#!/usr/bin/python
# -*- coding: utf-8 -*-

import time
from modules import DS18B20, AnalogInputs, AnalogOutput, Inputs, RelayController

EVOK = "http://localhost:8080"
MQTT = "192.168.2.2"
INTERVAL = 1
DS_MAP = {"tank": "28Ekjsdhfa"}

if __name__ == "__main__":
    # Initialize evok interfaces
    ao = AnalogOutput(mqtt_broker=MQTT, evok_api=EVOK)
    ai = AnalogInputs(mqtt_broker=MQTT, evok_api=EVOK)
    inputs = Inputs(mqtt_broker=MQTT, evok_api=EVOK)
    relays = RelayController(mqtt_broker=MQTT, evok_api=EVOK)
    # Initialize temperaature sensors
    sensors = []
    for alias, address in DS_MAP.items():
        sensors.append(DS18B20(address=address,
                               alias=alias,
                               mqtt_broker=MQTT,
                               evok_api=EVOK))

    # Run updates
    while True:
        start = time.time()
        inputs.update()
        ai.update()
        for s in sensors:
            s.update()
        interval = INTERVAL - (time.time() - start)
        if interval > 0:
            time.sleep(interval)
