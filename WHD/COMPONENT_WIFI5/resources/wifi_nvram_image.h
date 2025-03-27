/*
 * Copyright (c) 2019, Cypress Semiconductor Corporation, All Rights Reserved
 * SPDX-License-Identifier: LicenseRef-PBL
 *
 * This file and the related binary are licensed under the
 * Permissive Binary License, Version 1.0 (the "License");
 * you may not use these files except in compliance with the License.
 *
 * You may obtain a copy of the License here:
 * LICENSE-permissive-binary-license-1.0.txt and at
 * https://www.mbed.com/licenses/PBL-1.0
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDED_NVRAM_IMAGE_H_
#define INCLUDED_NVRAM_IMAGE_H_

#include <string.h>
#include <stdint.h>
#include "wiced_resource.h"
#include "generated_mac_address.txt"

#if defined(CY_STORAGE_WIFI_DATA)
CY_SECTION_WHD(CY_STORAGE_WIFI_DATA) __attribute__( (used) )
#endif

const resource_hnd_t wifi_nvram_image;
extern unsigned char wifi_nvram_image_data[];
extern uint32_t wifi_nvram_image_size;

#ifndef __IAR_SYSTEMS_ICC__
#ifdef CY_STORAGE_WIFI_DATA
RESOURCE_BIN_ADD(".cy_xip.nvram", NVRAM_IMAGE_NAME, wifi_nvram_image_data, wifi_nvram_image_size);
#else
RESOURCE_BIN_ADD(".rodata", NVRAM_IMAGE_NAME, wifi_nvram_image_data, wifi_nvram_image_size);
#endif
#else
uint32_t  wifi_nvram_image_size = NVRAM_IMAGE_SIZE;
#endif

const resource_hnd_t wifi_nvram_image = { RESOURCE_IN_MEMORY, NVRAM_IMAGE_SIZE, {.mem = { (char *) wifi_nvram_image_data }}};

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
