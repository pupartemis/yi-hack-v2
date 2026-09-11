#!/bin/sh

# When script if started without any arguments, we assume it is called from main.sh
# We restart if with useless param and redirect output to log file on sdcard

if [ $# -eq 0 ]; then
   export YI_HACK_LOGS=/sdcard/test/logs
   mkdir -p "$YI_HACK_LOGS"
   $0 nop > "$YI_HACK_LOGS/factory_test.log" 2>&1
   exit $?
fi

# Export the supported variables from yi-hack-v2.cfg.
if [ -f /sdcard/test/yi-hack-v2.cfg ]; then
   echo "### Export variables ... ###"
   while read assignment; do
      case "$assignment" in
         ''|'#'*) continue ;;
         YI_HACK_*=*)
            key=${assignment%%=*}
            value=${assignment#*=}
            case "$key" in
               YI_HACK_STARTUP_MODE|YI_HACK_LANGUAGE|YI_HACK_TELNET_SERVER|\
               YI_HACK_FTP_SERVER|YI_HACK_HTTP_SERVER|YI_HACK_HTTP_PORT|\
               YI_HACK_TIME_TIMEZONE|YI_HACK_TIME_FORMAT|YI_HACK_PROXY|\
               YI_HACK_NATIVE_TRACES)
                  export "$key=$value" ;;
               *) echo "Ignoring unsupported configuration key: $key" ;;
            esac
            ;;
         *) echo "Ignoring malformed configuration line: $assignment" ;;
      esac
   done < /sdcard/test/yi-hack-v2.cfg
   echo
fi

# Telnet server activation (no authentication required)
if [ "$YI_HACK_TELNET_SERVER" = "YES" ]; then
   echo "### Activating telnet server ... ###"
   telnetd -l /bin/sh &
   echo
fi

# Launch ftp server
if [ "$YI_HACK_FTP_SERVER" = "YES" ]; then
   if [ -f /sdcard/test/v2/bin/tcpsvd ]; then
      echo "### Activating FTP server ... ###"
      /sdcard/test/v2/bin/tcpsvd -vE 0.0.0.0 21 ftpd -w / &
      sleep 1s
      echo
   fi
fi

# Main hack
rm -f "$YI_HACK_NATIVE_TRACES"
if [ -f /sdcard/test/v2/bin/libyihackv2.so ]; then
   export LD_PRELOAD=/sdcard/test/v2/bin/libyihackv2.so
fi

# Mount config
mkdir -p /mnt/cfg
mount -t jffs2 /dev/mtdblock8 /mnt/cfg

# Launch the optional status and snapshot web interface before the camera
# startup script, because the modified encoder startup may stay in foreground.
if [ "$YI_HACK_HTTP_SERVER" = "YES" ]; then
   if [ -x /sdcard/test/web/start_http.sh ]; then
      /sdcard/test/web/start_http.sh
   else
      echo "Error: web server launcher is not available"
   fi
fi

# Launch expected startup
if [ "$YI_HACK_STARTUP_MODE" = "MODIFIED" ]; then
   if [ -f /sdcard/test/v2/scripts/startup_modified.sh ]; then
      /sdcard/test/v2/scripts/startup_modified.sh
   fi
else
   if [ -f /sdcard/test/v2/scripts/startup_official.sh ]; then
      /sdcard/test/v2/scripts/startup_official.sh
   fi
fi
