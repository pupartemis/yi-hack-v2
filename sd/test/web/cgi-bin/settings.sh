#!/bin/sh

CONFIG=/sdcard/test/yi-hack-v2.cfg
LED=/sdcard/test/v2/scripts/led.sh

if [ "${REQUEST_METHOD:-GET}" != "POST" ]; then
   printf "Status: 405 Method Not Allowed\r\n"
   printf "Allow: POST\r\n"
   printf "Content-Type: text/plain\r\n\r\n"
   printf "Settings require POST\n"
   exit 1
fi

set_value() {
   key=$1
   value=$2
   case "$key:$value" in
      YI_HACK_TELNET_SERVER:YES|YI_HACK_TELNET_SERVER:NO|\
      YI_HACK_FTP_SERVER:YES|YI_HACK_FTP_SERVER:NO|\
      YI_HACK_HTTP_SERVER:YES|YI_HACK_HTTP_SERVER:NO|\
      YI_HACK_RTSP_SERVER:YES|YI_HACK_RTSP_SERVER:NO) ;;
      *) return 1 ;;
   esac
   if grep "^${key}=" "$CONFIG" >/dev/null 2>&1; then
      sed "s/^${key}=.*/${key}=${value}/" "$CONFIG" > /tmp/yi-hack-v2.cfg.new &&
      mv /tmp/yi-hack-v2.cfg.new "$CONFIG"
   else
      printf "\n%s=%s\n" "$key" "$value" >> "$CONFIG"
   fi
}

stop_named() {
   name=$1
   ps 2>/dev/null | grep "$name" | grep -v grep | awk '{print $1}' |
   while read pid; do
      [ -n "$pid" ] && kill "$pid" 2>/dev/null
   done
}

start_ftp() {
   if ! ps 2>/dev/null | grep 'tcpsvd.*ftpd' | grep -v grep >/dev/null 2>&1 &&
      [ -f /sdcard/test/v2/bin/tcpsvd ]; then
      /sdcard/test/v2/bin/tcpsvd -vE 0.0.0.0 21 ftpd -w / &
   fi
}

start_telnet() {
   if ! ps 2>/dev/null | grep 'telnetd' | grep -v grep >/dev/null 2>&1; then
      telnetd -l /bin/sh &
   fi
}

body=`cat`
for item in `printf '%s' "$body" | tr '&' ' '`; do
   key=`printf '%s' "$item" | cut -d= -f1`
   value=`printf '%s' "$item" | cut -d= -f2`
   case "$key" in
      telnet) set_value YI_HACK_TELNET_SERVER "$value" || exit 2 ;;
      ftp) set_value YI_HACK_FTP_SERVER "$value" || exit 2 ;;
      http) set_value YI_HACK_HTTP_SERVER "$value" || exit 2 ;;
      rtsp) set_value YI_HACK_RTSP_SERVER "$value" || exit 2 ;;
      led_red|led_green|led_blue)
         case "$key" in
            led_red) color=red ;;
            led_green) color=green ;;
            led_blue) color=blue ;;
         esac
         case "$value" in
            off|on|flash) "$LED" "$color" "$value" || exit 2 ;;
            *) exit 2 ;;
         esac
         ;;
      *) exit 2 ;;
   esac
done

grep '^YI_HACK_TELNET_SERVER=YES$' "$CONFIG" >/dev/null 2>&1 && start_telnet ||
   stop_named telnetd
grep '^YI_HACK_FTP_SERVER=YES$' "$CONFIG" >/dev/null 2>&1 && start_ftp ||
   stop_named 'tcpsvd.*ftpd'

printf "Content-Type: text/plain\r\n"
printf "Cache-Control: no-store\r\n\r\n"
printf "Settings saved.\n"
printf "RTSP and HTTP startup changes take effect after reboot.\n"
