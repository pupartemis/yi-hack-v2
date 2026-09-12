CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror
LDFLAGS ?=
LDLIBS ?= -lasound

all: alsa_probe pcma_producer audio_rtsp_server

alsa_probe: alsa_probe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

pcma_producer: pcma_producer.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

audio_rtsp_server: audio_rtsp_server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -nostartfiles -o $@ audio_start.S $< $(LDLIBS)

clean:
	rm -f alsa_probe pcma_producer audio_rtsp_server
