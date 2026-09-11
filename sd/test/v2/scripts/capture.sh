#!/bin/sh
if [ $# -eq 4 ] && [ "$1" = "-o" ] && [ "$3" = "-q" ];
then
   OUTPUT=$2
   QUALITY=$4
   case "$QUALITY" in
      ''|*[!0-9]*) echo "Invalid JPEG quality"; exit 2 ;;
   esac
   TMPBASE=/tmp/yi-capture.$$
   /usr/local/bin/test_yuvcap -b 0 -Y -f "$TMPBASE" -r 1
   if [ $? -eq 0 ];
   then
      YUV_FILE=`find /tmp -name "yi-capture.$$*" -type f | head -n 1`
      if [ -z "$YUV_FILE" ]; then
         echo "Failed to find captured frame"
         exit 1
      fi
      # we get something like /tmp/tmp.xr8C1d_prev_M_1920x1080.yuv
      # we want to extract width and height
      YUV_FILE_SIZE=${YUV_FILE##*_}
      # we get 1920x1080.yuv
      WIDTH=${YUV_FILE_SIZE%%x*}
      HEIGHT_YUV=${YUV_FILE_SIZE##*x}
      HEIGHT=${HEIGHT_YUV%%.*}
      # convert yuv to jpg
      /usr/local/bin/jpg_enc -y "$YUV_FILE" -w "$WIDTH" -h "$HEIGHT" -q "$QUALITY" -f "$OUTPUT"
      RESULT=$?
      rm -f "$YUV_FILE"
      exit $RESULT
   else
      echo "Failed to capture frame, please ensure that test_encode has been correctly started." 
      exit 1
   fi
else
   echo "Usage:  capture.sh -o <JPG output file> -q <quality>"
   echo "Sample: capture.sh -o /sdcard/test/capture.jpg -q 70"
fi
