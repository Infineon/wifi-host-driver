/***************************************************************************//**
* \file cybsp.h
*
* \brief
* Basic API for setting up specific boards
*
********************************************************************************
* \copyright
* Copyright 2021 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation
*
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
*******************************************************************************/

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/* WIFI interface types */
#define CYBSP_SDIO_INTERFACE             (0)
#define CYBSP_SPI_INTERFACE              (1)
#define CYBSP_M2M_INTERFACE              (2)

#ifdef __cplusplus
}
#endif /* __cplusplus */
