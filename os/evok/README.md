# Steps taken

0. Added my SSH key
1. Packages installed:
  - tree
  - vim
  - lsof
  - iotop
  - ntp
  - i2c-tools
2. Changed default editor to VIM:
```
update-alternatives --config editor
```
3. Passwordless sudo
4. Added aliases to local user `.bashrc`:
  - `alias ro="sudo mount -o remount,ro /"`
  - `alias rw="sudo mount -o remount,rw /"`
5. Added script to automatically remount `/` into readonly with PAM (via https://unix.stackexchange.com/questions/136548/force-command-to-be-run-on-logout-or-disconnect):
```
#!/bin/sh
if [ "$PAM_TYPE" = "close_session" ]; then
	mount -o remount,ro /
fi
```
6. Edited /etc/fstab to include second drive (writable) and readonly main drive. Manually mounted second drive and bind-mounts.
```
proc            /proc           proc    defaults          0       0
PARTUUID=bce30dfe-01  /boot           vfat    defaults          0       2
PARTUUID=bce30dfe-02  /               ext4    defaults,noatime,ro  0       1

tmpfs   /tmp         tmpfs   nodev,nosuid,size=128M          0  0
/dev/sda1       /srv    xfs     defaults,nofail,noatime 0       0

/srv/log        /var/log        none     defaults,bind 0 0
/srv/docker     /var/lib/docker none     defaults,bind 0 0
```
7. Downloaded evok (v2.0.5b) to /opt/evok_source and proceded with installation from https://github.com/UniPiTechnology/evok#installation-process-for-the-20-evok-version
  - Web interface port: 80
  - API port: 8080
  - No wifi
  - No reboot
8. Downloaded docker (`curl get.docker.com | bash`)
9. Changed default docker log driver to `journald` in `/etc/docker/daemon.json` (new file)
10. Executed `raspi-config` to change:
  - timezone
  - hostname
11. Added `gpu_mem=16` and `dtparam=pwr_led_trigger=heartbeat` to /boot/config.txt
12. Changed /boot/cmdline.txt to:
```
dwc_otg.lpm_enable=0 console=serial0,115200 console=tty1 root=PARTUUID=bce30dfe-02 rootfstype=ext4 cgroup_enable=cpuset cgroup_enable=memory swapaccount=1 elevator=deadline fsck.repair=no rootwait fsck.mode=skip noswap ro
```
13. Changed `/etc/ntp.conf` driftfile option to `/tmp/ntp.driftfile`
14. Added cron command `hwclock -s` to sync hardware clock once per month
15. Logged in as **root** and changed user *pi* to *paulfantom*:
```
usermod -l pi paulfantom
usermod -d /home/paulfantom paulfantom
mv /home/pi /home/paulfantom
```
16. Packages purged:
  - rsyslog
  - fake-hwclock
17. Installed rsyslog alternative (without recommended packages, no exim!):
  - busybox-syslogd
18. Rebooted, checked everything, powered off, resized to less than 4GB (/dev/mmcblk0p2 is 3330MB), and backuped with `dd`:
```
sudo dd if=/dev/mmcblk0 bs=64K count=56800 | pv | pbzip2 -c > `date +%F`_evok.img.bz2
```
