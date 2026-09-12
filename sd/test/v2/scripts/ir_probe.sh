#!/bin/sh

# Read-only inventory of the H21 day/night and IR-related interfaces.

echo "Yi Hack v2 H21 IR/night-vision probe"
echo "=== stock configuration ==="
for file in /mnt/cfg/ipc_config/ipc.config /sdcard/test/yi-hack-v2.cfg; do
   if [ -f "$file" ]; then
      echo "--- $file ---"
      grep -E '^(lightmode|isdaymode|daynightMode|YI_HACK_NIGHT_MODE)=' "$file" ||
         echo "no day/night setting found"
   fi
done

echo "=== GPIO state (read-only) ==="
for gpio in 33 38 46; do
   path="/sys/class/gpio/gpio${gpio}"
   if [ -f "$path/value" ]; then
      printf "gpio%s direction=" "$gpio"
      cat "$path/direction"
      printf "gpio%s value=" "$gpio"
      cat "$path/value"
   else
      echo "gpio${gpio} is not exported"
   fi
done

echo "=== candidate device interfaces ==="
for path in /dev/amb_iris /sys/bus/platform/devices/e8006000.ir \
            /sys/class/backlight/*; do
   if [ -e "$path" ]; then
      echo "$path"
   fi
done

echo "=== stock helper presence ==="
for path in /usr/local/bin/gpio_check.sh /usr/local/bin/set_audio.sh \
            /home/web/ipc; do
   if [ -e "$path" ]; then
      echo "$path"
   fi
done

echo "No hardware state was changed."
