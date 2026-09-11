#!/bin/sh

OUTPUT=/tmp/yi-hack-snapshot.$$
trap 'rm -f "$OUTPUT"' 0 1 2 3 15

if /sdcard/test/v2/scripts/capture.sh -o "$OUTPUT" -q 80 >/dev/null 2>&1 &&
   [ -s "$OUTPUT" ]; then
   printf "Content-Type: image/jpeg\r\n\r\n"
   cat "$OUTPUT"
else
   printf "Status: 503 Service Unavailable\r\n"
   printf "Content-Type: text/plain\r\n\r\n"
   printf "Snapshot capture failed\n"
   exit 1
fi
