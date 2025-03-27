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
/* Automatically generated file - this comment ensures resources.h file creation */
/* Auto-generated header file. Do not edit */
#ifndef INCLUDED_RESOURCES_H_
#define INCLUDED_RESOURCES_H_
#include "wiced_resource.h"
#ifdef DM_43022C1
#include "whd_trxhdr.h"
#endif /* DM_43022C1 */

#if defined(CY_STORAGE_WIFI_DATA)
CY_SECTION_WHD(CY_STORAGE_WIFI_DATA) __attribute__((used))
#endif

const resource_hnd_t wifi_firmware_image;
extern const unsigned char wifi_firmware_image_data[];
extern uint32_t wifi_firmware_image_size;
#ifndef __IAR_SYSTEMS_ICC__
#ifdef CY_STORAGE_WIFI_DATA
RESOURCE_BIN_ADD(".cy_xip.fw", FW_IMAGE_NAME, wifi_firmware_image_data, wifi_firmware_image_size);
#else
RESOURCE_BIN_ADD(".rodata", FW_IMAGE_NAME, wifi_firmware_image_data, wifi_firmware_image_size);
#endif
#endif
#ifndef DM_43022C1
const resource_hnd_t wifi_firmware_image = { RESOURCE_IN_MEMORY, FW_IMAGE_SIZE, {.mem = { (const char *) wifi_firmware_image_data }}};
#else
const resource_hnd_t wifi_firmware_image = { RESOURCE_IN_MEMORY, (FW_IMAGE_SIZE - sizeof(trx_header_t)), {.mem = { (const char *) &wifi_firmware_image_data[sizeof(trx_header_t)] }}};
#endif /* DM_43022C1 */
#endif /* ifndef INCLUDED_RESOURCES_H_ */
