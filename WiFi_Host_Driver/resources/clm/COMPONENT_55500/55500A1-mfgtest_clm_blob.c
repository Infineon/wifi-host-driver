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
#include "wiced_resource.h"

#ifdef WLAN_MFG_FIRMWARE
#if defined(CY_STORAGE_WIFI_DATA)
CY_SECTION_WHD(CY_STORAGE_WIFI_DATA) __attribute__((used))
#endif
const unsigned char wifi_mfg_firmware_clm_blob_data[1519] = {
        66, 76, 79, 66, 60, 0, 0, 0, 32, 177, 53, 120, 1, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 60, 0, 0, 0, 175, 5, 0, 0, 221, 117, 79, 10, 0, 0, 0,
        0, 0, 0, 0, 0, 235, 5, 0, 0, 4, 0, 0, 0, 42, 234, 69, 158, 0, 0, 0,
        0, 67, 76, 77, 32, 68, 65, 84, 65, 0, 0, 20, 0, 0, 0, 73, 70, 88, 46,
        66, 82, 65, 78, 67, 72, 95, 49, 56, 95, 53, 51, 0, 0, 0, 0, 49, 46,
        52, 57, 46, 53, 0, 0, 0, 0, 0, 0, 0, 0, 204, 0, 0, 0, 67, 108, 109,
        73, 109, 112, 111, 114, 116, 58, 32, 49, 46, 52, 56, 46, 48, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 118, 49, 32, 50, 51, 47, 48, 57, 47, 49,
        49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 88, 90,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 112, 0, 0, 0, 1, 1, 1, 11,
        1, 13, 1, 14, 2, 10, 11, 11, 11, 13, 11, 14, 36, 48, 36, 64, 36, 140,
        36, 144, 36, 165, 52, 60, 64, 64, 64, 100, 100, 100, 100, 144, 100,
        165, 104, 128, 132, 140, 132, 144, 149, 165, 1, 93, 1, 233, 97, 109,
        97, 113, 113, 113, 113, 117, 117, 117, 117, 181, 117, 233, 121, 185,
        185, 233, 189, 233, 0, 0, 80, 5, 0, 0, 104, 5, 0, 0, 132, 0, 0, 0, 41,
        5, 0, 0, 72, 2, 0, 0, 215, 4, 0, 0, 94, 2, 0, 0, 29, 3, 0, 0, 173, 2,
        0, 0, 137, 3, 0, 0, 224, 1, 0, 0, 104, 0, 0, 0, 124, 0, 0, 0, 255, 80,
        129, 132, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 175, 4, 0, 0, 0, 0, 0, 0, 47, 5,
        0, 0, 88, 5, 0, 0, 112, 5, 0, 0, 120, 5, 0, 0, 96, 5, 0, 0, 36, 2, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 12, 0, 0, 0, 14, 0, 0, 0, 82, 0, 0, 0, 44, 2, 0, 0, 3, 5,
        0, 0, 136, 5, 0, 0, 237, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        144, 5, 0, 0, 152, 5, 0, 0, 128, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 4, 0, 0, 123, 4, 0,
        0, 5, 0, 0, 0, 232, 1, 0, 0, 35, 97, 0, 4, 1, 4, 1, 8, 0, 0, 1, 1, 68,
        69, 0, 1, 2, 1, 2, 40, 0, 0, 0, 0, 74, 80, 0, 2, 3, 2, 3, 8, 0, 0, 0,
        0, 85, 83, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 88, 90, 0, 3, 4, 3, 4, 8, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 2, 1, 1, 0,
        3, 1, 1, 0, 4, 1, 1, 0, 5, 1, 1, 0, 6, 1, 1, 0, 1, 1, 1, 2, 1, 3, 2,
        10, 22, 1, 11, 2, 11, 22, 1, 12, 3, 23, 26, 31, 1, 24, 0, 0, 255, 1,
        30, 1, 0, 5, 92, 0, 2, 86, 0, 3, 126, 4, 0, 92, 5, 2, 86, 5, 3, 1, 0,
        255, 1, 23, 2, 0, 1, 60, 2, 0, 1, 0, 255, 1, 23, 2, 0, 1, 60, 2, 0,
        1, 0, 0, 1, 23, 2, 0, 5, 92, 0, 2, 86, 0, 3, 126, 4, 0, 92, 6, 2, 86,
        6, 3, 2, 0, 255, 1, 30, 3, 0, 1, 120, 3, 0, 4, 3, 86, 0, 1, 126, 4,
        1, 86, 5, 1, 128, 4, 7, 98, 0, 1, 78, 0, 2, 86, 0, 7, 126, 4, 8, 98,
        5, 1, 78, 5, 2, 86, 5, 7, 4, 1, 60, 2, 1, 128, 4, 1, 60, 2, 8, 4, 1,
        60, 2, 1, 128, 4, 1, 60, 2, 8, 4, 3, 86, 0, 1, 126, 4, 1, 86, 6, 1,
        128, 4, 7, 98, 0, 1, 78, 0, 2, 86, 0, 5, 126, 4, 8, 98, 6, 1, 78, 6,
        2, 86, 6, 5, 4, 1, 120, 3, 1, 128, 4, 4, 120, 0, 3, 120, 3, 6, 120,
        4, 3, 120, 7, 3, 5, 2, 255, 2, 24, 11, 30, 22, 0, 6, 62, 8, 1, 90, 13,
        1, 80, 15, 1, 90, 19, 1, 86, 21, 1, 118, 22, 1, 6, 2, 255, 1, 30, 12,
        0, 1, 120, 12, 1, 3, 1, 255, 2, 24, 10, 30, 22, 0, 6, 62, 8, 1, 90,
        13, 1, 80, 15, 1, 90, 19, 1, 86, 20, 1, 118, 22, 1, 4, 2, 255, 2, 23,
        9, 30, 17, 0, 1, 60, 11, 1, 6, 2, 1, 2, 23, 9, 30, 18, 0, 6, 62, 8,
        1, 90, 13, 1, 80, 15, 1, 90, 19, 1, 86, 21, 1, 118, 22, 1, 4, 6, 96,
        8, 0, 90, 13, 0, 80, 15, 0, 90, 19, 0, 86, 21, 0, 118, 22, 0, 128, 4,
        12, 96, 8, 8, 90, 13, 8, 80, 14, 0, 90, 14, 1, 72, 14, 2, 80, 15, 6,
        80, 16, 0, 90, 16, 1, 72, 16, 2, 90, 19, 8, 86, 21, 8, 118, 22, 8, 4,
        1, 120, 12, 0, 128, 4, 1, 120, 12, 8, 4, 6, 96, 8, 0, 90, 13, 0, 80,
        15, 0, 90, 19, 0, 86, 20, 0, 118, 22, 0, 128, 4, 12, 96, 8, 8, 90, 13,
        8, 80, 14, 0, 90, 14, 1, 72, 14, 2, 80, 15, 4, 80, 16, 0, 90, 16, 1,
        72, 16, 2, 90, 19, 8, 86, 20, 8, 118, 22, 8, 4, 1, 60, 11, 0, 128, 4,
        1, 60, 11, 8, 4, 6, 96, 8, 0, 90, 13, 0, 80, 15, 0, 90, 19, 0, 86, 21,
        0, 118, 22, 0, 128, 4, 12, 96, 8, 8, 90, 13, 8, 80, 14, 0, 90, 14, 1,
        72, 14, 2, 80, 15, 4, 80, 16, 0, 90, 16, 1, 72, 16, 2, 90, 19, 8, 86,
        21, 8, 118, 22, 8, 7, 0, 255, 4, 30, 23, 24, 26, 30, 30, 24, 33, 0,
        6, 96, 23, 1, 90, 25, 1, 80, 27, 1, 72, 29, 1, 90, 32, 1, 118, 34, 1,
        8, 0, 255, 1, 30, 24, 0, 1, 120, 24, 1, 4, 5, 96, 23, 0, 90, 25, 0,
        80, 28, 0, 90, 32, 0, 118, 34, 0, 128, 4, 7, 96, 23, 8, 90, 25, 8, 90,
        28, 1, 72, 28, 2, 80, 28, 7, 90, 32, 8, 118, 34, 8, 4, 1, 120, 24, 0,
        128, 4, 1, 120, 24, 8, 12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
        12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 4, 0, 1, 2, 3, 8, 4,
        5, 6, 7, 8, 9, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 8, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 21, 22, 23, 8, 4, 5, 6, 7, 8, 9, 10, 11, 1, 0, 1, 4, 1, 5, 3, 0,
        4, 5, 3, 1, 2, 3, 4, 0, 1, 2, 3, 4, 1, 2, 3, 6, 5, 0, 1, 2, 3, 6, 7,
        0, 1, 2, 3, 4, 5, 6, 1, 2, 3, 9, 17, 22, 50, 48, 50, 52, 45, 48, 51,
        45, 50, 56, 32, 50, 51, 58, 50, 57, 58, 48, 51, 0, 60, 84, 73, 77, 69,
        83, 84, 65, 77, 80, 62, 0, 0, 1, 0, 0, 0, 160, 5, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 163, 5, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 172, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 14, 1, 36, 64, 4, 100, 144, 4, 149, 165, 4, 1, 233, 4, 204, 216,
        0, 0
};
const resource_hnd_t wifi_mfg_firmware_clm_blob = { RESOURCE_IN_MEMORY, 1519, {.mem = { (const char *) wifi_mfg_firmware_clm_blob_data }}};
#endif /* WLAN_MFG_FIRMWARE */
