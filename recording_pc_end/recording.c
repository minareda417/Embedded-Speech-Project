// /*
//  * record.c — Receives raw audio from ATmega32A over UART, saves as WAV
//  *
//  * Linux/Mac:
//  *   gcc -o record record.c
//  *   ./record /dev/ttyUSB0 output.wav
//  *
//  * Windows (MinGW):
//  *   gcc -o record.exe record.c
//  *   record.exe COM3 output.wav
//  *
//  * Press PD2 on ATmega to start recording.
//  * Press PD2 again to stop — WAV file is saved.
//  * Press Ctrl+C to quit anytime.
//  */

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdint.h>
// #include <signal.h>

// #ifdef _WIN32
//   #include <windows.h>
// #else
//   #include <fcntl.h>
//   #include <unistd.h>
//   #include <termios.h>
// #endif

// /* ── Config ───────────────────────────────────────────────────────── */
// #define SAMPLE_RATE   8000
// #define CHANNELS      1
// #define BITS_PER_SAMPLE 8
// #define READ_BUF_SIZE 4096       /* bytes read per iteration */
// #define MAX_AUDIO_MB  60         /* safety cap on file size  */
// #define MAX_SAMPLES   (MAX_AUDIO_MB * 1024 * 1024)

// #define START_SEN     "##START##"
// #define STOP_SEN      "##STOP##"
// #define START_LEN     9
// #define STOP_LEN      8

// /* ── WAV writer ───────────────────────────────────────────────────── */
// static void write_u16_le(FILE *f, uint16_t v)
// {
//     fputc(v & 0xFF, f);
//     fputc((v >> 8) & 0xFF, f);
// }

// static void write_u32_le(FILE *f, uint32_t v)
// {
//     fputc(v & 0xFF, f);
//     fputc((v >> 8) & 0xFF, f);
//     fputc((v >> 16) & 0xFF, f);
//     fputc((v >> 24) & 0xFF, f);
// }

// static void write_wav(const char *path, uint8_t *samples, uint32_t count)
// {
//     FILE *f = fopen(path, "wb");
//     if (!f) { perror("fopen wav"); return; }

//     uint32_t data_size  = count;
//     uint32_t chunk_size = 36 + data_size;

//     /* RIFF header */
//     fputs("RIFF", f);
//     write_u32_le(f, chunk_size);
//     fputs("WAVE", f);

//     /* fmt chunk */
//     fputs("fmt ", f);
//     write_u32_le(f, 16);                    /* chunk size          */
//     write_u16_le(f, 1);                     /* PCM                 */
//     write_u16_le(f, CHANNELS);
//     write_u32_le(f, SAMPLE_RATE);
//     write_u32_le(f, SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8); /* byte rate */
//     write_u16_le(f, CHANNELS * BITS_PER_SAMPLE / 8);               /* block align */
//     write_u16_le(f, BITS_PER_SAMPLE);

//     /* data chunk */
//     fputs("data", f);
//     write_u32_le(f, data_size);
//     fwrite(samples, 1, count, f);
//     fclose(f);

//     float secs = (float)count / SAMPLE_RATE;
//     printf("\nSaved: %s  (%u bytes, %.2f seconds)\n", path, count, secs);
// }

// /* ── Serial port ──────────────────────────────────────────────────── */
// #ifdef _WIN32

// typedef HANDLE serial_t;

// static serial_t serial_open(const char *port)
// {
//     /* Windows needs "\\.\COM10" style for COM10+ */
//     char full[64];
//     snprintf(full, sizeof(full), "\\\\.\\%s", port);

//     HANDLE h = CreateFileA(full, GENERIC_READ | GENERIC_WRITE,
//                            0, NULL, OPEN_EXISTING, 0, NULL);
//     if (h == INVALID_HANDLE_VALUE) {
//         fprintf(stderr, "Cannot open %s (error %lu)\n", port, GetLastError());
//         exit(1);
//     }

//     DCB dcb = {0};
//     dcb.DCBlength = sizeof(dcb);
//     GetCommState(h, &dcb);
//     dcb.BaudRate = CBR_115200;
//     dcb.ByteSize = 8;
//     dcb.Parity   = NOPARITY;
//     dcb.StopBits = ONESTOPBIT;
//     SetCommState(h, &dcb);

//     COMMTIMEOUTS to = {0};
//     to.ReadIntervalTimeout         = 1;
//     to.ReadTotalTimeoutMultiplier  = 0;
//     to.ReadTotalTimeoutConstant    = 10;   /* 10 ms timeout */
//     SetCommTimeouts(h, &to);

//     return h;
// }

// static int serial_read(serial_t h, uint8_t *buf, int len)
// {
//     DWORD got = 0;
//     ReadFile(h, buf, len, &got, NULL);
//     return (int)got;
// }

// static void serial_close(serial_t h) { CloseHandle(h); }

// #else  /* Linux / Mac */

// typedef int serial_t;

// static serial_t serial_open(const char *port)
// {
//     int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
//     if (fd < 0) { perror("open serial"); exit(1); }

//     struct termios tty;
//     tcgetattr(fd, &tty);

//     cfsetispeed(&tty, B115200);
//     cfsetospeed(&tty, B115200);

//     tty.c_cflag  = CS8 | CREAD | CLOCAL;  /* 8-bit, no parity, 1 stop */
//     tty.c_iflag  = 0;
//     tty.c_oflag  = 0;
//     tty.c_lflag  = 0;
//     tty.c_cc[VMIN]  = 0;
//     tty.c_cc[VTIME] = 1;   /* 0.1 s read timeout */

//     tcsetattr(fd, TCSANOW, &tty);
//     tcflush(fd, TCIFLUSH);
//     return fd;
// }

// static int serial_read(serial_t fd, uint8_t *buf, int len)
// {
//     int n = read(fd, buf, len);
//     return n < 0 ? 0 : n;
// }

// static void serial_close(serial_t fd) { close(fd); }

// #endif

// /* ── Sentinel scanner ─────────────────────────────────────────────── */
// /*
//  * Searches haystack[0..len) for needle, returns index of first match
//  * or -1 if not found.
//  */
// static int find_sentinel(const uint8_t *hay, int len,
//                          const char *needle, int nlen)
// {
//     for (int i = 0; i <= len - nlen; i++) {
//         if (memcmp(hay + i, needle, nlen) == 0) return i;
//     }
//     return -1;
// }

// /* ── Signal handler ───────────────────────────────────────────────── */
// static volatile int quit = 0;
// static void on_sigint(int s) { (void)s; quit = 1; }

// /* ── Main ─────────────────────────────────────────────────────────── */
// int main(int argc, char *argv[])
// {
//     if (argc < 3) {
//         fprintf(stderr,
//             "Usage: %s <port> <output.wav>\n"
//             "  Linux: ./record /dev/ttyUSB0 out.wav\n"
//             "  Win:   record.exe COM3 out.wav\n", argv[0]);
//         return 1;
//     }

//     signal(SIGINT, on_sigint);

//     const char *port     = argv[1];
//     const char *wav_path = argv[2];

//     printf("Opening %s at 115200 baud...\n", port);
//     serial_t ser = serial_open(port);
//     printf("Connected. Press PD2 on ATmega to start recording.\n\n");

//     /* Audio buffer — allocated on heap */
//     uint8_t *audio = malloc(MAX_SAMPLES);
//     if (!audio) { fprintf(stderr, "Out of memory\n"); return 1; }

//     uint8_t  read_buf[READ_BUF_SIZE];
//     /* Overlap buffer: keeps tail of last chunk to catch sentinels
//        that span two reads */
//     uint8_t  overlap[32];
//     int      overlap_len = 0;

//     int      recording   = 0;
//     uint32_t sample_count = 0;
//     int      rec_num     = 0;
//     char     out_path[512];

//     while (!quit) {
//         int n = serial_read(ser, read_buf, sizeof(read_buf));
//         if (n <= 0) continue;

//         /* Combine overlap + new data into one contiguous block */
//         uint8_t combined[sizeof(overlap) + READ_BUF_SIZE];
//         memcpy(combined, overlap, overlap_len);
//         memcpy(combined + overlap_len, read_buf, n);
//         int combined_len = overlap_len + n;

//         if (!recording) {
//             /* Looking for ##START## */
//             int pos = find_sentinel(combined, combined_len,
//                                     START_SEN, START_LEN);
//             if (pos != -1) {
//                 recording    = 1;
//                 sample_count = 0;
//                 rec_num++;
//                 /* Build output filename */
//                 if (rec_num == 1) {
//                     snprintf(out_path, sizeof(out_path), "%s", wav_path);
//                 } else {
//                     /* strip .wav, add _N.wav */
//                     char base[512];
//                     strncpy(base, wav_path, sizeof(base));
//                     char *dot = strrchr(base, '.');
//                     if (dot) *dot = '\0';
//                     snprintf(out_path, sizeof(out_path), "%s_%d.wav",
//                              base, rec_num);
//                 }
//                 printf("[REC #%d] Recording... press PD2 to stop.\n", rec_num);
//                 /* Any bytes after the sentinel are audio */
//                 int audio_start = pos + START_LEN;
//                 int leftover    = combined_len - audio_start;
//                 if (leftover > 0 && sample_count + leftover <= MAX_SAMPLES) {
//                     memcpy(audio + sample_count,
//                            combined + audio_start, leftover);
//                     sample_count += leftover;
//                 }
//                 overlap_len = 0;
//             } else {
//                 /* Keep tail in overlap for next iteration */
//                 int keep = combined_len < (int)sizeof(overlap)
//                            ? combined_len : (int)sizeof(overlap);
//                 memcpy(overlap, combined + combined_len - keep, keep);
//                 overlap_len = keep;
//             }
//         } else {
//             /* Recording — look for ##STOP## */
//             int pos = find_sentinel(combined, combined_len,
//                                     STOP_SEN, STOP_LEN);
//             if (pos != -1) {
//                 /* Everything before sentinel is audio */
//                 if (sample_count + pos <= MAX_SAMPLES) {
//                     memcpy(audio + sample_count, combined, pos);
//                     sample_count += pos;
//                 }
//                 recording = 0;
//                 overlap_len = 0;
//                 printf("\r[REC #%d] Stopped.                          \n",
//                        rec_num);
//                 write_wav(out_path, audio, sample_count);
//                 printf("Press PD2 to record again, Ctrl+C to quit.\n\n");
//             } else {
//                 /* All audio, keep tail as overlap to catch split sentinel */
//                 int safe = combined_len - (int)sizeof(overlap);
//                 if (safe < 0) safe = 0;
//                 if (sample_count + safe <= MAX_SAMPLES) {
//                     memcpy(audio + sample_count, combined, safe);
//                     sample_count += safe;
//                 }
//                 /* Progress */
//                 float secs = (float)sample_count / SAMPLE_RATE;
//                 printf("\r  %.1f sec  (%u bytes)", secs, sample_count);
//                 fflush(stdout);
//                 /* Keep tail */
//                 int keep = combined_len - safe;
//                 memcpy(overlap, combined + safe, keep);
//                 overlap_len = keep;
//             }
//         }
//     }

//     /* Ctrl+C — save partial if recording */
//     if (recording && sample_count > 0) {
//         printf("\nSaving partial recording...\n");
//         write_wav(out_path, audio, sample_count);
//     }

//     free(audio);
//     serial_close(ser);
//     printf("Done.\n");
//     return 0;
// }

/*
 * record.c — Saves ATmega32A audio stream as WAV on Windows
 *
 * Pure Win32 API — no termios, no POSIX, compiles with MinGW or MSVC.
 *
 * Compile (MinGW / MSYS2):
 *   gcc -O2 -o record.exe record.c
 *
 * Run:
 *   record.exe COM3 output.wav
 *   record.exe COM3 output.wav 115200    (optional baud override)
 *
 * Then press PD2 on the ATmega to start/stop recording.
 * Ctrl+C saves any partial recording and exits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <windows.h>

/* ── Config ───────────────────────────────────────────────────────── */
#define SAMPLE_RATE      8000
#define CHANNELS         1
#define BITS_PER_SAMPLE  8
#define READ_BUF_SIZE    4096
#define MAX_SECONDS      3600          /* 1 hour max recording */
#define MAX_SAMPLES      ((uint32_t)SAMPLE_RATE * MAX_SECONDS)

#define START_SEN        "##START##"
#define STOP_SEN         "##STOP##"
#define START_LEN        9
#define STOP_LEN         8
#define OVERLAP_SIZE     32

/* ── WAV writer ───────────────────────────────────────────────────── */
static void write_u16(FILE *f, uint16_t v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
}

static void write_u32(FILE *f, uint32_t v)
{
    fputc( v        & 0xFF, f);
    fputc((v >>  8) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 24) & 0xFF, f);
}

static void save_wav(const char *path, uint8_t *buf, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); return; }

    fputs("RIFF", f);  write_u32(f, 36 + n);
    fputs("WAVE", f);
    fputs("fmt ", f);  write_u32(f, 16);
    write_u16(f, 1);                                       /* PCM        */
    write_u16(f, CHANNELS);
    write_u32(f, SAMPLE_RATE);
    write_u32(f, SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8);
    write_u16(f, CHANNELS * BITS_PER_SAMPLE / 8);
    write_u16(f, BITS_PER_SAMPLE);
    fputs("data", f);  write_u32(f, n);
    fwrite(buf, 1, n, f);
    fclose(f);

    printf("\nSaved: %s  (%lu bytes, %.2f sec)\n",
           path, (unsigned long)n, (float)n / SAMPLE_RATE);
}

/* ── Serial (Win32) ───────────────────────────────────────────────── */
static HANDLE serial_open(const char *port, DWORD baud)
{
    char full[32];
    /* Handle COM10+ which needs \\.\COMx format */
    snprintf(full, sizeof(full), "\\\\.\\%s", port);

    HANDLE h = CreateFileA(full,
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open %s — error %lu\n",
                port, GetLastError());
        fprintf(stderr, "Is the port name correct? Try COM3, COM4, etc.\n");
        exit(1);
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        fprintf(stderr, "GetCommState failed\n"); exit(1);
    }
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fParity  = FALSE;
    if (!SetCommState(h, &dcb)) {
        fprintf(stderr, "SetCommState failed\n"); exit(1);
    }

    /* Timeouts: return immediately with whatever is available */
    COMMTIMEOUTS to;
    to.ReadIntervalTimeout         = MAXDWORD;
    to.ReadTotalTimeoutMultiplier  = 0;
    to.ReadTotalTimeoutConstant    = 0;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant   = 50;
    SetCommTimeouts(h, &to);

    /* Increase driver receive buffer to avoid drops */
    SetupComm(h, 65536, 4096);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return h;
}

static int serial_read(HANDLE h, uint8_t *buf, int len)
{
    DWORD got = 0;
    ReadFile(h, buf, (DWORD)len, &got, NULL);
    return (int)got;
}

/* ── Sentinel search ──────────────────────────────────────────────── */
static int find_bytes(const uint8_t *hay, int hlen,
                      const char   *needle, int nlen)
{
    for (int i = 0; i <= hlen - nlen; i++)
        if (memcmp(hay + i, needle, nlen) == 0) return i;
    return -1;
}

/* ── Ctrl+C handler ───────────────────────────────────────────────── */
static volatile int quit = 0;
static void on_sigint(int s) { (void)s; quit = 1; }

/* ── Main ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: record.exe <COMx> <output.wav> [baud]\n");
        printf("  Example: record.exe COM7 output.wav\n");
        printf("  Example: record.exe COM7 output.wav 115200\n");
        return 1;
    }

    signal(SIGINT, on_sigint);

    const char *port     = argv[1];
    const char *wav_path = argv[2];
    DWORD       baud     = (argc >= 4) ? (DWORD)atol(argv[3]) : 125000;

    printf("Opening %s at %lu baud...\n", port, (unsigned long)baud);
    HANDLE ser = serial_open(port, baud);
    printf("Connected!\n");
    printf("Press PD2 on ATmega to START recording.\n");
    printf("Press PD2 again to STOP and save WAV.\n");
    printf("Press Ctrl+C to quit.\n\n");

    /* Heap-allocate audio buffer */
    uint8_t *audio = (uint8_t *)malloc(MAX_SAMPLES);
    if (!audio) { fprintf(stderr, "malloc failed\n"); return 1; }

    uint8_t  rbuf[READ_BUF_SIZE];
    uint8_t  overlap[OVERLAP_SIZE];
    int      ov_len      = 0;
    int      recording   = 0;
    uint32_t sample_count = 0;
    int      rec_num     = 0;
    char     out_path[512];

    while (!quit) {

        int n = serial_read(ser, rbuf, sizeof(rbuf));

        /* Nothing received — small sleep to avoid 100% CPU */
        if (n <= 0) { Sleep(1); continue; }

        /* Merge overlap + new data */
        uint8_t combined[OVERLAP_SIZE + READ_BUF_SIZE];
        memcpy(combined,          overlap, ov_len);
        memcpy(combined + ov_len, rbuf,    n);
        int clen = ov_len + n;

        if (!recording) {
            /* ── Waiting for ##START## ── */
            int pos = find_bytes(combined, clen, START_SEN, START_LEN);
            if (pos != -1) {
                recording    = 1;
                sample_count = 0;
                rec_num++;
                ov_len = 0;

                if (rec_num == 1) {
                    snprintf(out_path, sizeof(out_path), "%s", wav_path);
                } else {
                    char base[512];
                    strncpy(base, wav_path, sizeof(base) - 1);
                    char *dot = strrchr(base, '.');
                    if (dot) *dot = '\0';
                    snprintf(out_path, sizeof(out_path),
                             "%s_%d.wav", base, rec_num);
                }

                printf("[REC #%d] Recording... press PD2 to stop.\n", rec_num);

                /* Bytes after sentinel = first audio bytes */
                int start = pos + START_LEN;
                int left  = clen - start;
                if (left > 0) {
                    if (sample_count + left > MAX_SAMPLES)
                        left = (int)(MAX_SAMPLES - sample_count);
                    memcpy(audio + sample_count, combined + start, left);
                    sample_count += left;
                }
            } else {
                /* Keep tail for next round */
                int keep = (clen < OVERLAP_SIZE) ? clen : OVERLAP_SIZE;
                memcpy(overlap, combined + clen - keep, keep);
                ov_len = keep;
            }

        } else {
            /* ── Recording — watching for ##STOP## ── */
            int pos = find_bytes(combined, clen, STOP_SEN, STOP_LEN);
            if (pos != -1) {
                /* Audio ends just before sentinel */
                if (sample_count + pos <= MAX_SAMPLES) {
                    memcpy(audio + sample_count, combined, pos);
                    sample_count += pos;
                }
                recording = 0;
                ov_len    = 0;
                printf("\r[REC #%d] Stopped.                        \n", rec_num);
                save_wav(out_path, audio, sample_count);
                printf("Press PD2 to record again, Ctrl+C to quit.\n\n");
            } else {
                /* Safe bytes = everything except possible partial sentinel at tail */
                int safe = clen - STOP_LEN;
                if (safe < 0) safe = 0;

                if ((uint32_t)safe + sample_count > MAX_SAMPLES)
                    safe = (int)(MAX_SAMPLES - sample_count);

                if (safe > 0) {
                    memcpy(audio + sample_count, combined, safe);
                    sample_count += safe;
                }

                /* Keep tail as overlap */
                int keep = clen - safe;
                memcpy(overlap, combined + safe, keep);
                ov_len = keep;

                /* Progress display */
                float secs = (float)sample_count / SAMPLE_RATE;
                printf("\r  %.1f sec  (%lu bytes) ", secs,
                       (unsigned long)sample_count);
                fflush(stdout);
            }
        }
    }

    /* Ctrl+C pressed */
    printf("\nQuitting...\n");
    if (recording && sample_count > 0) {
        printf("Saving partial recording...\n");
        save_wav(out_path, audio, sample_count);
    }

    free(audio);
    CloseHandle(ser);
    return 0;
}