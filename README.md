yi-hack-v2
==========

Custom startup files and utilities for the Xiaomi Yi Ants Camera 2 / H21
platform. This project is tested with firmware `2.1.1_20160429113900` and is
intended to turn the camera into a local RTSP appliance for software such as
Frigate and Home Assistant.

Special thanks to **fritz-smh** for the original yi-hack project:
https://github.com/fritz-smh/yi-hack

## What this project provides

The modified startup mode:

- Reuses the camera's stock Wi-Fi initialization.
- Avoids the heavy Xiaomi application and cloud processes.
- Starts the camera sensor and two local video encoders.
- Provides an H.264 RTSP stream at `1920x1080` and 25 fps.
- Provides a secondary MJPEG stream at `640x360` and 25 fps.
- Provides a lightweight status, snapshot, service-control, and LED web UI.
- Keeps persistent diagnostics on the SD card.

The tested target is the Xiaomi Yi Ants Camera 2 / H21 camera. The firmware,
hardware addresses, encoder commands, and stream-buffer layout are platform
specific; files from yi-hack-v5 are not drop-in compatible.

## Installation

### Prepare the SD card

Format a microSD card as FAT32, then copy the **contents** of the `sd/`
directory to the root of the card:

```text
home.bin
test/
```

Do not copy the Git repository directory itself or leave `sd/test/` nested
below another directory. The camera must find these paths directly:

```text
/sdcard/test/factory_test.sh
/sdcard/test/yi-hack-v2.cfg
/sdcard/test/wpa_supplicant.conf
/sdcard/test/v2/scripts/startup_modified.sh
```

Edit `test/wpa_supplicant.conf` with the camera's Wi-Fi credentials. Keep this
file private; it contains a network password and is not intended to be
committed.

### Boot

1. Power the camera off.
2. Insert the prepared card.
3. Power it from a normal 5 V supply, not from a computer USB host port.
4. Wait for the LED sequence.

The camera's stock Wi-Fi helper reports a connected USB host as a startup
failure. A computer USB port can therefore produce a red LED even when the
Wi-Fi configuration is correct.

LED states during modified startup:

- Yellow: early camera startup.
- Blinking blue: Wi-Fi association and DHCP are in progress.
- Solid blue: network setup succeeded.
- Red: Wi-Fi initialization, authentication, or DHCP failed.

The card must remain in the camera for the hack to run. Without it, the camera
boots its stock firmware and uses the official startup path.

## Configuration

The example `test/yi-hack-v2.cfg` enables modified startup and recovery access:

```text
YI_HACK_STARTUP_MODE=MODIFIED
YI_HACK_TELNET_SERVER=YES
YI_HACK_FTP_SERVER=YES
YI_HACK_HTTP_SERVER=YES
YI_HACK_HTTP_PORT=8080
YI_HACK_RTSP_SERVER=YES
```

Supported options include:

| Option | Values | Effect |
|---|---|---|
| `YI_HACK_STARTUP_MODE` | `MODIFIED`, anything else selects official startup | Selects the startup path |
| `YI_HACK_TELNET_SERVER` | `YES` / `NO` | Starts the unauthenticated root Telnet shell |
| `YI_HACK_FTP_SERVER` | `YES` / `NO` | Starts the anonymous writable FTP service |
| `YI_HACK_HTTP_SERVER` | `YES` / `NO` | Starts the custom web interface |
| `YI_HACK_HTTP_PORT` | Numeric port | Custom web interface port |
| `YI_HACK_RTSP_SERVER` | `YES` / `NO` | Starts RTSP in modified mode |
| `YI_HACK_LANGUAGE` | `CN`, `US`, `FR` | Selects startup voice files |
| `YI_HACK_TIME_TIMEZONE` | POSIX timezone string | Camera timezone |
| `YI_HACK_TIME_FORMAT` | `strftime` format | Optional embedded timestamp format |
| `YI_HACK_PROXY` | Proxy URL | Optional stock/cloud proxy |

The web interface can persist the Telnet, FTP, HTTP, and RTSP settings. FTP
and Telnet changes are applied immediately; HTTP and RTSP changes take effect
after reboot. LED changes are applied immediately.

## Services and access

In the default example configuration:

| Service | Port | Notes |
|---|---:|---|
| Telnet | 23 | Unauthenticated root shell; recovery only |
| FTP | 21 | Anonymous writable access to the camera filesystem |
| RTSP | 554 | Main and secondary local video streams |
| Yi Hack HTTP | 8080 | Status, snapshot, settings, and reboot UI |

The stock HTTP service may still occupy port 80 in official startup. SSH is not
available: this firmware payload does not include an SSH server such as
Dropbear. A connection made with `nc <IP> 23` is a Telnet shell, not SSH.

Disable Telnet and FTP after reliable web or serial recovery has been
established. They are intentionally enabled in the example configuration to
make first-time recovery easier.

## RTSP streams

The modified startup exposes:

```text
rtsp://<camera-ip>/stream1
rtsp://<camera-ip>/stream2
```

`stream1` is H.264 at `1920x1080`, 25 fps. Its configured target bitrate is
1.5 Mbps. `stream2` is MJPEG at `640x360`, 25 fps and is useful for low-cost
detection.

This camera uses an old Ambarella/LIVE555 RTSP server. The main H.264 stream
has been reliable over UDP but unreliable with interleaved TCP media. The
secondary MJPEG stream can use TCP. RTSP control requests may succeed over TCP
even when the main stream's media transport does not, so a successful
`DESCRIBE` alone does not prove TCP media works.

## Frigate example

Use UDP for the main H.264 stream and TCP for the secondary MJPEG stream:

```yaml
version: 0.17-0

mqtt:
  enabled: false

go2rtc:
  streams:
    yi1080p:
      - rtsp://192.168.121.46/stream1
    yi1080p_low:
      - rtsp://192.168.121.46/stream2#transport=tcp

cameras:
  yi1080p:
    enabled: true
    ffmpeg:
      inputs:
        - path: rtsp://127.0.0.1:8554/yi1080p
          roles:
            - record
        - path: rtsp://127.0.0.1:8554/yi1080p_low
          roles:
            - detect
    detect:
      width: 640
      height: 360
```

If go2rtc reports `404 Not Found` for the local low stream, inspect go2rtc's
registered streams first; that error means the local go2rtc name was not
registered and does not by itself prove that the camera's `stream2` endpoint is
missing.

The camera streams currently contain video only. The microphone hardware and
AAC-related stock firmware components exist, but the custom v2 RTSP server
does not advertise or send an audio track.

## Web interface

With the example configuration, open:

```text
http://<camera-ip>:8080/
```

The dashboard provides:

- Auto-refreshing uptime, load, memory, network, and video-process status.
- A live JPEG snapshot from the native v2 capture tools.
- Direct links to both RTSP endpoints.
- LED controls for red, green, and blue: off, on, or flash.
- Telnet, FTP, HTTP, and RTSP service controls.
- A reboot action sent only through HTTP `POST`.

The interface is intentionally lightweight and uses the firmware-provided
BusyBox `httpd`. It does not implement authentication or firmware updates.
Do not expose it outside a trusted LAN.

## Diagnostics and recovery

Persistent startup logs are written to:

```text
/sdcard/test/logs/factory_test.log
/sdcard/test/logs/modified_startup.log
```

The modified startup log records Wi-Fi helper statuses, interface state,
DHCP configuration, and recent WPA messages. These are the first files to
inspect after a red LED or a failed network boot.

Common causes:

- Incorrect SSID or password in `wpa_supplicant.conf`.
- The SD payload copied at the wrong directory level.
- Powering from a computer USB host, which the stock Wi-Fi helper detects as
  a connected USB host and rejects.
- DHCP or access-point authentication failure.

If the camera is stuck with the card inserted, power it off and boot once
without the card. A normal solid-blue stock boot separates camera/power
problems from SD-card startup problems. Do not rapidly power-cycle the camera.

## Security and operational notes

The default recovery services are unsafe on an untrusted network:

- Telnet provides an unauthenticated root shell.
- FTP allows anonymous writable access.
- The web interface has no authentication.
- The reboot endpoint is intentionally available to local web clients.

Use an isolated VLAN or trusted LAN, and disable Telnet/FTP when they are no
longer needed. The camera's modified mode also disables the official Xiaomi
application/cloud workflow and is intended for local operation.

The modified video pipeline is substantially lighter than the official
application stack, but the sensor, encoder, Wi-Fi, and RTSP server still
produce significant heat and CPU load. No reliable temperature sensor was
exposed by this firmware during testing. Measure power with a USB power meter
if consumption matters.

## Project structure

```text
home.bin
test/
  factory_test.sh
  logs/
  v2/
    audio/fr/                  French voice files
    bin/libyihackv2.so         Native hack library
    bin/tcpsvd                 FTP service launcher
    scripts/capture.sh         Native JPEG snapshot helper
    scripts/led.sh             GPIO LED control
    scripts/startup_modified.sh
    scripts/startup_official.sh
  web/
    index.html                 Web dashboard
    cgi-bin/                   Status, snapshot, settings, and reboot handlers
  wpa_supplicant.conf          Local Wi-Fi credentials; keep private
  yi-hack-v2.cfg               Runtime configuration
```

## TODO

- Implement the audio pipeline:
  - capture microphone audio through the camera's ALSA or stock shared-memory
    path;
  - produce AAC frames;
  - extend or replace the v2 RTSP server with an AAC RTP track;
  - expose a real `YI_HACK_AUDIO_SERVER` setting only after the pipeline is
    functional;
  - verify compatibility with go2rtc, Frigate, and common RTSP clients.
- Add authenticated web access or make the web UI read-only by default.
- Add a lightweight RTSP/Wi-Fi watchdog with clear failure logging.
- Add reproducible native builds for the camera's ARM userspace.
- Add automated shell/configuration checks and documented release artifacts.
