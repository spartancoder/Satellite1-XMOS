#include <string.h>
#include "debug_print.h"
#include "servicer.h"
#include "audio_pipeline_settings_servicer.h"
#include "FreeRTOS.h"
#include "platform/platform_conf.h"

// Payload size: 4 x uint16_t = 8 bytes
#define MIC_GAIN_PAYLOAD_SIZE (MIC_GAIN_NUM_CHANNELS * sizeof(uint16_t))

// Command map - defines what commands this servicer accepts
static control_cmd_info_t audio_pipeline_settings_servicer_cmd_map[] = {
    { AUDIO_PIPELINE_SETTINGS_CMD_MIC_GAIN, MIC_GAIN_NUM_CHANNELS, sizeof(uint16_t), CMD_READ_WRITE },
    { AUDIO_PIPELINE_SETTINGS_CMD_DOA_LED_ENABLED, 1, sizeof(uint8_t), CMD_READ_WRITE },
};

// Compile-time size check
_Static_assert(sizeof(mic_gain_t) == MIC_GAIN_PAYLOAD_SIZE, "mic_gain_t must be 8 bytes");

//-----------------Servicer read callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t audio_pipeline_settings_servicer_read_cmd(control_resid_t resid,
                                                                control_cmd_t cmd,
                                                                uint8_t *payload,
                                                                size_t payload_len,
                                                                void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;
    audio_pipeline_settings_servicer_ctx_t *ctx = (audio_pipeline_settings_servicer_ctx_t *) app_data;
    servicer_t *servicer = ctx->servicer;

    // For read commands, payload[0] is reserved for status
    payload_len -= 1;
    uint8_t *payload_ptr = &payload[1];

    debug_printf("Audio Pipeline Settings servicer on tile %d received READ command %02x for resid %02x\n",
                 THIS_XCORE_TILE, cmd, resid);

    control_resource_info_t *current_res_info = get_res_info(resid, servicer);
    xassert(current_res_info != NULL);

    control_cmd_info_t *current_cmd_info;
    ret = validate_cmd(&current_cmd_info, current_res_info, cmd, payload_ptr, payload_len);
    if (ret != CONTROL_SUCCESS) {
        payload[0] = ret;
        return ret;
    }

    uint8_t cmd_id = CONTROL_CMD_CLEAR_READ(cmd);
    switch (cmd_id) {
        case AUDIO_PIPELINE_SETTINGS_CMD_MIC_GAIN:
            // Copy gain values to payload (little-endian)
            for (int i = 0; i < MIC_GAIN_NUM_CHANNELS; i++) {
                uint16_t gain = ctx->mic_gain->gain[i];
                payload_ptr[i * 2] = gain & 0xFF;           // Low byte
                payload_ptr[i * 2 + 1] = (gain >> 8) & 0xFF; // High byte
            }
            payload[0] = CONTROL_SUCCESS;
            debug_printf("MIC_GAIN read: [%d, %d, %d, %d]\n",
                         ctx->mic_gain->gain[0], ctx->mic_gain->gain[1],
                         ctx->mic_gain->gain[2], ctx->mic_gain->gain[3]);
            return CONTROL_SUCCESS;

        case AUDIO_PIPELINE_SETTINGS_CMD_DOA_LED_ENABLED:
            payload_ptr[0] = ctx->led_settings->doa_led_enabled;
            payload[0] = CONTROL_SUCCESS;
            debug_printf("DOA_LED_ENABLED read: %d\n", ctx->led_settings->doa_led_enabled);
            return CONTROL_SUCCESS;

        default:
            debug_printf("Audio Pipeline Settings SERVICER UNHANDLED COMMAND: %02x\n", cmd_id);
            ret = CONTROL_BAD_COMMAND;
            payload[0] = ret;
            return ret;
    }
}

//-----------------Servicer write callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t audio_pipeline_settings_servicer_write_cmd(control_resid_t resid,
                                                                 control_cmd_t cmd,
                                                                 const uint8_t *payload,
                                                                 size_t payload_len,
                                                                 void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;
    audio_pipeline_settings_servicer_ctx_t *ctx = (audio_pipeline_settings_servicer_ctx_t *) app_data;
    servicer_t *servicer = ctx->servicer;

    debug_printf("Audio Pipeline Settings servicer on tile %d received WRITE command %02x for resid %02x\n",
                 THIS_XCORE_TILE, cmd, resid);

    control_resource_info_t *current_res_info = get_res_info(resid, servicer);
    xassert(current_res_info != NULL);

    control_cmd_info_t *current_cmd_info;
    ret = validate_cmd(&current_cmd_info, current_res_info, cmd, payload, payload_len);
    if (ret != CONTROL_SUCCESS) {
        return ret;
    }

    uint8_t cmd_id = CONTROL_CMD_CLEAR_READ(cmd);
    switch (cmd_id) {
        case AUDIO_PIPELINE_SETTINGS_CMD_MIC_GAIN:
            // Parse gain values from payload (little-endian)
            for (int i = 0; i < MIC_GAIN_NUM_CHANNELS; i++) {
                uint16_t gain = payload[i * 2] | (payload[i * 2 + 1] << 8);
                ctx->mic_gain->gain[i] = gain;
            }
            debug_printf("MIC_GAIN written: [%d, %d, %d, %d]\n",
                         ctx->mic_gain->gain[0], ctx->mic_gain->gain[1],
                         ctx->mic_gain->gain[2], ctx->mic_gain->gain[3]);
            return CONTROL_SUCCESS;

        case AUDIO_PIPELINE_SETTINGS_CMD_DOA_LED_ENABLED:
            ctx->led_settings->doa_led_enabled = payload[0] ? 1 : 0;
            debug_printf("DOA_LED_ENABLED written: %d\n", ctx->led_settings->doa_led_enabled);
            return CONTROL_SUCCESS;

        default:
            debug_printf("Audio Pipeline Settings SERVICER UNHANDLED COMMAND: %02x\n", cmd_id);
            return CONTROL_BAD_COMMAND;
    }
}

//-----------------Servicer task-----------------------//
void audio_pipeline_settings_servicer_task(void *args)
{
    device_control_servicer_t servicer_ctx;
    audio_pipeline_settings_servicer_ctx_t *ctx = (audio_pipeline_settings_servicer_ctx_t *) args;
    servicer_t *servicer = ctx->servicer;

    xassert(servicer != NULL);

    control_resid_t *resources = (control_resid_t *) pvPortMalloc(
        servicer->num_resources * sizeof(control_resid_t));
    for (int i = 0; i < servicer->num_resources; i++) {
        resources[i] = servicer->res_info[i].resource;
    }

    control_ret_t dc_ret;
    debug_printf("Audio Pipeline Settings servicer registering, ID %d, tile %d, core %d\n",
                 servicer->id, THIS_XCORE_TILE, rtos_core_id_get());

    dc_ret = device_control_servicer_register(&servicer_ctx,
                                              ctx->device_control_ctx,
                                              ctx->device_control_ctx_count,
                                              resources,
                                              servicer->num_resources);

    vPortFree(resources);

    if (dc_ret != CONTROL_SUCCESS) {
        debug_printf("Audio Pipeline Settings servicer registration failed: %d\n", dc_ret);
        return;
    }

    debug_printf("Audio Pipeline Settings servicer registered successfully\n");

    // Main command processing loop
    for (;;) {
        device_control_servicer_cmd_recv(&servicer_ctx,
                                         audio_pipeline_settings_servicer_read_cmd,
                                         audio_pipeline_settings_servicer_write_cmd,
                                         ctx,
                                         RTOS_OSAL_WAIT_FOREVER);
    }
}

//-----------------Initialization functions-----------------------//
void audio_pipeline_settings_servicer_init(audio_pipeline_settings_servicer_ctx_t *ctx,
                                           mic_gain_t *mic_gain,
                                           led_settings_t *led_settings)
{
    static servicer_t servicer;
    static control_resource_info_t servicer_res_info[AUDIO_PIPELINE_SETTINGS_SERVICER_NUM_RESOURCES];

    ctx->servicer = &servicer;

    memset(&servicer, 0, sizeof(servicer_t));
    servicer.id = AUDIO_PIPELINE_SETTINGS_SERVICER_RESID;
    servicer.start_io = 0;
    servicer.num_resources = AUDIO_PIPELINE_SETTINGS_SERVICER_NUM_RESOURCES;

    servicer.res_info = &servicer_res_info[0];
    servicer.res_info[0].resource = AUDIO_PIPELINE_SETTINGS_SERVICER_RESID;
    servicer.res_info[0].command_map.num_commands = NUM_AUDIO_PIPELINE_SETTINGS_CMDS;
    servicer.res_info[0].command_map.commands = audio_pipeline_settings_servicer_cmd_map;

    ctx->mic_gain = mic_gain;
    ctx->led_settings = led_settings;
}

void audio_pipeline_settings_servicer_start(audio_pipeline_settings_servicer_ctx_t *ctx,
                                            device_control_t **device_control_ctx,
                                            size_t device_control_ctx_count)
{
    ctx->device_control_ctx = device_control_ctx;
    ctx->device_control_ctx_count = device_control_ctx_count;

    xTaskCreate(
        audio_pipeline_settings_servicer_task,
        "Audio Pipeline Settings servicer",
        RTOS_THREAD_STACK_SIZE(audio_pipeline_settings_servicer_task),
        ctx,
        appconfDEVICE_CONTROL_SPI_PRIORITY - 1,
        NULL
    );
}
