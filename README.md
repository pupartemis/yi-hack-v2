yi-hack-v2
==========

Custom startup files and utilities for the Xiaomi Yi Ants Camera 2 / H21
platform. This project is tested with firmware `2.1.1_20160429113900` and is
intended to turn the camera into a local RTSP appliance for software such as
Frigate and Home Assistant.

Special thanks to **fritz-smh** for the original yi-hack project:
https://github.com/fritz-smh/yi-hack

This repository was forked from **niclet/yi-hack-v2**:
https://github.com/niclet/yi-hack-v2

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

### H21 audio bring-up

The camera has ALSA capture devices and the stock firmware contains an AAC
encoder inside `/home/web/ipc`. The modified startup does not run that process,
and `/usr/local/bin/rtsp_server` is video-only. The payload therefore does not
enable an audio setting or advertise an incomplete stream.

For hardware and codec diagnostics from a Telnet shell, run:

```sh
/sdcard/test/v2/scripts/audio_probe.sh
/sdcard/test/v2/scripts/audio_probe.sh --init
```

The `--init` form writes the codec registers using the firmware-provided
`/usr/local/bin/set_audio.sh`. It does not start recording or alter RTSP
behavior. Completing audio requires a standalone ARM AAC producer and an RTSP
server with an AAC RTP track.

The source for the next diagnostic is
`sd/test/v2/audio/alsa_probe.c`. It tests `hw:0,0` and `hw:0,1` with
`S16_LE` at 48 kHz for both mono and stereo, then reads one short PCM block.
It must be cross-compiled against the camera's ARM userspace and
`libasound.so.2` before being copied to the camera.

The H21 probe accepted only `hw:0,0`, stereo, `S16_LE`, 48 kHz, and read 480
frames successfully. Mono was rejected by ALSA with `EINVAL`; the dummy
`hw:0,1` device is not part of the microphone path.

### PCMA audio producer

The first audio implementation is a standalone G.711 A-law producer:

```sh
make -C sd/test/v2/audio pcma_producer
/sdcard/test/v2/audio/pcma_producer > /tmp/audio.pcma
```

It captures stereo `S16_LE` at 48 kHz from the confirmed `hw:0,0` ALSA device,
downmixes to mono, decimates to 8 kHz, and writes raw PCMA at real-time speed.
Each 160-byte block represents 20 ms and is directly suitable as an
`PCMA/8000` RTP payload. The producer is intentionally not started by modified
startup yet: the repository's existing RTSP server has no audio track or
consumer for this stream. The next integration step is an RTSP server that
reads 160-byte blocks and advertises `m=audio`, `a=rtpmap:8 PCMA/8000`.

### Separate audio RTSP server

`sd/test/v2/audio/audio_rtsp_server.c` provides a standalone audio-only RTSP
server on TCP port `8555`. It captures the same H21 ALSA profile, packetizes
PCMA as RTP/AVP payload type 8, and advertises:

```text
rtsp://<camera-ip>:8555/audio
```

The service is disabled by default. Build and copy
`sd/test/v2/audio/audio_rtsp_server` to the camera, then set
`YI_HACK_AUDIO_RTSP_SERVER=YES`. The existing video RTSP server remains
unchanged. The initial implementation supports UDP RTP transport and one
client; TCP-interleaved RTP and multi-client fan-out are intentionally left
for follow-up work.

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

- Improve the working audio pipeline:
  - add a proper low-pass filter/resampler instead of simple six-to-one
    decimation;
  - complete ALSA recovery for all recoverable errors;
  - support RTSP-over-TCP interleaved RTP;
  - support multiple audio clients and robust RTSP session parsing;
  - advertise the camera's actual IP address in SDP;
  - add a service watchdog and clean shutdown handling;
  - optionally support PCMU in addition to PCMA;
  - evaluate AAC only if bandwidth or quality requirements justify it.
- Add audio controls and features:
  - audio level monitoring;
  - mute controls;
  - event-triggered recording;
  - sound detection;
  - investigate two-way audio if the camera speaker path is available.
- Investigate IR and night vision safely:
  - locate stock day/night and IR-cut control calls in `home.bin` and
    `/home/web/ipc`;
  - identify the IR-cut filter and illuminator interfaces;
  - determine whether control uses GPIO, PWM, a kernel driver, or another
    firmware device;
  - add read-only day/night status detection first;
  - add explicit `day`, `night`, and `auto` modes;
  - expose `YI_HACK_NIGHT_MODE=AUTO` and web controls only after pin-level
    behavior is verified;
  - never toggle candidate GPIOs blindly because they may affect the sensor,
    LEDs, Wi-Fi, or boot behavior.
- Add the read-only `sd/test/v2/scripts/ir_probe.sh` to the diagnostic
  deployment and use it to record the camera's current day/night interfaces.
- Static recovery from the stock H21 IPC binary now identifies:
  - IR illuminator brightness at `/sys/class/backlight/0.pwm_bl/brightness`,
    with `max_brightness=255` on the test camera.
  - IR-cut control through `/dev/emd` and ioctl `0x400464c9`
    (`_IOW('d', 0xc9, 4)`).
  - Stock IR-cut selector values `0x18` and `0x19`, driven in a
    break-before-make sequence with approximately 150 ms between directions.
  - The stock sensor input path
    `/sys/devices/e8000000.apb/e801d000.adc/adcsys`.
  - The `0x18` and `0x19` values are driver group selectors, not confirmed
    physical GPIO numbers. Do not replace them with GPIO sysfs writes.
- A temporary official-startup boot confirmed that `/home/web/ipc -w` owns
  `/dev/emd`, ALSA capture, `ipc.config`, and a dedicated
  `ivs_daynight_thr` thread. It also opened GPIO value nodes for 24, 25, 33,
  38, 46, 92, and 100. The camera was restored to modified startup after the
  check; no IR control was invoked.
- The recovered illuminator path is implemented as the explicit helper
  `sd/test/v2/scripts/ir_light.sh`. On the camera it supports `status`, `off`,
  `on`, or a numeric brightness from `0` to `255`. It does not operate the
  IR-cut filter and is not started automatically.
- A standalone controller source is now under `sd/test/v2/ir/`. It supports
  explicit `day` and `night` transitions through the recovered `/dev/emd`
  ioctl and the PWM brightness path, with single-process locking and
  break-before-make sequencing. `auto` is intentionally guarded by a
  non-zero, caller-supplied sensor threshold and offset until the stock
  sensor record is decoded. The controller is not enabled or deployed yet.
- Optical IR-cut polarity testing is deferred. It requires a live video view,
  Telnet recovery access, a one-shot test using only the recovered `/dev/emd`
  ioctl, and an observable scene containing visible colors plus an IR source.
  The test must keep the illuminator off, compare both candidate selector
  values, and restore the original selector and brightness after each attempt.
  An ioctl success alone does not identify day versus night polarity.
- Automatic day/night mode remains disabled until the optical polarity and
  sensor record format/threshold are verified. The controller's polarity
  defaults are placeholders and must not be used for unattended operation.
- Add authenticated web access or make the web UI read-only by default.
- Add a lightweight RTSP/Wi-Fi watchdog with clear failure logging.
- Add reproducible native builds for the camera's ARM userspace.
- Add automated shell/configuration checks and documented release artifacts.
