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

#ifndef _SDIO_AT_CMD_SUPPORT_H_
#define _SDIO_AT_CMD_SUPPORT_H_

#if defined(SDIO_HM_AT_CMD)

#include "at_command_parser.h"

/******************************************************************************
* Function prototypes
*******************************************************************************/
bool sdio_cmd_at_is_data_ready();
uint32_t sdio_cmd_at_read_data(uint8_t *buffer, uint32_t size);
cy_rslt_t sdio_cmd_at_write_data(uint8_t *buffer, uint32_t length);

#endif /* defined(SDIO_HM_AT_CMD) */

#endif /* _SDIO_AT_CMD_SUPPORT_H_ */
