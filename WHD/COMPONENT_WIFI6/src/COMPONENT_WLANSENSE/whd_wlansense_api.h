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

/** @file whd_wlansense_api.h
 *  Provides event handler functions for CSI functionality.
 */

#if defined(COMPONENT_WLANSENSE)
#ifndef INCLUDED_WHD_WLANSENSE_API_H
#define INCLUDED_WHD_WLANSENSE_API_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "whd.h"
#include "whd_int.h"
#include "whd_wlioctl.h"
#include "whd_wlansense_core.h"


/* CSI parameters */
#define CSI_CAPTURE_PERIOD_NON_PERIODIC     -1
#define CSI_DEFAULT_CAPTURE_WINDOW_DUR      10

#define CSI_AS_MODE_UNASSOC                 0
#define CSI_AS_MODE_ASSOC                   1

#define CSI_SOLICIT_MODE_UNSOL              0
#define CSI_SOLICIT_MODE_AF                 1
#define CSI_SOLICIT_MODE_NDP                2
#define CSI_SOLICIT_MODE_RTS                3

#define CSI_BSS_MODE_MY_DEVICE              0
#define CSI_BSS_MODE_MY_BSS                 1
#define CSI_BSS_MODE_ALL_BSS                2

#define INVALID_FT                          0b11
#define CSI_FRAMETYPE_SUBTYPE_BEACON        32
#define CSI_FRAMETYPE_SUBTYPE_DATA          34
#define CSI_FRAMETYPE_SUBTYPE_QOSDATA       42

#define KEY_MAXLEN                          64
#define INV_CHANSPEC                        255


typedef struct wlc_csi_cfg whd_csi_cfg_t;


/******************************************************
*             Function declarations
******************************************************/

/** Called before user starts CSI capture. Registers the callback function to sendup CSI data
 *
 * @param user_data:            A pointer value which will be passed to the event handler function (NULL is allowed).
 * @param csi_data_cb_func:     Pointer to callback function that should be used to sendup CSI data
 * @param csi_cfg:              Pointer to the CSI_Cfg struct provided by user to get the default parameters from WHD
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_register_callback(void* user_data, whd_csi_data_sendup csi_data_cb_func, whd_csi_cfg_t *csi_cfg);

/** Called when user enters csi,enable cmd. Registers the event handler
 *
 * @param csi_cfg:              Pointer to the CSI_Cfg struct which will be sent to FW
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_start_capture(whd_csi_cfg_t *csi_cfg);

/** Called when user enters csi,disable cmd. Deregisters the event handler
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_stop_capture(void);

/** Dumps the wlc_csi_cfg_t struct from FW.
 *
 * @param csi_cfg:              Pointer to the CSI_Cfg struct provided by user to get the current parameters from WHD
 *
 * @return whd_result_t:        WHD_SUCCESS or Error code.
 */
extern whd_result_t whd_wlansense_get_info(whd_csi_cfg_t *csi_cfg);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ifndef INCLUDED_WHD_WLANSENSE_API_H */
#endif /* defined(COMPONENT_WLANSENSE) */
