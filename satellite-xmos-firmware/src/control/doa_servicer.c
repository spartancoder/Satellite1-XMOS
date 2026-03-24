#include <string.h>
#include "debug_print.h"
#include "servicer.h"
#include "doa_servicer.h"
#include "FreeRTOS.h"
#include "platform/platform_conf.h"

// Command map - defines what commands this servicer accepts
static control_cmd_info_t doa_servicer_cmd_map[] = {
    { DOA_SERVICER_CMD_GET_DOA, DOA_RESULT_SIZE, sizeof(uint8_t), CMD_READ_ONLY },
};

// Compile-time size check
_Static_assert(sizeof(doa_result_t) == 19, "doa_result_t must be 19 bytes");
_Static_assert(sizeof(doa_source_t) == 6, "doa_source_t must be 6 bytes");

//-----------------Servicer read callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t doa_servicer_read_cmd(control_resid_t resid,
                                           control_cmd_t cmd,
                                           uint8_t *payload,
                                           size_t payload_len,
                                           void *app_data)
{
    control_ret_t ret = CONTROL_SUCCESS;
    doa_servicer_ctx_t *ctx = (doa_servicer_ctx_t *) app_data;
    servicer_t *servicer = ctx->servicer;

    // For read commands, payload[0] is reserved for status
    payload_len -= 1;
    uint8_t *payload_ptr = &payload[1];

    debug_printf("DOA servicer on tile %d received READ command %02x for resid %02x\n",
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
        case DOA_SERVICER_CMD_GET_DOA:
            // Copy DOA result to payload (19 bytes)
            memcpy(payload_ptr, ctx->doa_result, sizeof(doa_result_t));
            payload[0] = CONTROL_SUCCESS;
            debug_printf("DOA: az=%d, el=%d, conf=%d, vad=%d, count=%d\n",
                         ctx->doa_result->sources[0].azimuth_cdeg,
                         ctx->doa_result->sources[0].elevation_cdeg,
                         ctx->doa_result->sources[0].confidence,
                         ctx->doa_result->sources[0].vad,
                         ctx->doa_result->count);
            return CONTROL_SUCCESS;

        default:
            debug_printf("DOA SERVICER UNHANDLED COMMAND: %02x\n", cmd_id);
            ret = CONTROL_BAD_COMMAND;
            payload[0] = ret;
            return ret;
    }
}

//-----------------Servicer write callback function-----------------------//
DEVICE_CONTROL_CALLBACK_ATTR
static control_ret_t doa_servicer_write_cmd(control_resid_t resid,
                                            control_cmd_t cmd,
                                            const uint8_t *payload,
                                            size_t payload_len,
                                            void *app_data)
{
    // DOA servicer is read-only, no write commands
    debug_printf("DOA servicer received unexpected WRITE command %02x for resid %02x\n",
                 cmd, resid);
    return CONTROL_BAD_COMMAND;
}

//-----------------Servicer task-----------------------//
void doa_servicer_task(void *args)
{
    device_control_servicer_t servicer_ctx;
    doa_servicer_ctx_t *ctx = (doa_servicer_ctx_t *) args;
    servicer_t *servicer = ctx->servicer;

    xassert(servicer != NULL);

    control_resid_t *resources = (control_resid_t *) pvPortMalloc(
        servicer->num_resources * sizeof(control_resid_t));
    for (int i = 0; i < servicer->num_resources; i++) {
        resources[i] = servicer->res_info[i].resource;
    }

    control_ret_t dc_ret;
    debug_printf("DOA servicer registering, ID %d, tile %d, core %d\n",
                 servicer->id, THIS_XCORE_TILE, rtos_core_id_get());

    dc_ret = device_control_servicer_register(&servicer_ctx,
                                              ctx->device_control_ctx,
                                              ctx->device_control_ctx_count,
                                              resources,
                                              servicer->num_resources);

    vPortFree(resources);

    if (dc_ret != CONTROL_SUCCESS) {
        debug_printf("DOA servicer registration failed: %d\n", dc_ret);
        return;
    }

    debug_printf("DOA servicer registered successfully\n");

    // Main command processing loop
    for (;;) {
        device_control_servicer_cmd_recv(&servicer_ctx,
                                         doa_servicer_read_cmd,
                                         doa_servicer_write_cmd,
                                         ctx,
                                         RTOS_OSAL_WAIT_FOREVER);
    }
}

//-----------------Initialization functions-----------------------//
void doa_servicer_init(doa_servicer_ctx_t *ctx, doa_result_t *doa_result)
{
    static servicer_t servicer;
    static control_resource_info_t servicer_res_info[DOA_SERVICER_NUM_RESOURCES];

    ctx->servicer = &servicer;

    memset(&servicer, 0, sizeof(servicer_t));
    servicer.id = DOA_SERVICER_RESID;
    servicer.start_io = 0;
    servicer.num_resources = DOA_SERVICER_NUM_RESOURCES;

    servicer.res_info = &servicer_res_info[0];
    servicer.res_info[0].resource = DOA_SERVICER_RESID;
    servicer.res_info[0].command_map.num_commands = NUM_DOA_SERVICER_RESID_CMDS;
    servicer.res_info[0].command_map.commands = doa_servicer_cmd_map;

    ctx->doa_result = doa_result;
}

void doa_servicer_start(doa_servicer_ctx_t *ctx,
                        device_control_t **device_control_ctx,
                        size_t device_control_ctx_count)
{
    ctx->device_control_ctx = device_control_ctx;
    ctx->device_control_ctx_count = device_control_ctx_count;

    xTaskCreate(
        doa_servicer_task,
        "DOA servicer",
        RTOS_THREAD_STACK_SIZE(doa_servicer_task),
        ctx,
        appconfDEVICE_CONTROL_SPI_PRIORITY - 1,
        NULL
    );
}
