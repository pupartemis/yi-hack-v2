#!/bin/sh

echo "### MODIFIED startup ... ###"

LOG=/sdcard/test/logs/modified_startup.log
mkdir -p /sdcard/test/logs
exec >> "$LOG" 2>&1
set -x
echo "=== modified startup: `date` ==="

# Copy wpa_supplicant.conf
if [ -f /sdcard/test/wpa_supplicant.conf ]; then
   mkdir -p /tmp/config
   cp -f /sdcard/test/wpa_supplicant.conf /tmp/config/wpa_supplicant.conf
else
   echo "Error: /sdcard/test/wpa_supplicant.conf is not available"
   exit
fi

# Wi-Fi. Reuse the stock initialization path because it handles the USB
# gadget state, calibration/MAC setup, and module loading for this camera.
/bak/usr/local/bin/usb_wifi.sh
WIFI_STATUS=$?
echo "usb_wifi status=$WIFI_STATUS"
ifconfig -a
if [ "$WIFI_STATUS" = 1 ]; then
   echo "Error: USB Wi-Fi initialization detected a connected USB host"
   exit 1
fi

# Match the stock startup workaround before associating with the access point.
/usr/local/bin/amba_debug -g 51 -d 0x1

# Init led
/sdcard/test/v2/scripts/led.sh red init     
/sdcard/test/v2/scripts/led.sh green init   
/sdcard/test/v2/scripts/led.sh blue init  

# Make blue led flash during wifi connection
/sdcard/test/v2/scripts/led.sh red off
/sdcard/test/v2/scripts/led.sh green off
/sdcard/test/v2/scripts/led.sh blue flash

# Connect to wifi
wifi_auto.sh
WIFI_STATUS=$?
echo "wifi_auto status=$WIFI_STATUS"
ifconfig -a
echo "--- ip.conf ---"
cat /tmp/config/ip.conf 2>&1
echo "--- wpa_supplicant.log ---"
tail -100 /tmp/wpa_supplicant.log 2>&1

if [ "$WIFI_STATUS" != 0 ]; then
   # Turn off blue led and turn on red led, wifi is KO
   /sdcard/test/v2/scripts/led.sh blue off
   /sdcard/test/v2/scripts/led.sh red on
   exit 1
fi

# Turn on blue led, wifi is OK
/sdcard/test/v2/scripts/led.sh blue on

# MN34220PL 1/3 -Inch, 1944x1213, 2.4-Megapixel CMOS Digital Image Sensor
modprobe mn34220pl bus_addr=0x36

/usr/local/bin/init.sh --na

/usr/local/bin/test_tuning -a 0 &
/usr/local/bin/test_encode -A -i 1920x1080 --bitrate 1500000 -f 25 --enc-mode 4 --hdr-expo 2 --hdr-mode 1 -J --btype off  -K --btype off -X --bmaxsize 1920x1080 --bsize 1920x1080 --smaxsize 1920x1080 -Y --bmaxsize 640x360 --bsize 640x360 -B -m 640x360 --smaxsize 640x360
if [ "${YI_HACK_RTSP_SERVER:-YES}" = "YES" ]; then
   /usr/local/bin/rtsp_server &
fi
if [ "${YI_HACK_AUDIO_RTSP_SERVER:-NO}" = "YES" ]; then
   if [ -x /sdcard/test/v2/audio/audio_rtsp_server ]; then
      /sdcard/test/v2/audio/audio_rtsp_server &
   else
      echo "Audio RTSP server is enabled but not installed"
   fi
fi
/usr/local/bin/test_encode -A -h 1080p -e --bitrate 1500000
/usr/local/bin/test_encode -B -e
