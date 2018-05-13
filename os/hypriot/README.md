# Preparing OS

Everything is prepared by configuration files and cloud-init. Here are only resources used to prepare it.

## I2C

- https://www.raspberrypi.org/forums/viewtopic.php?t=168787

## RTC + NTP

- https://www.raspberrypi.org/forums/viewtopic.php?t=134082#p940741
- https://github.com/UniPiTechnology/evok/search?utf8=✓&q=rtc&type=
- NTPd package is distributed with hypriotOS

## Root partition as Read-Only

- https://k3a.me/how-to-make-raspberrypi-truly-read-only-reliable-and-trouble-free/
- https://hallard.me/raspberry-pi-read-only/
- https://github.com/hypriot/image-builder-rpi/issues/119

## TODO

1. Change hostname
2. Add aliases `ro` and `rw` for switching `/` partition readonly and writable
3. Add failasfe command in `.bash_logout` to switch `/` partition readonly
