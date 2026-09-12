# Copilot project context

This document is a handoff for future contributors and Copilot sessions.
It describes the verified camera, repository, deployment process, current
limitations, and the next useful work. Do not add Wi-Fi passwords, private
keys, or other credentials here.

## Project and target

- Repository: `pupartemis/yi-hack-v2`
- Upstream fork source: `niclet/yi-hack-v2`
- Local project path used during development:
  `/Users/jan/Desktop/yihack2/yi-hack-v2`
- Target: Xiaomi Yi Ants Camera 2 / H21 platform
- Tested firmware: `2.1.1_20160429113900`
- Camera platform is Ambarella-based.
- The project is intended to make the camera a low-load local RTSP appliance
  for Frigate/Home Assistant instead of using Xiaomi cloud services.
- The camera's actual Wi-Fi interface is `mlan0`, not `wlan0`.

## Repository layout

The camera payload is under `sd/`. The contents of `sd/` must be copied to
the SD-card root, not the repository directory and not a nested `sd/`
directory.

Expected SD-card layout:

```text
home.bin
test/
  factory_test.sh
  yi-hack-v2.cfg
  wpa_supplicant.conf
  v2/
  web/
```

The Wi-Fi file contains real local credentials in development environments.
Keep it private and do not print, commit, or copy its contents into issue
reports or documentation.

## Current modified-mode architecture

`sd/test/factory_test.sh`:

1. Parses supported `YI_HACK_*` settings.
2. Starts Telnet if enabled.
3. Starts anonymous writable FTP if enabled.
4. Starts the custom BusyBox HTTP interface if enabled.
5. Runs official or modified startup.

`sd/test/v2/scripts/startup_modified.sh`:

1. Copies `wpa_supplicant.conf` to `/tmp/config`.
2. Runs the stock `/bak/usr/local/bin/usb_wifi.sh`.
3. Uses `/usr/local/bin/amba_debug -g 51 -d 0x1`.
4. Initializes GPIO LEDs and connects with `wifi_auto.sh`.
5. Initializes the MN34220PL sensor.
6. Runs `test_tuning`.
7. Configures two encoder streams.
8. Starts the firmware-provided `rtsp_server` when
   `YI_HACK_RTSP_SERVER=YES`.

The main encoder is currently configured as:

```text
1920x1080, 25 fps, target bitrate 1500000
```

The secondary stream is:

```text
640x360 MJPEG, 25 fps
```

The current custom RTSP server is video-only.

## Current camera runtime

The known test camera has normally used `192.168.121.46`, but DHCP can change
the address. Check the router's DHCP leases if the address is unreachable.

Default recovery configuration:

```text
YI_HACK_STARTUP_MODE=MODIFIED
YI_HACK_TELNET_SERVER=YES
YI_HACK_FTP_SERVER=YES
YI_HACK_HTTP_SERVER=YES
YI_HACK_HTTP_PORT=8080
YI_HACK_RTSP_SERVER=YES
```

Expected services:

| Service | Port | Notes |
|---|---:|---|
| Telnet | 23 | Unauthenticated root shell |
| FTP | 21 | Anonymous writable filesystem access |
| RTSP | 554 | `/stream1` and `/stream2` |
| Custom HTTP | 8080 | Dashboard and CGI endpoints |

SSH is not available. `nc <camera-ip> 23` is a Telnet shell, not SSH.

Modified mode intentionally does not start the heavy stock application/cloud
processes such as `/home/web/ipc`, `show_stack`, Mosquitto, and stock cloud
services. It is incompatible with the official Xiaomi app workflow.

## RTSP and Frigate facts

Verified endpoints:

```text
rtsp://<camera-ip>/stream1
rtsp://<camera-ip>/stream2
```

Verified media:

- `stream1`: H.264, 1920x1080, 25 fps.
- `stream2`: MJPEG, 640x360, 25 fps.
- Neither stream currently contains audio.

Transport behavior:

- Main H.264 media works reliably over UDP.
- Main H.264 interleaved TCP media is unreliable with the old Ambarella/
  LIVE555 server.
- Secondary MJPEG works over TCP and UDP.
- A successful RTSP `OPTIONS` or `DESCRIBE` does not prove that TCP media
  delivery works.

Recommended Frigate/go2rtc shape:

```yaml
go2rtc:
  streams:
    yi1080p:
      - rtsp://<camera-ip>/stream1
    yi1080p_low:
      - rtsp://<camera-ip>/stream2#transport=tcp

cameras:
  yi1080p:
    ffmpeg:
      inputs:
        - path: rtsp://127.0.0.1:8554/yi1080p
          roles: [record]
        - path: rtsp://127.0.0.1:8554/yi1080p_low
          roles: [detect]
    detect:
      width: 640
      height: 360
```

If go2rtc reports a 404 for `127.0.0.1:8554/yi1080p_low`, first check that
go2rtc registered the named stream. That error can be local go2rtc
configuration, not necessarily a missing camera endpoint.

## Web interface

Files:

- `sd/test/web/index.html`
- `sd/test/web/start_http.sh`
- `sd/test/web/cgi-bin/status.sh`
- `sd/test/web/cgi-bin/snapshot.sh`
- `sd/test/web/cgi-bin/reboot.sh`
- `sd/test/web/cgi-bin/settings.sh`

The dashboard provides:

- Auto-refreshing status, load, memory, network, and video-process output.
- Native JPEG snapshots.
- Links to both RTSP endpoints.
- LED controls: red, green, blue; off/on/flash.
- Telnet, FTP, HTTP, and RTSP settings.
- Reboot through POST only.

Settings behavior:

- LED changes apply immediately.
- FTP and Telnet changes apply immediately.
- HTTP and RTSP settings are persisted but take effect after reboot.
- The UI has no authentication and must stay on a trusted LAN.
- The settings handler uses simple form parsing suitable for the firmware's
  BusyBox environment; do not assume modern shell utilities exist.

There is deliberately no audio checkbox yet because the current RTSP server
cannot publish audio.

## Audio findings and future implementation

The microphone hardware is present. On the live camera, these were observed:

- ALSA capture devices:
  `/dev/snd/pcmC0D0c` and `/dev/snd/pcmC0D1c`
- Loaded modules include `snd_soc_alc5633`,
  `snd_soc_ambarella_i2s`, `snd_pcm`, and `snd`.
- `/proc/audio11/alc_audio` exists.
- `/usr/local/bin/set_audio.sh` configures the audio codec.
- Stock `/home/web/ipc` contains AAC encoder and audio-buffer symbols.

However:

- The custom `/usr/local/bin/rtsp_server` only advertises H.264 video.
- Its SDP has no `m=audio` section.
- The repository has no compatible audio producer or RTSP audio server.
- yi-hack-v5 contains an audio design, but its binaries and shared-memory
  offsets are for different camera platforms and cannot be copied directly.

Future audio work should:

1. Determine whether H21 audio is best captured from ALSA or the stock shared
   memory/video buffer.
2. Produce valid AAC/ADTS frames.
3. Build an ARM-compatible FIFO/audio producer.
4. Extend or replace the v2 RTSP server with an AAC RTP track.
5. Add a real `YI_HACK_AUDIO_SERVER` setting only after end-to-end testing.
6. Verify go2rtc, Frigate, VLC, and ffprobe compatibility.

Do not implement a UI-only audio setting.

## Diagnostics and failure modes

Persistent logs:

```text
/sdcard/test/logs/factory_test.log
/sdcard/test/logs/modified_startup.log
```

The modified startup log includes:

- `usb_wifi.sh` status
- interface state
- DHCP configuration
- recent WPA supplicant output

Important startup behavior:

- `usb_wifi.sh` returning status `2` was observed during a successful boot.
- Status `1` means the stock helper detected a connected USB host and aborts.
- Powering the camera from a computer USB port can therefore cause failure.
  Use a normal 5 V power adapter.
- Wrong SSID/password causes a red LED.
- Yellow means early startup.
- Blinking blue means Wi-Fi association/DHCP in progress.
- Solid blue means network setup succeeded.
- Red means Wi-Fi initialization, authentication, or DHCP failed.

If the SD boot fails:

1. Power off.
2. Inspect the two log files on the card.
3. Verify the payload is at the SD root with `test/` directly present.
4. Verify Wi-Fi credentials without exposing them.
5. Boot once without the SD card to distinguish camera/power problems from
   SD startup problems.
6. Do not rapidly power-cycle the camera.

No reliable numeric temperature sensor was exposed by the tested firmware.
The modified pipeline reduces load relative to stock startup, but the sensor,
encoder, Wi-Fi, and RTSP server can still make the camera hot. Use an inline
USB power meter for actual power measurements.

## Deployment workflow

Before changing the camera:

1. Inspect `git status`.
2. Keep `sd/test/wpa_supplicant.conf` private.
3. Validate shell changes with `sh -n`.
4. Run `git diff --check`.
5. Prefer uploading one file at a time over Telnet using base64 chunks.
6. Verify the remote SHA-256 before replacing the target file.
7. Avoid rebooting unless the user explicitly requested it or the change
   requires reboot.

For an SD-card deployment, synchronize the contents of `sd/test/` into
`/Volumes/BOOT/test/`, preserving the card's private Wi-Fi file and diagnostic
logs. Run `sync` and eject with:

```sh
diskutil eject /Volumes/BOOT
```

Never use a computer USB host as the camera's power source during its modified
boot.

## Git history and current branch

The active branch is `master`, pushed to `origin`.

Recent feature commits:

- `709614e` Add v2 web interface and RTSP startup
- `9ff271a` Document modified startup and web interface
- `ed6bfa5` Add web service and LED controls
- `3ac1ad6` Reduce main RTSP bitrate
- `f538d7a` Expand project documentation

This context file should be updated when verified runtime behavior,
configuration, deployment steps, or priorities materially change.
