#!/bin/sh

# Explicit IR illuminator control. This does not operate the IR-cut filter.

BRIGHTNESS=/sys/class/backlight/0.pwm_bl/brightness
MAX_BRIGHTNESS=/sys/class/backlight/0.pwm_bl/max_brightness

usage() {
   echo "Usage: $0 status | off | on | <brightness 0-255>" >&2
   exit 2
}

[ -r "$BRIGHTNESS" ] || {
   echo "IR brightness control is unavailable: $BRIGHTNESS" >&2
   exit 1
}

case "${1:-}" in
   status)
      printf "brightness="
      cat "$BRIGHTNESS"
      if [ -r "$MAX_BRIGHTNESS" ]; then
         printf "max_brightness="
         cat "$MAX_BRIGHTNESS"
      fi
      exit 0
      ;;
   off)
      value=0
      ;;
   on)
      value=255
      ;;
   ''|*[!0-9]*)
      usage
      ;;
   *)
      value=$1
      ;;
esac

max=255
if [ -r "$MAX_BRIGHTNESS" ]; then
   max=`cat "$MAX_BRIGHTNESS"`
fi

case "$max" in
   ''|*[!0-9]*) echo "Invalid max brightness: $max" >&2; exit 1 ;;
esac

if [ "$value" -gt "$max" ]; then
   echo "Brightness must be between 0 and $max" >&2
   exit 2
fi

printf '%s\n' "$value" > "$BRIGHTNESS" || {
   echo "Failed to set IR brightness" >&2
   exit 1
}

actual=`cat "$BRIGHTNESS"` || exit 1
if [ "$actual" != "$value" ]; then
   echo "IR brightness readback mismatch: requested $value, got $actual" >&2
   exit 1
fi

echo "IR brightness set to $actual"
