# SOLAR

## Algorithm

```
if (T1 < T_crit) and (T1 < T3) and (T8 <= T_tank_solar):
    A = (T1 + T2) / 2 - T3
    if A >= T_off:
    	if T1 - T3 > T_on:
    	    start_solar()
        set_flow()
        update_reduced_exchange_time()
    elif curr_time < reduced_exchange_time:
        set_flow(MIN)
    else:
        stop_solar()
else:
    stop_solar()
```

Flow is set according to function:

```
           | s_min, ΔT <= T_min
flow(ΔT) = | A * ΔT + B, A = (s_max - s_min) / (T_max - T_min), B = s_min - T_min * A
           | s_max, ΔT >= T_max

 ^ [flow]
 |
 |       -----------
 |      /
 |     /
 |____/
 |                  [ΔT]
 +------------------->
```

## Glossary

All values are temperarures unless otherwise stated

### Sensors

T1 - Solar
T2 - Tank supply from solar (measured in tank)
T3 - Tank return pipe to solar (measured in tank)
T8 - Tank temperature (measured near the top of the tank)

### Setpoints

T_crit - Critical, glycol boiling temperaature
T_tank_solar - Maximum water temperature
T_on - Minimum difference at which solar circuit should start working
T_off - Difference at which solar circuit should stop working
reduced_exchange_time - algorithm runs solar circuit with lowest possible flow for set amount of time after normally it should be OFF. This allows to accumulate more hot water.

### Flow calculation setpoints

s_min - minimal duty cycle (flow)
s_max - maximum duty cycle (flow)
T_min - minimum temperature (usually set to T_off)
T_max - maximum temperature (usually set to T_on)

# CIRCULATION

## Algorithm

```
if (curr_time > last_run + interval) and water_consumption_detection():
    last_run = curr_time
    start_circulation()
elif: curr_time > last_run + time_on:
    stop_circulation()
```

## Glossary

### Sensors

Only one sensor (digital input) is needed, it's value is taken from `water_consumption_detection()`

### Setpoints

interval - minimum time interval between each circulation start (must be longer than *time_on*)
time_on - how long to force water circulation

# HEATER

## Algorithm

```
if T5 < T_crit:
    stop_burner()
elif T5 < T_crit - 4.0:     # 4.0 degrees are set so there won't be any short start/stop cycles
    if not is_schedule():
        stop_burner()
        actuator_to_home_heat()
    else:
      if T8 < T_tank_min:
          actuator_to_dhw()
          start_burner()
      elif: T8 >= T_tank_max:
          actuator_to_home_heat()
          if T9 >= T_room + T_hyst / 2:
              stop_burner()
          elif T9 < T_room - T_hyst / 2:
              start_burner()
```

## Glossary

All values are temperatures unless stated otherwise

### Sensors

T5 - Water supply from heater
T8 - Tank temperature (measured near the top of the tank)
T9 - Room temperature

### Setpoints

T_crit - Critical water temperature (we don't want to boil anything)
T_tank_min - Comfortable minimum to take shower
T_tank_max - Maximum water temperature when when using heater
T_room - Expected room temperature
T_hyst - Heating hysteresis (around 1 degree)

### `is_schedule()`

Heating is currently set according to time schedule. In the future it can be expanded to detect user presence etc.
