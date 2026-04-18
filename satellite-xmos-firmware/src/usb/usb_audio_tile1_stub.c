// Tile 1 stub for usb_audio_recv - uses only intertile, no TinyUSB/XUD dependency
// Copyright 2022-2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <string.h>
#include <xcore/channel.h>
#include "FreeRTOS.h"
#include "rtos_intertile.h"
#include "app_conf.h"

/* Match the format used by Tile 0's usb_audio.c:
 *   samp_t = int16_t (CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX == 2)
 *   2 RX channels (CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX == 2)
 *   USB_AUDIO_RECV_DELAY = 0 (non-USB mic source default)
 */
#define TILE1_USB_AUDIO_N_CHANS_RX       2
#define TILE1_USB_AUDIO_BYTES_PER_SAMPLE  2
#define TILE1_USB_AUDIO_RECV_DELAY        0

void usb_audio_recv(rtos_intertile_t *intertile_ctx,
                    size_t frame_count,
                    int32_t **frame_buffers,
                    size_t num_chans)
{
    typedef int16_t samp_t;
    static samp_t usb_audio_out_frame[appconfAUDIO_SPK_PIPELINE_FRAME_ADVANCE][TILE1_USB_AUDIO_N_CHANS_RX];
    size_t bytes_received;
    int32_t *frame_buf_ptr = (int32_t *) frame_buffers;

    const int src_32_shift = 8 * (4 - TILE1_USB_AUDIO_BYTES_PER_SAMPLE);

    bytes_received = rtos_intertile_rx_len(
            intertile_ctx,
            appconfUSB_AUDIO_PORT,
            TILE1_USB_AUDIO_RECV_DELAY);

    if (bytes_received > 0) {
        rtos_intertile_rx_data(
                intertile_ctx,
                usb_audio_out_frame,
                bytes_received);
    } else {
        memset(usb_audio_out_frame, 0, sizeof(usb_audio_out_frame));
    }

    if (frame_buf_ptr != NULL) {
        for (int ch = 0; ch < TILE1_USB_AUDIO_N_CHANS_RX; ch++) {
            for (int i = 0; i < appconfAUDIO_SPK_PIPELINE_FRAME_ADVANCE; i++) {
                if (ch < num_chans) {
                    frame_buf_ptr[i + (appconfAUDIO_SPK_PIPELINE_FRAME_ADVANCE * ch)] =
                        (int32_t)usb_audio_out_frame[i][ch] << src_32_shift;
                }
            }
        }
    }
}
