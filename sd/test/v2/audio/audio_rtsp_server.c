#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
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
    INPUT_FRAMES = 960,
    RTP_PAYLOAD = 8,
    RTP_CLOCK = 8000,
    RTSP_PORT = 8555,
    RTP_PORT = 8556,
    RTP_HEADER_SIZE = 12,
    RTSP_BUFFER_SIZE = 4096
};

static volatile sig_atomic_t running = 1;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static unsigned char linear_to_alaw(int sample)
{
    int sign = sample < 0 ? 0x80 : 0;
    int exponent;
    int mantissa;
    unsigned char value;

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

static int configure_capture(snd_pcm_t **pcm_out)
{
    snd_pcm_t *pcm = NULL;
    snd_pcm_hw_params_t *params;
    size_t size;
    int error;

    error = snd_pcm_open(&pcm, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (error < 0) {
        fprintf(stderr, "snd_pcm_open: %s (%d)\n", snd_strerror(error), error);
        return -1;
    }
    size = snd_pcm_hw_params_sizeof();
    params = malloc(size);
    if (params == NULL) {
        snd_pcm_close(pcm);
        return -1;
    }
    memset(params, 0, size);
#define ALSA_CALL(call) \
    do { \
        error = (call); \
        if (error < 0) { \
            fprintf(stderr, "ALSA setup: %s (%d)\n", snd_strerror(error), error); \
            free(params); \
            snd_pcm_close(pcm); \
            return -1; \
        } \
    } while (0)
    ALSA_CALL(snd_pcm_hw_params_any(pcm, params));
    ALSA_CALL(snd_pcm_hw_params_set_access(pcm, params,
                                            SND_PCM_ACCESS_RW_INTERLEAVED));
    ALSA_CALL(snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE));
    ALSA_CALL(snd_pcm_hw_params_set_channels(pcm, params, 2));
    ALSA_CALL(snd_pcm_hw_params_set_rate(pcm, params, 48000, 0));
    ALSA_CALL(snd_pcm_hw_params(pcm, params));
    free(params);
#undef ALSA_CALL
    if (snd_pcm_prepare(pcm) < 0) {
        snd_pcm_close(pcm);
        return -1;
    }
    *pcm_out = pcm;
    return 0;
}

static int send_rtsp(int client, const char *data)
{
    size_t length = strlen(data);
    return send(client, data, length, 0) == (ssize_t)length ? 0 : -1;
}

static int parse_client_port(const char *request, unsigned short *port)
{
    const char *transport = strstr(request, "client_port=");
    unsigned int value;

    if (transport == NULL || sscanf(transport, "client_port=%u", &value) != 1 ||
        value > 65535)
        return -1;
    *port = (unsigned short)value;
    return 0;
}

static int handle_rtsp_request(int client, const char *request,
                               unsigned short *rtp_port, int *playing)
{
    const char *cseq = strstr(request, "CSeq:");
    unsigned int sequence = cseq == NULL ? 1 : (unsigned int)strtoul(cseq + 5, NULL, 10);
    char response[2048];

    if (strncmp(request, "OPTIONS ", 8) == 0) {
        snprintf(response, sizeof(response),
                 "RTSP/1.0 200 OK\r\nCSeq: %u\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n",
                 sequence);
    } else if (strncmp(request, "DESCRIBE ", 9) == 0) {
        static const char sdp[] =
            "v=0\r\n"
            "o=- 0 0 IN IP4 0.0.0.0\r\n"
            "s=Yi Hack PCMA audio\r\n"
            "t=0 0\r\n"
            "a=control:*\r\n"
            "m=audio 0 RTP/AVP 8\r\n"
            "a=rtpmap:8 PCMA/8000\r\n"
            "a=control:track1\r\n";
        snprintf(response, sizeof(response),
                 "RTSP/1.0 200 OK\r\nCSeq: %u\r\nContent-Base: rtsp://0.0.0.0:%d/audio/\r\n"
                 "Content-Type: application/sdp\r\nContent-Length: %u\r\n\r\n%s",
                 sequence, RTSP_PORT, (unsigned int)strlen(sdp), sdp);
    } else if (strncmp(request, "SETUP ", 6) == 0) {
        if (parse_client_port(request, rtp_port) != 0)
            snprintf(response, sizeof(response),
                     "RTSP/1.0 461 Unsupported Transport\r\nCSeq: %u\r\n\r\n", sequence);
        else
            snprintf(response, sizeof(response),
                     "RTSP/1.0 200 OK\r\nCSeq: %u\r\nTransport: RTP/AVP;unicast;"
                     "client_port=%u-%u;server_port=8556-8557\r\nSession: 1\r\n\r\n",
                     sequence, *rtp_port, (unsigned int)(*rtp_port + 1));
    } else if (strncmp(request, "PLAY ", 5) == 0) {
        *playing = 1;
        snprintf(response, sizeof(response),
                 "RTSP/1.0 200 OK\r\nCSeq: %u\r\nSession: 1\r\nRTP-Info: url=track1\r\n\r\n",
                 sequence);
    } else if (strncmp(request, "TEARDOWN ", 9) == 0) {
        *playing = 0;
        snprintf(response, sizeof(response),
                 "RTSP/1.0 200 OK\r\nCSeq: %u\r\nSession: 1\r\n\r\n", sequence);
    } else {
        snprintf(response, sizeof(response),
                 "RTSP/1.0 501 Not Implemented\r\nCSeq: %u\r\n\r\n", sequence);
    }
    return send_rtsp(client, response);
}

int main(void)
{
    int listener;
    int client = -1;
    int rtp_socket = -1;
    int playing = 0;
    unsigned short client_port = 0;
    unsigned short sequence = 0;
    unsigned int timestamp = 0;
    snd_pcm_t *pcm = NULL;
    struct sockaddr_in address;
    struct sockaddr_in destination;
    char request[RTSP_BUFFER_SIZE];
    size_t request_length = 0;
    short samples[INPUT_FRAMES * 2];
    unsigned char packet[RTP_HEADER_SIZE + 160];

    signal(SIGTERM, stop_handler);
    signal(SIGINT, stop_handler);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return 1;
    {
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(RTSP_PORT);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        close(listener);
        return 1;
    }
    fprintf(stderr, "audio RTSP server listening on %d\n", RTSP_PORT);

    while (running) {
        fd_set read_set;
        struct timeval timeout;
        int highest = listener;

        FD_ZERO(&read_set);
        FD_SET(listener, &read_set);
        if (client >= 0) {
            FD_SET(client, &read_set);
            if (client > highest)
                highest = client;
        }
        timeout.tv_sec = 0;
        timeout.tv_usec = 20000;
        if (select(highest + 1, &read_set, NULL, NULL, &timeout) < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (FD_ISSET(listener, &read_set)) {
            if (client >= 0)
                close(client);
            client = accept(listener, NULL, NULL);
            request_length = 0;
            playing = 0;
            client_port = 0;
        }
        if (client >= 0 && FD_ISSET(client, &read_set)) {
            ssize_t received = recv(client, request + request_length,
                                    sizeof(request) - request_length - 1, 0);
            if (received <= 0) {
                close(client);
                client = -1;
                playing = 0;
                request_length = 0;
            } else {
                request_length += (size_t)received;
                request[request_length] = '\0';
                if (strstr(request, "\r\n\r\n") != NULL) {
                    if (handle_rtsp_request(client, request, &client_port, &playing) != 0) {
                        close(client);
                        client = -1;
                        playing = 0;
                    }
                    request_length = 0;
                }
            }
        }
        if (client >= 0 && playing && client_port != 0) {
            if (pcm == NULL && configure_capture(&pcm) != 0) {
                playing = 0;
                continue;
            }
            if (rtp_socket < 0)
                rtp_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (rtp_socket < 0)
                break;
            if (rtp_socket >= 0 && sequence == 0) {
                struct sockaddr_in rtp_address;
                memset(&rtp_address, 0, sizeof(rtp_address));
                rtp_address.sin_family = AF_INET;
                rtp_address.sin_addr.s_addr = htonl(INADDR_ANY);
                rtp_address.sin_port = htons(RTP_PORT);
                if (bind(rtp_socket, (struct sockaddr *)&rtp_address,
                         sizeof(rtp_address)) != 0) {
                    close(rtp_socket);
                    rtp_socket = -1;
                    playing = 0;
                    continue;
                }
            }
            memset(&destination, 0, sizeof(destination));
            destination.sin_family = AF_INET;
            destination.sin_port = htons(client_port);
            destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (getpeername(client, (struct sockaddr *)&destination,
                            &(socklen_t){sizeof(destination)}) == 0) {
                destination.sin_port = htons(client_port);
                if (snd_pcm_readi(pcm, samples, INPUT_FRAMES) == INPUT_FRAMES) {
                    packet[0] = 0x80;
                    packet[1] = RTP_PAYLOAD;
                    packet[2] = (unsigned char)(sequence >> 8);
                    packet[3] = (unsigned char)sequence++;
                    packet[4] = (unsigned char)(timestamp >> 24);
                    packet[5] = (unsigned char)(timestamp >> 16);
                    packet[6] = (unsigned char)(timestamp >> 8);
                    packet[7] = (unsigned char)timestamp;
                    packet[8] = packet[9] = packet[10] = packet[11] = 0;
                    for (size_t i = 0; i < 160; ++i) {
                        int sample = ((int)samples[i * 12] +
                                      (int)samples[(i * 12) + 1]) / 2;
                        packet[RTP_HEADER_SIZE + i] = linear_to_alaw(sample);
                    }
                    sendto(rtp_socket, packet, sizeof(packet), 0,
                           (struct sockaddr *)&destination, sizeof(destination));
                    timestamp += 160;
                } else {
                    snd_pcm_prepare(pcm);
                }
            }
        }
    }
    if (pcm != NULL)
        snd_pcm_close(pcm);
    if (rtp_socket >= 0)
        close(rtp_socket);
    if (client >= 0)
        close(client);
    close(listener);
    return 0;
}
