#!/bin/sh

printf "Content-Type: text/plain\r\n\r\n"
printf "Reboot requested\n"
( sleep 1; reboot ) >/tmp/yi-hack-reboot.log 2>&1 &
