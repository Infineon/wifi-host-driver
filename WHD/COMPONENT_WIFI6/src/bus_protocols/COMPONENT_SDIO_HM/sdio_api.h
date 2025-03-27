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

#ifndef _SDIO_API_H_
#define _SDIO_API_H_

//==================================================================================================
// Types and constants
//==================================================================================================

#define MAX_USER_EVT_LEN                  (1024)

/** User defined callback function prototype.
 *
 * Routine is used to handle user defined command from host via SDIO bus.
 *
 * @param[in] set     : Set or Get command.
 * @param[in] cmd     : command data.
 * @param[in] cmd_len : length of command data.
 *
 * @return command result.
 */
typedef cy_rslt_t (*sdio_hm_user_cmd_cb)(bool set, void *cmd, uint16_t cmd_len);

//==================================================================================================
// Public Functions
//==================================================================================================

/**
 * sdio_hm_register_user_cmd_cb - register callback function
 *
 * @cmd_cb: user defined command callback function.
 */
cy_rslt_t sdio_hm_register_user_cmd_cb(sdio_hm_user_cmd_cb cmd_cb);

/**
 * sdio_hm_usr_evt_to_host - send event packet to host via SDIO bus
 *
 * @evt: event data.
 * @evt_len: event data length.
 */
cy_rslt_t sdio_hm_usr_evt_to_host(const void *evt, uint16_t evt_len);

#endif /* _SDIO_API_H_ */
