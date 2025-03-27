/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if defined(COMPONENT_SDIO_HM)

#include "sdio_hosted_support.h"

cy_rslt_t sdio_hm_usr_evt_to_host(const void *evt, uint16_t evt_len)
{
    sdio_handler_t sdio_hm = sdio_hm_get_sdio_handler();
    inf_user_event_t *inf_event = NULL;
    inf_event_base_t *base = NULL;
    cy_rslt_t result;

    if ((!sdio_hm) || (!sdio_hm->sdio_instance.is_ready)) {
        PRINT_HM_ERROR(("Host is not up, can't send user event\n"));
        return CYHAL_SDIO_RSLT_CANCELED;
    }

    if(evt_len > MAX_USER_EVT_LEN) {
        PRINT_HM_ERROR(("Over max event length\n"));
        return CYHAL_SDIO_RSLT_ERR_UNSUPPORTED;
    }

    inf_event = whd_mem_malloc(sizeof(*inf_event));
    if (!inf_event) {
        PRINT_HM_ERROR(("inf_event malloc failed\n"));
        return CYHAL_SDIO_RSLT_ERR_COMMAND_SEND;
    }

    whd_mem_memset(inf_event, 0, sizeof(*inf_event));
    whd_mem_memcpy(inf_event->event, evt, evt_len);

    base = &inf_event->base;
    base ->ver = USER_EVENT_VER;
    base ->type = INF_USER_EVENT;
    base ->len = evt_len;

    result = SDIO_HM_TX_EVENT(sdio_hm, inf_event, sizeof(*inf_event));
    if (result != SDIOD_STATUS_SUCCESS) {
        sdio_hm->tx_info->err_event++;
        PRINT_HM_ERROR(("tx event failed: 0x%lx\n", result));
        free(inf_event);
    }

    return result;
}

cy_rslt_t sdio_hm_register_user_cmd_cb(sdio_hm_user_cmd_cb cmd_cb)
{
    sdio_handler_t sdio_hm = sdio_hm_get_sdio_handler();

    if (!sdio_hm) {
        PRINT_HM_ERROR(("SDIO host is not ready\n"));
        return CYHAL_SDIO_RSLT_CANCELED;
    }

    if (!sdio_hm->sdio_cmd) {
        PRINT_HM_ERROR(("SDIO command is not ready\n"));
        return CYHAL_SDIO_RSLT_CANCELED;
    }

    sdio_hm->sdio_cmd->user_cmd_cb = cmd_cb;

    return CY_RSLT_SUCCESS;
}

#endif /* COMPONENT_SDIO_HM */
