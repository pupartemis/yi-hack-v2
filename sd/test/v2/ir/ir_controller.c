#include <errno.h>
#include <fcntl.h>

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define IR_BRIGHTNESS "/sys/class/backlight/0.pwm_bl/brightness"
#define IR_MAX_BRIGHTNESS "/sys/class/backlight/0.pwm_bl/max_brightness"
#define IR_SENSOR "/sys/devices/e8000000.apb/e801d000.adc/adcsys"
#define IRCUT_DEVICE "/dev/emd"
#define LOCK_FILE "/tmp/yi-hack-ir-controller.lock"
#define IRCUT_IOCTL 0x400464c9UL
#define IRCUT_GROUP_A 0x18U
#define IRCUT_GROUP_B 0x19U
#define SWITCH_DELAY_US 150000U

struct ircut_value {
   uint32_t group;
   uint8_t value;
   uint8_t reserved[3];
};

struct controller_config {
   unsigned day_brightness;
   unsigned night_brightness;
   unsigned day_ircut;
   unsigned night_ircut;
   unsigned threshold;
   unsigned sensor_offset;
   unsigned interval;
};

static volatile sig_atomic_t running = 1;

static void stop_controller(int signal_number)
{
   (void)signal_number;
   running = 0;
}

static int write_number(const char *path, unsigned value)
{
   char buffer[16];
   int fd;
   int length;

   fd = open(path, O_WRONLY);
   if (fd < 0)
      return -1;
   length = snprintf(buffer, sizeof(buffer), "%u\n", value);
   if (write(fd, buffer, (size_t)length) != length) {
      int saved_errno = errno;
      close(fd);
      errno = saved_errno;
      return -1;
   }
   return close(fd);
}

static int read_number(const char *path, unsigned *value)
{
   char buffer[32];
   char *end;
   int fd;
   ssize_t length;
   unsigned long parsed;

   fd = open(path, O_RDONLY);
   if (fd < 0)
      return -1;
   length = read(fd, buffer, sizeof(buffer) - 1);
   close(fd);
   if (length <= 0)
      return -1;
   buffer[length] = '\0';
   errno = 0;
   parsed = strtoul(buffer, &end, 10);
   if (errno != 0 || end == buffer || parsed > 255)
      return -1;
   *value = (unsigned)parsed;
   return 0;
}

static int set_brightness(unsigned value)
{
   unsigned maximum = 255;

   if (read_number(IR_MAX_BRIGHTNESS, &maximum) < 0)
      maximum = 255;
   if (value > maximum) {
      fprintf(stderr, "brightness %u exceeds maximum %u\n", value, maximum);
      return -1;
   }
   if (write_number(IR_BRIGHTNESS, value) < 0) {
      perror("write IR brightness");
      return -1;
   }
   return 0;
}

static int set_ircut_group(int fd, unsigned group, unsigned value)
{
   struct ircut_value request;

   request.group = group;
   request.value = (uint8_t)value;
   memset(request.reserved, 0, sizeof(request.reserved));
   if (ioctl(fd, IRCUT_IOCTL, &request) < 0)
      return -1;
   return 0;
}

static int set_ircut(unsigned mode)
{
   int fd;
   unsigned first_value;
   unsigned second_value;
   int result = -1;

   fd = open(IRCUT_DEVICE, O_RDWR);
   if (fd < 0) {
      perror("open IR-cut device");
      return -1;
   }

   /*
    * The stock sequence is break-before-make. The mode argument is kept
    * configurable because physical day/night polarity is not verified yet.
    */
   first_value = mode ? 1U : 0U;
   second_value = mode ? 0U : 1U;
   if (set_ircut_group(fd, IRCUT_GROUP_A, first_value) < 0)
      goto out;
   if (set_ircut_group(fd, IRCUT_GROUP_B, second_value) < 0)
      goto out;
   usleep(SWITCH_DELAY_US);
   if (set_ircut_group(fd, IRCUT_GROUP_A, 0) < 0)
      goto out;
   if (set_ircut_group(fd, IRCUT_GROUP_B, 0) < 0)
      goto out;
   result = 0;
out:
   if (result < 0)
      perror("IR-cut ioctl");
   close(fd);
   return result;
}

static int apply_mode(const char *name, const struct controller_config *config)
{
   unsigned ircut_value;
   unsigned brightness;

   if (strcmp(name, "day") == 0) {
      ircut_value = config->day_ircut;
      brightness = config->day_brightness;
   } else if (strcmp(name, "night") == 0) {
      ircut_value = config->night_ircut;
      brightness = config->night_brightness;
   } else {
      fprintf(stderr, "unsupported mode: %s\n", name);
      return -1;
   }

   if (set_brightness(0) < 0)
      return -1;
   if (set_ircut(ircut_value) < 0)
      return -1;
   if (set_brightness(brightness) < 0)
      return -1;
   printf("mode=%s brightness=%u ircut=%u\n", name, brightness, ircut_value);
   return 0;
}

static int read_sensor(const struct controller_config *config, unsigned *value)
{
   uint8_t data[64];
   int fd;
   ssize_t length;

   if (config->sensor_offset + sizeof(uint32_t) > sizeof(data))
      return -1;
   fd = open(IR_SENSOR, O_RDONLY);
   if (fd < 0)
      return -1;
   length = read(fd, data, sizeof(data));
   close(fd);
   if (length < (ssize_t)(config->sensor_offset + sizeof(uint32_t)))
      return -1;
   *value = (unsigned)data[config->sensor_offset] |
            ((unsigned)data[config->sensor_offset + 1] << 8) |
            ((unsigned)data[config->sensor_offset + 2] << 16) |
            ((unsigned)data[config->sensor_offset + 3] << 24);
   return 0;
}

static int run_auto(const struct controller_config *config)
{
   unsigned sensor;
   unsigned current = 0;
   int have_mode = 0;

   if (config->threshold == 0) {
      fprintf(stderr, "AUTO requires a verified non-zero sensor threshold\n");
      return -1;
   }
   while (running) {
      if (read_sensor(config, &sensor) < 0) {
         fprintf(stderr, "unable to read the day/night sensor\n");
         return -1;
      }
      if (!have_mode) {
         current = sensor < config->threshold ? 0U : 1U;
         if (apply_mode(current == 0 ? "night" : "day", config) < 0)
            return -1;
      } else if (current == 1U && sensor < config->threshold) {
         if (apply_mode("night", config) < 0)
            return -1;
         current = 0U;
      } else if (current == 0U && sensor >= config->threshold) {
         if (apply_mode("day", config) < 0)
            return -1;
         current = 1U;
      }
      have_mode = 1;
      sleep(config->interval);
   }
   return 0;
}

static int lock_controller(void)
{
   int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0600);
   if (fd < 0)
      return -1;
   if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
      close(fd);
      errno = EBUSY;
      return -1;
   }
   return fd;
}

static void usage(const char *program)
{
   fprintf(stderr,
           "Usage: %s status | day | night | auto [threshold offset interval]\n",
           program);
}

int main(int argc, char **argv)
{
   struct controller_config config = {
      0, 255, 0, 1, 0, 0, 5
   };
   int lock_fd;
   int result;

   if (argc < 2) {
      usage(argv[0]);
      return 2;
   }
   lock_fd = lock_controller();
   if (lock_fd < 0) {
      perror("IR controller already running");
      return 1;
   }
   signal(SIGTERM, stop_controller);
   signal(SIGINT, stop_controller);

   if (strcmp(argv[1], "status") == 0) {
      unsigned brightness;
      result = read_number(IR_BRIGHTNESS, &brightness);
      if (result == 0)
         printf("brightness=%u\n", brightness);
   } else if (strcmp(argv[1], "auto") == 0) {
      if (argc >= 3)
         config.threshold = (unsigned)strtoul(argv[2], NULL, 10);
      if (argc >= 4)
         config.sensor_offset = (unsigned)strtoul(argv[3], NULL, 10);
      if (argc >= 5)
         config.interval = (unsigned)strtoul(argv[4], NULL, 10);
      result = run_auto(&config);
   } else {
      result = apply_mode(argv[1], &config);
   }
   close(lock_fd);
   return result == 0 ? 0 : 1;
}
