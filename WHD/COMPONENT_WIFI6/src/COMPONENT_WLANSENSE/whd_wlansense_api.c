/*
 * Copyright 2025, Cypress Semiconductor Corporation (an Infineon company)
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

/** @file whd_wlansense_api.c
 *  Provides event handler functions for CSI functionality.
 */

#if defined(COMPONENT_WLANSENSE)
#include "whd_wlansense_api.h"
#include "whd_proto.h"
#include "whd_events_int.h"
#include "whd_types_int.h"


/******************************************************
*             Static variables
******************************************************/
static whd_bool_t IsWlansenseStart = WHD_FALSE;


/******************************************************
*             Function definitions
******************************************************/
whd_result_t
whd_wlansense_register_callback(void* user_data, whd_csi_data_sendup csi_data_cb_func, whd_csi_cfg_t *csi_cfg)
{
    whd_interface_t csi_ifp = whd_wlansense_get_interface();
    CHECK_IFP_NULL(csi_ifp);

    whd_csi_info_t csi_info;

    if (!csi_data_cb_func)
    {
        WPRINT_WHD_ERROR(("The wlansense callback function is not provided!\n"));
        return WHD_BADARG;
    }

    if (!csi_cfg)
    {
        WPRINT_WHD_ERROR(("The wlansense CSI_Cfg Struct is not provided!\n"));
        return WHD_BADARG;
    }

    /* Check if the handler isn't registered, then register it. */
    if (csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY] == WHD_EVENT_NOT_REGISTERED)
    {
        CHECK_RETURN (whd_wlansense_register_handler(user_data));
    }
    csi_info = whd_mem_malloc(sizeof(*csi_info));

    /* Allocating memory for whd_csi_info structure pointer */
    if (!csi_info) {
        WPRINT_WHD_ERROR(("CSI: Failed to allocate memory for csi_info \n"));
        return WHD_BUFFER_ALLOC_FAIL;
    }
    csi_info->csi_data_cb_func = csi_data_cb_func;
    csi_ifp->csi_info = csi_info;

    /* Initialize the CSI_Cfg struc with the default parameters */
    whd_wlansense_init_cfg_params(csi_cfg);

    return WHD_SUCCESS;
}

whd_result_t
whd_wlansense_start_capture(whd_csi_cfg_t *csi_cfg)
{
    if (IsWlansenseStart)
    {
        WPRINT_WHD_ERROR(("The wlansense capture already started!\n"));
        return WHD_SUCCESS;
    }

    whd_interface_t csi_ifp = whd_wlansense_get_interface();
    CHECK_IFP_NULL(csi_ifp);
    whd_driver_t whd_driver = csi_ifp->whd_driver;
    CHECK_DRIVER_NULL(whd_driver);
    whd_interface_t prim_ifp = whd_get_primary_interface(whd_driver);
    CHECK_IFP_NULL(prim_ifp);

    whd_result_t result;
    uint32_t *csi_iovar;
    whd_buffer_t buffer;

    if (!csi_cfg)
    {
        WPRINT_WHD_ERROR(("The wlansense cfg parameters are not provided!\n"));
        return WHD_BADARG;
    }
    else if (csi_cfg->csi_enable == WHD_FALSE)
    {
        WPRINT_WHD_ERROR(("The wlansense feature in cfg parameters struct is disabled!\n"));
        return WHD_BADARG;
    }

    /* Validate CSI_Cfg parameters which will be sent to FW */
    whd_wlansense_validate_cfg_params(csi_cfg);

    csi_iovar = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, sizeof(wlc_csi_cfg_t), IOVAR_STR_CSI);
    CHECK_IOCTL_BUFFER (csi_iovar);
    whd_mem_memcpy(csi_iovar, (uint32_t *)csi_cfg, sizeof(wlc_csi_cfg_t) );
    result = whd_proto_set_iovar(prim_ifp, buffer, 0);
    IsWlansenseStart = WHD_TRUE;
    return result;
}

whd_result_t
whd_wlansense_stop_capture(void)
{
    if (!IsWlansenseStart)
    {
        WPRINT_WHD_ERROR(("The wlansense capture already stopped!\n"));
        return WHD_SUCCESS;
    }

    whd_interface_t csi_ifp = whd_wlansense_get_interface();
    CHECK_IFP_NULL(csi_ifp);
    whd_driver_t whd_driver = csi_ifp->whd_driver;
    CHECK_DRIVER_NULL(whd_driver);
    whd_interface_t prim_ifp = whd_get_primary_interface(whd_driver);
    CHECK_IFP_NULL(prim_ifp);

    whd_result_t result;
    uint32_t *csi_iovar;
    whd_buffer_t buffer;
    wlc_csi_cfg_t csi_cfg_params;

    if (csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY] != WHD_EVENT_NOT_REGISTERED)
    {
        result = whd_wifi_deregister_event_handler(csi_ifp, csi_ifp->event_reg_list[WHD_SCAN_EVENT_ENTRY]);
        csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY] = WHD_EVENT_NOT_REGISTERED;
    }

    if (result != CY_RSLT_SUCCESS) {
        WPRINT_WHD_ERROR(("Error while deregistering the handler %ld \n",result));
    }

    whd_wlansense_init_cfg_params(&csi_cfg_params);
    csi_cfg_params.csi_enable = WHD_FALSE;
    csi_iovar = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, sizeof(wlc_csi_cfg_t),
                                                       IOVAR_STR_CSI);
    CHECK_IOCTL_BUFFER (csi_iovar);
    whd_mem_memcpy(csi_iovar, (uint32_t *)&csi_cfg_params, sizeof(wlc_csi_cfg_t) );
    result = whd_proto_set_iovar(prim_ifp, buffer, 0);
    IsWlansenseStart = WHD_FALSE;
    return result;
}

whd_result_t whd_wlansense_get_info(whd_csi_cfg_t *csi_cfg)
{
    return whd_wlansense_get_config(csi_cfg);
}

#endif /* defined(COMPONENT_WLANSENSE) */
