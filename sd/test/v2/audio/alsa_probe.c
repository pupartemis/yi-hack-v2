typedef unsigned long size_t;
#define NULL ((void *)0)
extern int printf(const char *format, ...);
extern int puts(const char *string);
extern void *malloc(size_t size);
extern void free(void *pointer);
extern void probe_exit(void);

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
extern int snd_pcm_hw_params_get_rate(const snd_pcm_hw_params_t *params,
                                      unsigned int *rate, int *dir);
extern int snd_pcm_hw_params_get_channels(const snd_pcm_hw_params_t *params,
                                          unsigned int *channels);
extern int snd_pcm_hw_params_get_format(const snd_pcm_hw_params_t *params,
                                        int *format);
extern int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);

enum {
    SND_PCM_STREAM_CAPTURE = 1,
    SND_PCM_ACCESS_RW_INTERLEAVED = 3,
    SND_PCM_FORMAT_S16_LE = 2
};

static int report_error(const char *operation, int error)
{
    printf("%s failed: %s (%d)\n", operation, snd_strerror(error), error);
    return 1;
}

static int probe_device(const char *device, unsigned int channels)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *params = NULL;
    unsigned int actual_rate = 48000;
    unsigned int actual_channels = 0;
    int actual_format = -1;
    int direction = 0;
    int error;
    short samples[480];
    snd_pcm_sframes_t frames;

    printf("=== %s, S16_LE, 48000 Hz, %u channel(s) ===\n", device, channels);
    error = snd_pcm_open(&pcm, device, SND_PCM_STREAM_CAPTURE, 0);
    if (error < 0)
        return report_error("snd_pcm_open", error);

    params = malloc(snd_pcm_hw_params_sizeof());
    if (params == NULL) {
        printf("cannot allocate ALSA hw_params\n");
        snd_pcm_close(pcm);
        return 1;
    }
    {
        size_t index;
        for (index = 0; index < snd_pcm_hw_params_sizeof(); ++index)
            ((unsigned char *)params)[index] = 0;
    }

#define ALSA_CALL(name, call) \
    do { \
        error = (call); \
        if (error < 0) { \
            int result = report_error(name, error); \
            free(params); \
            snd_pcm_close(pcm); \
            return result; \
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
              snd_pcm_hw_params_set_channels(pcm, params, channels));
    ALSA_CALL("snd_pcm_hw_params_set_rate",
              snd_pcm_hw_params_set_rate(pcm, params, actual_rate, 0));
    ALSA_CALL("snd_pcm_hw_params", snd_pcm_hw_params(pcm, params));

    ALSA_CALL("snd_pcm_hw_params_get_rate",
              snd_pcm_hw_params_get_rate(params, &actual_rate, &direction));
    ALSA_CALL("snd_pcm_hw_params_get_channels",
              snd_pcm_hw_params_get_channels(params, &actual_channels));
    ALSA_CALL("snd_pcm_hw_params_get_format",
              snd_pcm_hw_params_get_format(params, &actual_format));

    printf("accepted: format=%d rate=%u channels=%u\n",
           actual_format, actual_rate, actual_channels);
    ALSA_CALL("snd_pcm_prepare", snd_pcm_prepare(pcm));
    frames = snd_pcm_readi(pcm, samples, sizeof(samples) / sizeof(samples[0]));
    if (frames < 0) {
        int result = report_error("snd_pcm_readi", (int)frames);
        free(params);
        snd_pcm_close(pcm);
        return result;
    }
    printf("read %ld frame(s), first sample=%d\n", (long)frames, samples[0]);
    return 0;
}

int main(void)
{
    int result;

    puts("Yi Hack v2 H21 ALSA capture probe");
    puts("Expected firmware profile: S16_LE, 48000 Hz, mono or stereo");
    result = probe_device("hw:0,0", 1);
    if (probe_device("hw:0,0", 2) == 0)
        probe_exit();
    return result ? 1 : 0;
}
