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

Audio is now available through a separate, opt-in RTSP service; the existing
video RTSP server remains unchanged.

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

The working deployment uses a separate PCMA RTSP stream:

```text
rtsp://<camera-ip>:8555/audio
```

go2rtc can combine it with the existing video stream using a second source
with `#media=audio`. This has been validated end to end with the live camera.

Do not implement a UI-only audio setting.

The repository now includes `sd/test/v2/scripts/audio_probe.sh`. It performs
read-only ALSA/firmware checks by default, and `--init` explicitly runs the
firmware's `set_audio.sh` codec setup. It does not start `/home/web/ipc`,
because that is the full stock application process and is not a safe audio
producer for modified startup. The live H21 checks confirmed:

- ARMv7 Ambarella S2L kernel.
- ALSA capture devices `pcmC0D0c` and `pcmC0D1c`.
- `snd_soc_alc5633`, `snd_soc_ambarella_i2s`, `snd_pcm`, and related modules.
- No `arecord`, FFmpeg, GStreamer, or standalone AAC utility.
- AAC encoder and ALSA code embedded in `/home/web/ipc`.
- No audio support in `/usr/local/bin/rtsp_server`.

The first native diagnostic source is
`sd/test/v2/audio/alsa_probe.c`. It tests both ALSA capture devices with
`S16_LE` at 48 kHz for mono and stereo and reads a short PCM block. Its
`Makefile` expects an `arm-linux-gnueabihf-gcc` toolchain and `libasound`
development files. The current macOS host has no ARM Linux toolchain, and the
camera has no compiler or ALSA headers, so the source is ready but the binary
cannot yet be built or deployed from this host.

An ARM binary was subsequently built using Clang plus the camera's extracted
ARM `libasound.so.2`/libc runtime and deployed temporarily to the camera. It
confirmed the microphone profile: `hw:0,0`, stereo, `S16_LE`, 48 kHz, with 480
PCM frames readable. Mono was rejected with ALSA `EINVAL`. The diagnostic
process reports a shutdown-time segmentation fault after the successful read,

Static analysis of the stock IPC ALSA path adds these details:

- The capture setup uses `snd_pcm_open("default", ..., SND_PCM_NONBLOCK=0)`.
- It selects interleaved access (`SND_PCM_ACCESS_RW_INTERLEAVED`).
- The stock format selector maps to `S16_LE` for the active profile.
- The stock configuration stores a 48 kHz rate and one channel before calling
  `snd_pcm_hw_params_set_channels`; the standalone hardware probe showed that
  this firmware's active microphone path accepts stereo and rejects mono, so
  the eventual producer must validate the channel choice at runtime.
- The setup obtains the maximum buffer time, derives a period time, applies
  near-period and near-buffer settings, then reads the negotiated period and
  buffer sizes. It sets the software available minimum to the period size and
  allocates `period_size * bytes_per_sample` bytes for each capture operation.
- The read path uses `snd_pcm_readi` and contains explicit ALSA status/XRUN
  recovery handling.

The stock binary does not expose a reusable AAC API or an isolated audio
process. Its AAC encoder and ring-buffer code are internal to the full IPC
application, so launching `/home/web/ipc` in modified startup remains unsafe.

The implemented audio path is a standalone PCMA producer:

- Captures stereo `S16_LE` at 48 kHz from the confirmed `hw:0,0` device.
- Downmixes stereo to mono and decimates by six to 8 kHz.
- Encodes G.711 A-law without an external codec library.
- Emits raw PCMA in real time, in 160-byte/20 ms blocks suitable for
  `PCMA/8000` RTP.
- Is started by modified startup only when
  `YI_HACK_AUDIO_RTSP_SERVER=YES`.

The separate audio RTSP path is now implemented as
`sd/test/v2/audio/audio_rtsp_server.c`:

- Listens for RTSP control on TCP port 8555.
- Advertises `PCMA/8000` as RTP payload type 8 at `/audio`.
- Accepts one UDP RTP client using the port from the RTSP `SETUP` request.
- Captures and packetizes 20 ms audio blocks from the H21 microphone.
- Is gated by `YI_HACK_AUDIO_RTSP_SERVER`, which defaults to `NO`.
- Does not modify or replace the existing video RTSP server.

This first server is intentionally minimal: UDP RTP only, one client, and no
RTSP-over-TCP interleaving. It is ARM-built against the camera's GLIBC 2.18
runtime and has been validated on the live camera with RTSP control and
160-byte PCMA RTP packets.

Audio follow-up work:

- Replace simple six-to-one decimation with a proper low-pass
  filter/resampler.
- Handle all recoverable ALSA errors.
- Add RTSP-over-TCP interleaving, multiple clients, robust session parsing,
  accurate SDP addressing, a watchdog, and clean shutdown.
- Optionally add PCMU support.
- Consider AAC only if PCMA's bandwidth or quality becomes insufficient.
- Add audio levels, mute, event recording, sound detection, and investigate
  two-way audio if a speaker path exists.

## IR and night vision investigation

The firmware contains evidence of stock night-mode functionality, including
`ircut`, `daynight_mode`, `day2night_`, `COMM_EVENT_DAYNIGHT_N2D3_`, and
`mn34220pl_aliso_adj_param_night.bin`. The platform also exposes an
`e8006000.ir` device, but this appears to be an infrared input/receiver device
and is not yet proven to control the IR illuminator.

The currently exported GPIOs 33, 38, and 46 must not be toggled blindly. A
safe implementation plan is:

1. Locate stock day/night and IR-cut control calls in `home.bin` and
   `/home/web/ipc`.
2. Identify the IR-cut filter and illuminator interfaces.
3. Determine whether control uses GPIO, PWM, a kernel driver, or another
   firmware device.
4. Add read-only day/night status detection.
5. Add explicit day, night, and auto controls.
6. Expose `YI_HACK_NIGHT_MODE=AUTO` and web controls only after pin behavior is
   verified.

Candidate GPIO testing must be reversible and isolated because the pins may
   affect the sensor, status LEDs, Wi-Fi, or boot behavior.

The first read-only IR probe is `sd/test/v2/scripts/ir_probe.sh`. Live
inspection found:

- Stock IPC strings for `app_hal_set_daynight_mode`, `set ircut`,
  `EMI_setIRLightBrightness`, and `EMI_setGpioGroupVal`.
- Stock configuration fields `lightmode`, `isdaymode`, and `daynightMode`.
- GPIOs 33, 38, and 46 are already assigned to the red, blue, and green
  status LEDs by the modified startup.
- `/dev/amb_iris` and the platform `e8006000.ir` device exist, but neither has
  yet been proven to control the IR illuminator.
- No standalone IR-light command or exposed IR GPIO mapping has been found.

The probe changes no hardware state. Actual day/night control remains blocked
until the stock GPIO/PWM mapping is recovered from the stripped IPC path or
verified through a controlled stock-runtime test.

### Recovered stock IR mappings

Static analysis of `/tmp/camera-ipc` recovered the following interfaces
without invoking them:

- `fcn.00319774` (`EMI_setIRLightBrightness`) formats the requested integer
  as decimal text and writes it to
  `/sys/class/backlight/0.pwm_bl/brightness`.
- The live camera reports `max_brightness=255` and current brightness `0`.
- `fcn.00319a8c` (`set ircut` path) opens `/dev/emd` and calls ioctl
  `0x400464c9`, equivalent to `_IOW('d', 0xc9, 4)` under the Linux ioctl
  encoding.
- The ioctl argument buffer contains a selector word and a value byte. The
  stock IR-cut sequence uses selector values `0x18` and `0x19`, changes them
  in opposite directions, waits approximately 150 ms, and then clears both
  selectors. This is a driver-level group interface; `0x18` and `0x19` must
  not be treated as physical GPIO numbers.
- The stock IR sensor reader opens
  `/sys/devices/e8000000.apb/e801d000.adc/adcsys` and reads a fixed binary
  record. The live file currently returns NUL bytes, so its value format and
  day/night threshold remain unresolved.

These findings are enough to build a carefully gated control helper, but not
enough to claim the IR-cut polarity or sensor threshold. No recovered ioctl or
PWM interface has been invoked by the modified firmware.

### Temporary official-startup runtime check

For one controlled reboot, `YI_HACK_STARTUP_MODE=OFFICIAL` was selected after
backing up the live configuration and startup files under
`/sdcard/test/logs/ir-stock/`. The stock stack came up with:

- `/home/web/ipc -w` running alongside `/home/web/show_stack`;
- an open `/dev/emd` descriptor;
- an open `ipc.config` descriptor;
- ALSA capture and playback descriptors;
- a dedicated `ivs_daynight_thr` thread;
- GPIO value descriptors for 24, 25, 33, 38, 46, 92, and 100.

The official stack did not expose a standalone day/night command or a readable
sensor value during this pass. The sensor sysfs node still returned NUL bytes,
and the stored values remained `lightmode=0`, `isdaymode=0`, and
`daynightMode=0`. The camera was restored to `YI_HACK_STARTUP_MODE=MODIFIED`
and rebooted; the video RTSP server and separate audio RTSP server are
running again.

The recovered illuminator path is implemented in
`sd/test/v2/scripts/ir_light.sh`. It reads the kernel-reported maximum,
accepts `status`, `off`, `on`, or a numeric brightness, writes the validated
value to `/sys/class/backlight/0.pwm_bl/brightness`, and verifies readback.
It is intentionally manual and disabled from startup. It does not invoke the
unresolved IR-cut ioctl or automatic day/night detection.

The standalone controller implementation is in `sd/test/v2/ir/ir_controller.c`
with a camera-compatible build file in `sd/test/v2/ir/Makefile`. It provides
manual `day`, `night`, and guarded `auto` modes, validates the PWM range,
serializes access with `/tmp/yi-hack-ir-controller.lock`, and uses the
recovered `/dev/emd` ioctl sequence. Its day/night polarity defaults are
placeholders (`day=0`, `night=1`) and must be corrected after the optical
polarity test. AUTO refuses a zero threshold and is not suitable for use until
the sensor record offset and threshold have been measured.

### Deferred optical polarity test

The optical polarity test is intentionally deferred. It requires:

- a live video view and an active Telnet recovery path;
- a one-shot utility that invokes only the recovered `/dev/emd` ioctl;
- illuminator brightness forced to zero during the comparison;
- a normally lit scene with visible colors and a separate IR source, such as
  an IR remote or IR LED;
- testing both candidate selector values and restoring the original selector
  and brightness after each test.

The test result must be based on the image response and IR visibility, not on
ioctl success. The night position should remove the IR-cut filter and admit
IR, while the day position should reject IR and preserve visible-light color.
Until this test and sensor sampling are complete, the controller is not
deployed, not started by modified startup, and AUTO mode must remain disabled.
but the negotiated hardware parameters and PCM read are valid; the binary is
not part of the runtime startup path.

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
