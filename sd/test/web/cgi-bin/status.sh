#!/bin/sh

printf "Content-Type: text/plain\r\n"
printf "Cache-Control: no-store\r\n\r\n"
printf "Yi Hack v2 status\n"
printf "Date: %s\n" "`date`"
printf "Uptime: "
cat /proc/uptime 2>/dev/null || printf "unavailable\n"
printf "Load: "
cat /proc/loadavg 2>/dev/null || printf "unavailable\n"
printf "\nMemory:\n"
grep -E '^(MemTotal|MemFree|Buffers|Cached):' /proc/meminfo 2>/dev/null
printf "\nNetwork:\n"
ifconfig 2>/dev/null
printf "\nServices:\n"
for service in "rtsp_server:RTSP" "httpd:HTTP"; do
   process="${service%%:*}"
   label="${service#*:}"
   if ps 2>/dev/null | grep "$process" | grep -v grep >/dev/null 2>&1; then
      printf "%s: running\n" "$label"
   else
      printf "%s: stopped\n" "$label"
   fi
done
printf "\nVideo processes:\n"
ps 2>/dev/null | grep -E 'test_encode|test_tuning|rtsp_server' | grep -v grep
