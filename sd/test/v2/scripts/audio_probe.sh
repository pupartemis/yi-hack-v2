#!/bin/sh

# H21 audio bring-up diagnostic. This intentionally does not start the stock
# /home/web/ipc process or claim that an RTSP audio track is available.

INIT=0
if [ "$1" = "--init" ]; then
   INIT=1
elif [ $# -ne 0 ]; then
   echo "Usage: audio_probe.sh [--init]"
   exit 2
fi

echo "Yi Hack v2 H21 audio probe"
echo "Kernel: `uname -a 2>/dev/null`"

if [ ! -d /dev/snd ]; then
   echo "ERROR: /dev/snd is unavailable"
   exit 1
fi

echo "Capture devices:"
ls -l /dev/snd/pcm*C*c 2>/dev/null || {
   echo "ERROR: no ALSA capture devices found"
   exit 1
}

if [ -r /proc/asound/cards ]; then
   echo "ALSA cards:"
   cat /proc/asound/cards
fi
if [ -r /proc/asound/pcm ]; then
   echo "ALSA PCM devices:"
   cat /proc/asound/pcm
fi

if [ -x /usr/local/bin/set_audio.sh ]; then
   echo "Codec setup: available"
   if [ "$INIT" = 1 ]; then
      echo "Initializing codec through /usr/local/bin/set_audio.sh"
      /usr/local/bin/set_audio.sh || {
         echo "ERROR: codec initialization failed"
         exit 1
      }
   else
      echo "Codec setup: not run (use --init to write codec registers)"
   fi
else
   echo "ERROR: /usr/local/bin/set_audio.sh is unavailable"
   exit 1
fi

for command_name in arecord ffmpeg avconv gst-launch-1.0; do
   if command -v "$command_name" >/dev/null 2>&1; then
      echo "Capture tool: $command_name"
   fi
done

if [ -x /home/web/ipc ]; then
   echo "Stock AAC implementation: embedded in /home/web/ipc"
else
   echo "ERROR: /home/web/ipc is unavailable"
   exit 1
fi

echo "RTSP audio: unavailable in /usr/local/bin/rtsp_server"
echo "Result: capture/codec hardware is present; a standalone AAC producer and"
echo "an audio-capable RTSP server are still required."
