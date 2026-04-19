// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef WAV_UTILS_H_
#define WAV_UTILS_H_

#include <stdint.h>
#include <string.h>
#include "xscope_io_device.h"

#define WAV_HEADER_BYTES 44

typedef struct wav_header {
    /* RIFF Header */
    char riff_header[4];    /* "RIFF" */
    int  wav_size;          /* data_bytes + WAV_HEADER_BYTES - 8 */
    char wave_header[4];    /* "WAVE" */

    /* Format Subsection */
    char fmt_header[4];     /* "fmt " */
    int  fmt_chunk_size;
    short audio_format;     /* 1 = PCM */
    short num_channels;
    int   sample_rate;
    int   byte_rate;        /* sample_rate * num_channels * (bit_depth/8) */
    short sample_alignment; /* num_channels * (bit_depth/8) */
    short bit_depth;        /* bits per sample */

    /* Data Subsection */
    char data_header[4];    /* "data" */
    int  data_bytes;        /* num_frames * num_channels * (bit_depth/8) */
} wav_header_t;

/* Read and parse WAV header from open file. Seeks to start of audio data.
 * Returns 0 on success, non-zero on error. */
int wav_read_header(xscope_file_t *file, wav_header_t *hdr, unsigned *header_size);

/* Build a WAV header for output with the given parameters. */
int wav_form_header(wav_header_t *hdr, short num_channels, int sample_rate,
                    short bit_depth, int num_frames);

/* Write WAV header to the beginning of an open file. */
void wav_write_header(xscope_file_t *file, const wav_header_t *hdr);

/* Seek back to the data_bytes field and update it (for files where the
 * final frame count wasn't known at creation time). */
void wav_update_data_size(xscope_file_t *file, int data_bytes);

/* Return bytes per interleaved frame (num_channels * bytes_per_sample). */
static inline unsigned wav_get_num_bytes_per_frame(const wav_header_t *s) {
    return (unsigned)(s->num_channels * (s->bit_depth / 8));
}

/* Return total number of sample frames in the file. */
static inline int wav_get_num_frames(const wav_header_t *s) {
    return s->data_bytes / (int)wav_get_num_bytes_per_frame(s);
}

/* Return byte offset of a specific sample frame within the file. */
static inline long wav_get_frame_start(const wav_header_t *s,
                                       unsigned frame_number,
                                       uint32_t header_size) {
    return (long)header_size + (long)frame_number * wav_get_num_bytes_per_frame(s);
}

#endif /* WAV_UTILS_H_ */
