#!/bin/sh

printf "Content-Type: text/plain\r\n\r\n"
printf "Yi Hack v2 status\n"
printf "Date: %s\n" "`date`"
printf "Uptime: "
cat /proc/uptime 2>/dev/null || printf "unavailable\n"
printf "Network:\n"
ifconfig 2>/dev/null
printf "\nProcesses:\n"
ps 2>/dev/null
