// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "wav_utils.h"

#define RIFF_SECTION_SIZE       12
#define FMT_SUBCHUNK_MIN_SIZE   24
#define EXTENDED_FMT_GUID_SIZE  16

static const char wav_default_header[WAV_HEADER_BYTES] = {
    0x52, 0x49, 0x46, 0x46,     /* "RIFF" */
    0x00, 0x00, 0x00, 0x00,     /* wav_size (placeholder) */
    0x57, 0x41, 0x56, 0x45,     /* "WAVE" */
    0x66, 0x6d, 0x74, 0x20,     /* "fmt " */
    0x10, 0x00, 0x00, 0x00,     /* fmt_chunk_size = 16 */
    0x00, 0x00, 0x00, 0x00,     /* audio_format + num_channels */
    0x00, 0x00, 0x00, 0x00,     /* sample_rate */
    0x00, 0x00, 0x00, 0x00,     /* byte_rate */
    0x00, 0x00, 0x00, 0x00,     /* sample_alignment + bit_depth */
    0x64, 0x61, 0x74, 0x61,     /* "data" */
    0x00, 0x00, 0x00, 0x00,     /* data_bytes (placeholder) */
};

int wav_read_header(xscope_file_t *file, wav_header_t *hdr, unsigned *header_size)
{
    /* Rewind to start */
    xscope_fseek(file, 0, SEEK_SET);

    /* Read RIFF header section (12 bytes) */
    xscope_fread(file, (uint8_t *)&hdr->riff_header[0], RIFF_SECTION_SIZE);
    if (memcmp(hdr->riff_header, "RIFF", 4) != 0) {
        printf("Error: WAV missing RIFF header\n");
        return 1;
    }
    if (memcmp(hdr->wave_header, "WAVE", 4) != 0) {
        printf("Error: WAV missing WAVE header\n");
        return 1;
    }

    /* Read fmt subchunk header (24 bytes: fmt_header + fmt_chunk_size + format fields) */
    xscope_fread(file, (uint8_t *)&hdr->fmt_header[0], FMT_SUBCHUNK_MIN_SIZE);
    if (memcmp(hdr->fmt_header, "fmt ", 4) != 0) {
        printf("Error: WAV missing fmt subchunk\n");
        return 1;
    }

    unsigned fmt_subchunk_actual_size = hdr->fmt_chunk_size + 8; /* +8 for header + size fields */
    unsigned fmt_subchunk_remaining_size = fmt_subchunk_actual_size - FMT_SUBCHUNK_MIN_SIZE;

    if (hdr->audio_format == (short)0xfffe) {
        /* Extended format: seek to GUID, read actual format, skip rest */
        xscope_fseek(file, fmt_subchunk_remaining_size - EXTENDED_FMT_GUID_SIZE, SEEK_CUR);
        xscope_fread(file, (uint8_t *)&hdr->audio_format, sizeof(hdr->audio_format));
        xscope_fseek(file, EXTENDED_FMT_GUID_SIZE - sizeof(hdr->audio_format), SEEK_CUR);
    } else {
        /* Standard format: skip any extra fmt bytes */
        xscope_fseek(file, fmt_subchunk_remaining_size, SEEK_CUR);
    }

    if (hdr->audio_format != 1) {
        printf("Error: WAV audio format %d is not PCM\n", hdr->audio_format);
        return 1;
    }

    /* Read data subchunk header */
    xscope_fread(file, (uint8_t *)&hdr->data_header[0], sizeof(hdr->data_header));

    /* Handle optional "fact" subchunk */
    if (memcmp(hdr->data_header, "fact", 4) == 0) {
        uint32_t chunk_size;
        xscope_fread(file, (uint8_t *)&chunk_size, sizeof(chunk_size));
        xscope_fseek(file, chunk_size, SEEK_CUR);
        xscope_fread(file, (uint8_t *)&hdr->data_header[0], sizeof(hdr->data_header));
    }

    if (memcmp(hdr->data_header, "data", 4) != 0) {
        printf("Error: WAV missing data subchunk\n");
        return 1;
    }

    /* Read data size */
    xscope_fread(file, (uint8_t *)&hdr->data_bytes, sizeof(hdr->data_bytes));

    /* Current position is the start of audio data */
    *header_size = (unsigned)xscope_ftell(file);

    return 0;
}

int wav_form_header(wav_header_t *hdr, short num_channels, int sample_rate,
                    short bit_depth, int num_frames)
{
    memcpy(hdr, wav_default_header, WAV_HEADER_BYTES);

    hdr->audio_format = 1; /* PCM */
    hdr->num_channels = num_channels;
    hdr->sample_rate  = sample_rate;
    hdr->bit_depth    = bit_depth;
    hdr->byte_rate    = sample_rate * num_channels * (bit_depth / 8);
    hdr->sample_alignment = num_channels * (bit_depth / 8);

    int data_bytes = num_frames * num_channels * (bit_depth / 8);
    hdr->data_bytes = data_bytes;
    hdr->wav_size   = data_bytes + WAV_HEADER_BYTES - 8;

    return 0;
}

void wav_write_header(xscope_file_t *file, const wav_header_t *hdr)
{
    xscope_fseek(file, 0, SEEK_SET);
    xscope_fwrite(file, (uint8_t *)hdr, WAV_HEADER_BYTES);
}

void wav_update_data_size(xscope_file_t *file, int data_bytes)
{
    /* data_bytes field is at offset 40 in a standard 44-byte WAV header */
    xscope_fseek(file, 40, SEEK_SET);
    xscope_fwrite(file, (uint8_t *)&data_bytes, sizeof(data_bytes));
}
