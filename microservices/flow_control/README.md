# Solar circuit flow controller

## Purpose

Application controls the flow of medium in solar circuit.

## Devices used

- GPIO18 PWM

## Communication

- Analog output (0-10V) limited by blue trimmer on unipi board
- External voltage MUST be applied to AOV connector (+12V from board?)

## MQTT Data model

{ "value": "20.2" }
