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
 * Generated from cyw943022wlref_latest.txt
 */

static const char wifi_nvram_image[] =
        "NVRAMRev=$Rev: 723327 $"                                            "\x00"
        "sromrev=11"                                                         "\x00"
        "etmode=0x11"                                                        "\x00"
        "cckdigfilttype=4"                                                   "\x00"
        "bphyscale=0x16"                                                     "\x00"
        "boardflags3=0x4000C101"                                             "\x00"
        "vendid=0x12be "                                                     "\x00"
        "devid=0xbd3f"                                                       "\x00"
        "manfid=0x2d0"                                                       "\x00"
        "prodid=0x052e"                                                      "\x00"
        NVRAM_GENERATED_MAC_ADDRESS                                          "\x00"
        "nocrc=1"                                                            "\x00"
        "boardtype=0x08a6"                                                   "\x00"
        "boardrev=0x1100"                                                    "\x00"
        "lpflags=0x00000"                                                    "\x00"
        "xtalfreq=37400"                                                     "\x00"
        "boardflags2=0xc0800000"                                             "\x00"
        "boardflags=0x00400001"                                              "\x00"
        "extpagain2g=2"                                                      "\x00"
        "extpagain5g=2"                                                      "\x00"
        "ccode=0 "                                                           "\x00"
        "regrev=0 "                                                          "\x00"
        "antswitch = 0"                                                      "\x00"
        "rxgains2gelnagaina0=0"                                              "\x00"
        "rxgains2gtrisoa0=15"                                                "\x00"
        "rxgains2gtrelnabypa0=0"                                             "\x00"
        "rxgains5gelnagaina0=0"                                              "\x00"
        "rxgains5gtrisoa0=9"                                                 "\x00"
        "rxgains5gtrelnabypa0=0"                                             "\x00"
        "pdgain5g=0"                                                         "\x00"
        "pdgain2g=0"                                                         "\x00"
        "tworangetssi2g=0"                                                   "\x00"
        "tworangetssi5g=0"                                                   "\x00"
        "rxchain=1"                                                          "\x00"
        "txchain=1"                                                          "\x00"
        "aa2g=1"                                                             "\x00"
        "aa5g=1"                                                             "\x00"
        "tssipos5g=0"                                                        "\x00"
        "tssipos2g=0"                                                        "\x00"
        "tssisleep_en=0x5"                                                   "\x00"
        "femctrl=17"                                                         "\x00"
        "subband5gver=4"                                                     "\x00"
        "pa2ga0=-162,6070,-755"                                              "\x00"
        "pa5ga0=-147,5658,-685,-124,5923,-699,-134,5801,-684,-148,5803,-716" "\x00"
        "cckpwroffset0=2"                                                    "\x00"
        "pdoffset40ma0=0"                                                    "\x00"
        "pdoffset80ma0=0"                                                    "\x00"
        "lowpowerrange2g=0"                                                  "\x00"
        "lowpowerrange5g=0"                                                  "\x00"
        "rssicorrnorm_c0=-10,0"                                              "\x00"
        "rssicorrnorm5g_c0=4,0,0,-5,0,0,-5,0,0,-9,0,0"                       "\x00"
        "ed_thresh2g=-63"                                                    "\x00"
        "ed_thresh5g=-63"                                                    "\x00"
        "swctrlmap_2g=0x20002000,0x60406040, 0x20002000,0x000040,0x1e7"      "\x00"
        "swctrlmapext_2g=0x02020202,0x00000000, 0x02020202,0x000000,0x003"   "\x00"
        "swctrlmap_5g=0x20002000,0x20002000,0x20002000,0x000000,0x1a7"       "\x00"
        "swctrlmapext_5g=0x00000000,0x00010101, 0x00010001,0x000000,0x003"   "\x00"
        "ulpnap=0"                                                           "\x00"
        "ulpadc=1"                                                           "\x00"
        "ssagc_en=0"                                                         "\x00"
        "ds1_nap=0"                                                          "\x00"
        "epacal2g=0"                                                         "\x00"
        "epacal5g=0"                                                         "\x00"
        "epacal2g_mask=0x3fff"                                               "\x00"
        "maxp2ga0=82"                                                        "\x00"
        "ofdmlrbw202gpo=0x0077"                                              "\x00"
        "dot11agofdmhrbw202gpo=0x8888"                                       "\x00"
        "mcsbw202gpo=0x99999999"                                             "\x00"
        "mac_clkgating=1"                                                    "\x00"
        "maxp5ga0=82,82,82,82"                                               "\x00"
        "mcsbw205glpo=0xffc97777"                                            "\x00"
        "mcsbw205gmpo=0xffc97777"                                            "\x00"
        "mcsbw205ghpo=0xffc97777"                                            "\x00"
        "txwbpapden=1"                                                       "\x00"
        "femctrlwar=0"                                                       "\x00"
        "use5gpllfor2g=1"                                                    "\x00"
        "wb_rxattn=0x0303"                                                   "\x00"
        "wb_txattn=0x0202"                                                   "\x00"
        "wb_papdcalidx=0x1015"                                               "\x00"
        "wb_papdcalidx_5g=0x00101010"                                        "\x00"
        "wb_eps_offset=0x01bf01af"                                           "\x00"
        "wb_eps_offset_5g=0x01bf01bf"                                        "\x00"
        "wb_bbmult=0x67"                                                     "\x00"
        "wb_bbmult_5g=0x004f5052"                                            "\x00"
        "wb_calref_db=0x1926"                                                "\x00"
        "wb_tia_gain_mode=0x0606"                                            "\x00"
        "wb_txbuf_offset=0x1e1e"                                             "\x00"
        "wb_frac_del=0x78ae"                                                 "\x00"
        "wb_g_frac_bits=0xaa"                                                "\x00"
        "nb_rxattn=0x0403"                                                   "\x00"
        "nb_txattn=0x0402"                                                   "\x00"
        "nb_papdcalidx= 0x1405"                                              "\x00"
        "nb_eps_offset= 0x01d701ca"                                          "\x00"
        "nb_bbmult= 0x5A50"                                                  "\x00"
        "nb_tia_gain_mode=0x0006"                                            "\x00"
        "AvVmid_c0=4,140,7,80,7,80,7,80,7,80"                                "\x00"
        "lpo_select=4"                                                       "\x00"
        "csml=0x10"                                                          "\x00"
        "pt5db_gaintbl=0"                                                    "\x00"
        "papdcck=0"                                                          "\x00"
        "paprrmcsgamma2g=450,500,550,600,675,950,950,950,950,950,950,950"    "\x00"
        "paprrmcsgamma5g20=600,600,600,600,800,950,1100,1100,1100,1100,1100,1100""\x00"
        "lhlgpio_cfg=0x00000020,0x00000000"                                  "\x00"
        "txidxcap2g=10"                                                      "\x00"
        "txidxcap5g=5"                                                       "\x00"
        "initbaseidx2govrval=30"                                             "\x00"
        "initbaseidx5govrval=30"                                             "\x00"
        "swdiv_en=1 "                                                        "\x00"
        "swdiv_gpio=0"                                                       "\x00"
        "swdiv_swctrl_en=2"                                                  "\x00"
        "swdiv_swctrl_ant0=0"                                                "\x00"
        "swdiv_swctrl_ant1=1"                                                "\x00"
        "swdiv_antmap2g_main=1"                                              "\x00"
        "swdiv_antmap5g_main=1"                                              "\x00"
        "swdiv_snrlim=290 "                                                  "\x00"
        "swdiv_thresh=2000 "                                                 "\x00"
        "swdiv_snrthresh=24 "                                                "\x00"
        "swdiv_timeon=10"                                                    "\x00"
        "swdiv_timeoff=1"                                                    "\x00"
        "swdiv_snr2g20=232"                                                  "\x00"
        "swdiv_snr2g40=257"                                                  "\x00"
        "swdiv_snr5g20=296"                                                  "\x00"
        "swdiv_snr5g40=312"                                                  "\x00"
        "swdiv_snr5g80=296"                                                  "\x00"
        "swdiv_ap_dead_check=0 "                                             "\x00"
        "swdiv_ap_div=0 "                                                    "\x00"
        "muxenab=0x01"                                                       "\x00"
        "\x00\x00";
#ifdef __cplusplus
} /*extern "C" */
#endif

#else /* ifndef INCLUDED_NVRAM_IMAGE_H_ */

#error Wi-Fi NVRAM image included twice

#endif /* ifndef INCLUDED_NVRAM_IMAGE_H_ */
