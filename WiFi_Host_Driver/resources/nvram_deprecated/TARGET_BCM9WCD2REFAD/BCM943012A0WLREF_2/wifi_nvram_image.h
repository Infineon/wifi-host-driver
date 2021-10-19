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

/** @file
 *  NVRAM variables taken from bcm943012fcbga_hwoob.txt nvram file (2.4 GHz, 20 MHz BW mode)
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
 */

static const char wifi_nvram_image[] =
    // # Sample variables file for BCM943012 BU board
    "NVRAMRev=$Rev: 351687 $"                                           "\x00"
    "sromrev=11"                                                        "\x00"
    "cckdigfilttype=4"                                                  "\x00"
    "bphyscale=0x28"                                                    "\x00"
    "boardflags3=0x40000101"                                            "\x00"
    "vendid=0x14e4"                                                     "\x00"
    "devid=0xA804"                                                      "\x00"
    "manfid=0x2d0"                                                      "\x00"
    "prodid=0x052e"                                                     "\x00"
    NVRAM_GENERATED_MAC_ADDRESS                                         "\x00"
    "nocrc=1"                                                           "\x00"
    "muxenab=0x1"                                                       "\x00"
    "boardtype=0x07d7"                                                  "\x00"
    "boardrev=0x1201"                                                   "\x00"
    "lpflags=0x00000000"                                                "\x00"
    "xtalfreq=37400"                                                    "\x00"
    "boardflags2=0x00000000"                                            "\x00"
    "boardflags=0x00000000"                                             "\x00"
    "extpagain2g=1"                                                     "\x00"
    "extpagain5g=1"                                                     "\x00"
    "ccode=0"                                                           "\x00"
    "regrev=0"                                                          "\x00"
    "antswitch=0"                                                       "\x00"
    "rxgains2gelnagaina0=0"                                             "\x00"
    "rxgains2gtrisoa0=15"                                               "\x00"
    "rxgains2gtrelnabypa0=0"                                            "\x00"
    "rxgains5gelnagaina0=0"                                             "\x00"
    "rxgains5gtrisoa0=9"                                                "\x00"
    "rxgains5gtrelnabypa0=0"                                            "\x00"
    "pdgain5g=0"                                                        "\x00"
    "pdgain2g=0"                                                        "\x00"
    "tworangetssi2g=0"                                                  "\x00"
    "tworangetssi5g=0"                                                  "\x00"
    "rxchain=1"                                                         "\x00"
    "txchain=1"                                                         "\x00"
    "aa2g=1"                                                            "\x00"
    "aa5g=1"                                                            "\x00"
    "tssipos5g=0"                                                       "\x00"
    "tssipos2g=0"                                                       "\x00"
    "tssisleep_en=0x5"                                                  "\x00"
    "femctrl=17"                                                        "\x00"
    "subband5gver=4"                                                    "\x00"
    "pa2ga0=-216,6052,-751"                                             "\x00"
    "pa5ga0=-41,7410,-961,-48,7513,-965,-54,7673,-976,-54,8076,-1011"  "\x00"
    "cckpwroffset0=3"                                                   "\x00"
    "pdoffset40ma0=0"                                                   "\x00"
    "pdoffset80ma0=0"                                                   "\x00"
    "lowpowerrange2g=0"                                                 "\x00"
    "lowpowerrange5g=0"                                                 "\x00"
    "ed_thresh2g=-63"                                                   "\x00"
    "ed_thresh5g=-63"                                                   "\x00"
    "swctrlmap_2g=0x00080008,0x00000001,0x00000001,0x000000,0x3ff"       "\x00"
    "swctrlmapext_2g=0x00000000,0x00000000,0x00000000,0x010001,0x000"   "\x00"
    "swctrlmap_5g=0x00000004,0x00000001,0x00000001,0x000000,0x3ff"      "\x00"
    "swctrlmapext_5g=0x00000000,0x00000000,0x00000000,0x010001,0x000"   "\x00"
    "ulpnap=0"                                                          "\x00"
    "ulpadc=1"                                                          "\x00"
    "ssagc_en=0"                                                        "\x00"
    "ds1_nap=0"                                                         "\x00"
    "epacal2g=1"                                                        "\x00"
    "epacal2g_mask=0x3fff"                                              "\x00"
    "maxp2ga0=72"                                                       "\x00"
    "ofdmlrbw202gpo=0x0011"                                             "\x00"
    "dot11agofdmhrbw202gpo=0x7744"                                      "\x00"
    "mcsbw202gpo=0x99777441"                                            "\x00"
    "rssicorrnorm_c0=-6"                                                "\x00"
    "rssicorrnorm5g_c0=-5,0,0,-4,0,0,-4,0,0,-6,0,0"                     "\x00"
    "mac_clkgating=1"                                                   "\x00"
    //#mcsbw402gpo=0x99555533
    "maxp5ga0=76,76,76,76"                                              "\x00"
    "mcsbw205glpo=0x99777441"                                           "\x00"
    "mcsbw205gmpo=0x99777441"                                           "\x00"
    "mcsbw205ghpo=0x99777441"                                           "\x00"
    //#mcsbw405glpo=0x99555000
    //#mcsbw405gmpo=0x99555000
    //#mcsbw405ghpo=0x99555000
    //#mcsbw805glpo=0x99555000
    //#mcsbw805gmpo=0x99555000
    //#mcsbw805ghpo=0x99555000
    "txwbpapden=1"                                                      "\x00"
    "femctrlwar=1"                                                      "\x00"
    "use5gpllfor2g=0"                                                      "\x00"

//#tx papd cal params
    "wb_rxattn=0x0302"                                                  "\x00"
    "wb_txattn=0x0606"                                                  "\x00"
    "wb_papdcalidx=0x0814"                                              "\x00"
    "wb_eps_offset=0x01c501c5"                                          "\x00"
    "wb_bbmult=0x5A40"                                                  "\x00"
    "wb_calref_db=0x121a"                                               "\x00"
    "wb_tia_gain_mode=0x0606"                                           "\x00"
    "wb_frac_del=0x4B78"                                           "\x00"

    "nb_rxattn=0x0202"                                                  "\x00"
    "nb_txattn=0x0606"                                                  "\x00"
    "nb_papdcalidx=0x0c0c"                                              "\x00"
    "nb_eps_offset=0x01d701d7"                                          "\x00"
    "nb_bbmult=0x5A5A"                                                  "\x00"
    "nb_tia_gain_mode=0x0606"                                           "\x00"
    "lpo_select=4"                                                      "\x00"
    "\x00\x00";


#ifdef __cplusplus
} /* extern "C" */
#endif

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
