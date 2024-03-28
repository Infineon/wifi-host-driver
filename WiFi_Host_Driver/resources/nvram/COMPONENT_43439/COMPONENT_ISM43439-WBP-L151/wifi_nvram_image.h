/*
 * Copyright (c) 2024, Inventek Systems, All Rights Reserved
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
#include "generated_mac_address.txt"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(NVRAM_PWR_16DB)
#pragma message "Warning TX power set for 16dB"
#elif defined(NVRAM_PWR_17DB)
#pragma message "Warning TX power set for 17dB"
#elif defined(NVRAM_PWR_18DB)
#pragma message "Warning TX power set for 18dB"
#elif defined(NVRAM_PWR_19DB)
#pragma message "Warning TX power set for 19dB"
#else
#pragma message "Default TX power set for 20dB"
#endif
/**
 * Character array of NVRAM image
 * Generated for ISM43439-WBP-L151
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
        //#PA parameters for 2.4GHz, measured at CHIP OUTPUT
        "pa2ga0=-168,6777,-789"                                              "\x00"
        "AvVmid_c0=0x0,0xc8"                                                 "\x00"
        "cckpwroffset0=5"                                                    "\x00"
        //# PPR params
#if defined(NVRAM_PWR_16DB)
        "maxp2ga0=64"                                                        "\x00"
#elif defined(NVRAM_PWR_17DB)
        "maxp2ga0=68"                                                        "\x00"
#elif defined(NVRAM_PWR_18DB)
        "maxp2ga0=72"                                                        "\x00"
#elif defined(NVRAM_PWR_19DB)
        "maxp2ga0=76"                                                        "\x00"
#else
        "maxp2ga0=80"                                                        "\x00"
#endif
        "txpwrbckof=6"                                                       "\x00"
        "cckbw202gpo=0"                                                      "\x00"
        "legofdmbw202gpo=0x66111111"                                         "\x00"
        "mcsbw202gpo=0x77711111"                                             "\x00"
        "propbw202gpo=0xdd"                                                  "\x00"
        //# OFDM IIR :
        "ofdmdigfilttype=18"                                                 "\x00"
        "ofdmdigfilttypebe=18"                                               "\x00"
        //# PAPD mode:
        "papdmode=1"                                                         "\x00"
        "papdvalidtest=1"                                                    "\x00"
        "pacalidx2g=45"                                                      "\x00"
        "papdepsoffset=-30"                                                  "\x00"
        "papdendidx=58"                                                      "\x00"
        //# LTECX flags
        "ltecxmux=0"                                                         "\x00"
        "ltecxpadnum=0x0102"                                                 "\x00"
        "ltecxfnsel=0x44"                                                    "\x00"
        "ltecxgcigpio=0x01"                                                  "\x00"
        //"il0macaddr=00:90:4c:c5:12:38"                                       "\x00"
        "wl0id=0x431b"                                                       "\x00"
        "deadman_to=0xffffffff"                                              "\x00"
		 //# muxenab: 0x1 for UART enable, 0x2 for GPIOs, 0x8 for JTAG, 0x10 for HW OOB
        "muxenab=0x11"                                                       "\x00"
        "spurconfig=0x3"                                                     "\x00"
        "glitch_based_crsmin=1"                                              "\x00"
        // # BT COEX Mode 0=Disable, 1=WiFi Priority , 2=BLE Priority
		"btc_mode=1"                                                         "\x00"
        "bt_default_ant=0"                                                   "\x00"
        "\x00\x00";
#ifdef __cplusplus
} /*extern "C" */
#endif

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
