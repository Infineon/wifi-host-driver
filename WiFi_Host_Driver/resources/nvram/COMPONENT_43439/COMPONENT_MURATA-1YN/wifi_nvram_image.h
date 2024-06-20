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

#ifndef INCLUDED_NVRAM_IMAGE_H_
#define INCLUDED_NVRAM_IMAGE_H_

#include <string.h>
#include <stdint.h>
#include "generated_mac_address.txt"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Character array of NVRAM image
 * Generated from cyfmac43439-sdio.1YN.txt
 */

static const char wifi_nvram_image[] =
        "NVRAMRev=$Rev: 726808 $"                                            "\x00"
        "manfid=0x2d0"                                                       "\x00"
        "prodid=0x0727"                                                      "\x00"
        "vendid=0x14e4"                                                      "\x00"
        "devid=0x43e2"                                                       "\x00"
        "boardtype=0x0887"                                                   "\x00"
        "boardrev=0x1101"                                                    "\x00"
        "boardnum=22"                                                        "\x00"
        NVRAM_GENERATED_MAC_ADDRESS                                          "\x00"
        "sromrev=11"                                                         "\x00"
        "boardflags=0x00404001"                                              "\x00"
        "boardflags3=0x08000000"                                             "\x00"
        "xtalfreq=37400"                                                     "\x00"
        "nocrc=1"                                                            "\x00"
        "ag0=255"                                                            "\x00"
        "aa2g=1"                                                             "\x00"
        "ccode=ALL"                                                          "\x00"
        "pa0itssit=0x20"                                                     "\x00"
        "extpagain2g=0"                                                      "\x00"
        "pa2ga0=-168,6777,-789"                                              "\x00"
        "AvVmid_c0=0x0,0xc8"                                                 "\x00"
        "AvVmidIQcal=0x2,0xa8"                                               "\x00"
        "cckpwroffset0=5"                                                    "\x00"
        "maxp2ga0=74"                                                        "\x00"
        "txpwrbckof=6"                                                       "\x00"
        "cckbw202gpo=0"                                                      "\x00"
        "legofdmbw202gpo=0x88888888"                                         "\x00"
        "mcsbw202gpo=0xaaaaaaaa"                                             "\x00"
        "propbw202gpo=0xdd"                                                  "\x00"
        "ofdmdigfilttype=18"                                                 "\x00"
        "ofdmdigfilttypebe=18"                                               "\x00"
        "papdmode=1"                                                         "\x00"
        "papdvalidtest=1"                                                    "\x00"
        "pacalidx2g=45"                                                      "\x00"
        "papdepsoffset=-30"                                                  "\x00"
        "papdendidx=58"                                                      "\x00"
        "ltecxmux=0"                                                         "\x00"
        "ltecxpadnum=0x0102"                                                 "\x00"
        "ltecxfnsel=0x44"                                                    "\x00"
        "ltecxgcigpio=0x01"                                                  "\x00"
        "il0macaddr=00:90:4c:c5:12:38"                                       "\x00"
        "wl0id=0x431b"                                                       "\x00"
        "deadman_to=0xffffffff"                                              "\x00"
        "muxenab=0x11"                                                       "\x00" //Note: muxenab=0x11 to enable the mask for UART and HOSTWAKE [DRIVERS-9095]
        "spurconfig=0x3"                                                     "\x00"
        "glitch_based_crsmin=1"                                              "\x00"
        "btc_mode=0"                                                         "\x00"
        "bt_default_ant=0"                                                   "\x00"
        "\x00\x00";
#ifdef __cplusplus
} /*extern "C" */
#endif

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
