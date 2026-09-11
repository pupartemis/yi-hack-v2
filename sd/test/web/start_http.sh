#!/bin/sh

ROOT=/sdcard/test/web
PORT=${YI_HACK_HTTP_PORT:-80}
PIDFILE=/tmp/yi-hack-httpd.pid

case "$PORT" in
   ''|*[!0-9]*) echo "Invalid HTTP port: $PORT"; exit 2 ;;
esac

if [ -f "$PIDFILE" ] && kill -0 "`cat "$PIDFILE"`" 2>/dev/null; then
   echo "HTTP server is already running"
   exit 0
fi

HTTPD=
for candidate in httpd /usr/sbin/httpd /usr/local/bin/httpd /usr/bin/httpd; do
   if command -v "$candidate" >/dev/null 2>&1; then
      HTTPD=$candidate
      break
   fi
done

if [ -n "$HTTPD" ]; then
   "$HTTPD" -p "$PORT" -h "$ROOT" > /tmp/yi-hack-httpd.log 2>&1 &
   PID=$!
   sleep 1
   if kill -0 "$PID" 2>/dev/null; then
      echo "$PID" > "$PIDFILE"
      echo "HTTP server started on port $PORT"
   else
      echo "Error: HTTP server failed to bind port $PORT"
      exit 1
   fi
else
   echo "Error: no compatible httpd executable was found"
   exit 1
fi
