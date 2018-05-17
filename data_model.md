### MQTT data model

| Level | Meaning | Example |
| ----- | ------- | ------- |
| 1 | Function / home location | solar |
| 2 | Functional type | sensor/actuator/setpoints |
| 3 | Name, usually arbitrary object location | wall, ceiling, up... |
| 4 | data, can be in JSON | True |

```yaml
outside:
  sensor:
    temperature: 10
room/first_floor/hallway:
  sensor:
    hmi_alt: 21
solar:
  sensor:
    up: 10
  actuator:
    flow_regulator: 35
    pump: True
    valve: True
  setpoints:
    flow: { duty: [35, 41], temperature: [5.0, 9.0] }
    critical: 90.0
    on: 8.0
    off: 5.0
    max: 65.0
    reduced_time: 30
tank:
  sensor:
    up: 14
    solar_supply: 11
    solar_return: 10
  setpoints:
    max_solar: 65.0
    max_heater: 48.0
    min_heater: 44.0
heater:
  sensor:
    return: 10
    supply: 10
  actuator:
    valve: True
    burner: True
  setpoints:
    temp_critical: 80
    hysteresis: 0.5
    schedule: { minimal_temp: 18, {on: time, off: time, expected: 22} ??? }
circulation:
  sensor:
    detection: False
  actuator:
    pump: False
  setpoints:
    time_on: 60
    interval: 1800
```
