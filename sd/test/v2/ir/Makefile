CROSS_COMPILE ?= arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror
LDFLAGS ?= -nostartfiles

all: ir_controller

ir_controller: ir_controller.c ../audio/audio_start.S
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ ../audio/audio_start.S ir_controller.c

clean:
	rm -f ir_controller
