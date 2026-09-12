#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct snd_pcm snd_pcm_t;
typedef struct snd_pcm_hw_params snd_pcm_hw_params_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

extern const char *snd_strerror(int error);
extern int snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream, int mode);
extern int snd_pcm_close(snd_pcm_t *pcm);
extern int snd_pcm_prepare(snd_pcm_t *pcm);
extern snd_pcm_sframes_t snd_pcm_readi(snd_pcm_t *pcm, void *buffer,
                                       snd_pcm_uframes_t size);
extern size_t snd_pcm_hw_params_sizeof(void);
extern int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
extern int snd_pcm_hw_params_set_access(snd_pcm_t *pcm,
                                        snd_pcm_hw_params_t *params, int access);
extern int snd_pcm_hw_params_set_format(snd_pcm_t *pcm,
                                        snd_pcm_hw_params_t *params, int format);
extern int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm,
                                          snd_pcm_hw_params_t *params,
                                          unsigned int channels);
extern int snd_pcm_hw_params_set_rate(snd_pcm_t *pcm,
                                      snd_pcm_hw_params_t *params,
                                      unsigned int rate, int dir);
extern int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

enum {
    SND_PCM_STREAM_CAPTURE = 1,
    SND_PCM_ACCESS_RW_INTERLEAVED = 3,
    SND_PCM_FORMAT_S16_LE = 2,
    INPUT_RATE = 48000,
    INPUT_CHANNELS = 2,
    OUTPUT_RATE = 8000,
    OUTPUT_FRAMES = 160,
    INPUT_FRAMES = 960
};

static volatile sig_atomic_t running = 1;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static unsigned char linear_to_alaw(int sample)
{
    int sign;
    int exponent;
    int mantissa;
    unsigned char value;

    sign = (sample < 0) ? 0x80 : 0;
    if (sample < 0)
        sample = -sample;
    if (sample > 32635)
        sample = 32635;

    if (sample >= 256) {
        exponent = 7;
        while ((sample & (0x4000 >> (7 - exponent))) == 0)
            --exponent;
        mantissa = (sample >> (exponent + 3)) & 0x0f;
        value = (unsigned char)(sign | (exponent << 4) | mantissa);
    } else {
        value = (unsigned char)(sign | (sample >> 4));
    }
    return value ^ 0x55;
}

static int write_all(FILE *output, const unsigned char *buffer, size_t length)
{
    while (length != 0) {
        size_t written = fwrite(buffer, 1, length, output);
        if (written == 0) {
            if (ferror(output))
                return -1;
            errno = EIO;
            return -1;
        }
        buffer += written;
        length -= written;
    }
    return fflush(output);
}

static int configure_capture(snd_pcm_t **pcm_out)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *params = NULL;
    int error;
    size_t params_size;

    error = snd_pcm_open(&pcm, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (error < 0) {
        fprintf(stderr, "snd_pcm_open(hw:0,0): %s (%d)\n",
                snd_strerror(error), error);
        return -1;
    }

    params_size = snd_pcm_hw_params_sizeof();
    params = malloc(params_size);
    if (params == NULL) {
        fprintf(stderr, "cannot allocate ALSA hw_params\n");
        snd_pcm_close(pcm);
        return -1;
    }
    memset(params, 0, params_size);

#define ALSA_CALL(name, call) \
    do { \
        error = (call); \
        if (error < 0) { \
            fprintf(stderr, "%s: %s (%d)\n", name, snd_strerror(error), error); \
            free(params); \
            snd_pcm_close(pcm); \
            return -1; \
        } \
    } while (0)

    ALSA_CALL("snd_pcm_hw_params_any", snd_pcm_hw_params_any(pcm, params));
    ALSA_CALL("snd_pcm_hw_params_set_access",
              snd_pcm_hw_params_set_access(
                  pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED));
    ALSA_CALL("snd_pcm_hw_params_set_format",
              snd_pcm_hw_params_set_format(
                  pcm, params, SND_PCM_FORMAT_S16_LE));
    ALSA_CALL("snd_pcm_hw_params_set_channels",
              snd_pcm_hw_params_set_channels(pcm, params, INPUT_CHANNELS));
    ALSA_CALL("snd_pcm_hw_params_set_rate",
              snd_pcm_hw_params_set_rate(pcm, params, INPUT_RATE, 0));
    ALSA_CALL("snd_pcm_hw_params", snd_pcm_hw_params(pcm, params));
    free(params);

    *pcm_out = pcm;
    return 0;

#undef ALSA_CALL
}

int main(int argc, char **argv)
{
    snd_pcm_t *pcm = NULL;
    FILE *output = stdout;
    short samples[INPUT_FRAMES * INPUT_CHANNELS];
    unsigned char encoded[OUTPUT_FRAMES];
    size_t frames_read;
    int error;

    if (argc == 3 && strcmp(argv[1], "--output") == 0) {
        output = fopen(argv[2], "wb");
        if (output == NULL) {
            fprintf(stderr, "cannot open output %s: %s\n", argv[2],
                    strerror(errno));
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--output FILE]\n", argv[0]);
        return 2;
    }

    if (signal(SIGTERM, stop_handler) == SIG_ERR ||
        signal(SIGINT, stop_handler) == SIG_ERR) {
        fprintf(stderr, "cannot install signal handlers\n");
        if (output != stdout)
            fclose(output);
        return 1;
    }

    if (configure_capture(&pcm) != 0) {
        if (output != stdout)
            fclose(output);
        return 1;
    }
    if (snd_pcm_prepare(pcm) < 0) {
        fprintf(stderr, "snd_pcm_prepare failed\n");
        snd_pcm_close(pcm);
        if (output != stdout)
            fclose(output);
        return 1;
    }

    fprintf(stderr, "capturing 48 kHz stereo PCM and emitting 8 kHz PCMA\n");
    while (running) {
        frames_read = 0;
        while (frames_read < INPUT_FRAMES && running) {
            snd_pcm_sframes_t frames = snd_pcm_readi(
                pcm, samples + (frames_read * INPUT_CHANNELS),
                INPUT_FRAMES - frames_read);
            if (frames == -EPIPE) {
                error = snd_pcm_prepare(pcm);
                if (error < 0) {
                    fprintf(stderr, "ALSA XRUN recovery failed: %s (%d)\n",
                            snd_strerror(error), error);
                    running = 0;
                    break;
                }
                continue;
            }
            if (frames < 0) {
                fprintf(stderr, "snd_pcm_readi: %s (%ld)\n",
                        snd_strerror((int)frames), (long)frames);
                running = 0;
                break;
            }
            if (frames == 0)
                continue;
            frames_read += (size_t)frames;
        }
        if (!running || frames_read != INPUT_FRAMES)
            break;

        for (size_t output_index = 0; output_index < OUTPUT_FRAMES;
             ++output_index) {
            size_t input_index = output_index * 6;
            int left = samples[input_index * 2];
            int right = samples[(input_index * 2) + 1];
            encoded[output_index] = linear_to_alaw((left + right) / 2);
        }
        if (write_all(output, encoded, sizeof(encoded)) != 0) {
            fprintf(stderr, "audio output failed: %s\n", strerror(errno));
            running = 0;
        }
    }

    snd_pcm_close(pcm);
    if (output != stdout && fclose(output) != 0)
        return 1;
    return 0;
}
