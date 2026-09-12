#!/bin/sh

if [ "${REQUEST_METHOD:-GET}" != "POST" ]; then
   printf "Status: 405 Method Not Allowed\r\n"
   printf "Allow: POST\r\n"
   printf "Content-Type: text/plain\r\n\r\n"
   printf "Reboot requires POST\n"
   exit 1
fi

printf "Content-Type: text/plain\r\n"
printf "Cache-Control: no-store\r\n\r\n"
printf "Reboot requested\n"
( sleep 1; reboot ) >/tmp/yi-hack-reboot.log 2>&1 &
