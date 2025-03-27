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

/** @file whd_wlansense_core.h
 *  Provides event handler functions for CSI functionality.
 */

#if defined(COMPONENT_WLANSENSE)
#ifndef INCLUDED_WHD_WLANSENSE_CORE_H
#define INCLUDED_WHD_WLANSENSE_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "whd.h"
#include "whd_wlioctl.h"


#define WHD_IF_E_ADD              1
#define WHD_IF_E_DEL              2
#define WHD_IF_E_CHANGE           3

#define WL_DUMP_BUF_LEN (4 * 1024)

struct wlc_csi_fragment_hdr {
    uint8_t hdr_version;
    uint8_t sequence_num;
    uint8_t fragment_num;
    uint8_t total_fragments;
};

/** Callback for CSI data that is called when CSI data is processed and is ready to be sent to upper layer
 *
 * @param data: Pointer to CSI data
 * @param len: Length of the CSI data
 *
 */
typedef void (*whd_csi_data_sendup)(char* data, uint32_t len);

struct whd_csi_info {
    char *data;
    whd_csi_data_sendup csi_data_cb_func;
};

typedef struct whd_csi_info *whd_csi_info_t;


/******************************************************
*             Function declarations
******************************************************/

/** Get Wlansense interface instatnce
 *
 * @return whd_interface_t:     The pointer to Wlansense interface instatnce.
 */
extern whd_interface_t whd_wlansense_get_interface(void);

/** Handler attach function that would attach the handler function.
 *
 * @param user_data:            A pointer value which will be passed to the event handler function (NULL is allowed).
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_register_handler(void* user_data);

/** Initializes CSI_Cfg struct with default parameters.
 *
 * @param csi_cfg:              Pointer to the CSI_Cfg struct.
 *
 * @return void:                No error code returns.
 */
extern void whd_wlansense_init_cfg_params(wlc_csi_cfg_t *csi_cfg);

/** Validate CSI_Cfg parameters which will be sent to FW.
 *
 * @param csi_cfg:              Pointer to the CSI_Cfg struct.
  *
 * @return void:                No error code returns.
 */
extern void whd_wlansense_validate_cfg_params(wlc_csi_cfg_t *csi_cfg);

/** Create wlansense interface for handling CSI data.
 *
 * @param whd_driver:           Pointer to handle instance of the driver.
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_create_interface(whd_driver_t whd_driver);

/** Dumps the wlc_csi_cfg_t struct from FW.
 *
 * @param ifp:                  Pointer to handle instance of csi_cfg parameters.
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_get_config(wlc_csi_cfg_t *csi_cfg);


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ifndef INCLUDED_WHD_WLANSENSE_CORE_H */
#endif /* defined(COMPONENT_WLANSENSE) */
