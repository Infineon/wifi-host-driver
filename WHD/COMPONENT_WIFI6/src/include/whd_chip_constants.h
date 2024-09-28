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

#ifndef INCLUDED_CHIP_CONSTANTS_H_
#define INCLUDED_CHIP_CONSTANTS_H_

#include "whd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WRAPPER_REGISTER_OFFSET     (0x100000)
typedef enum chip_var
{
    ARM_CORE_BASE_ADDRESS = 1,
    SOCSRAM_BASE_ADDRESS,
    SOCSRAM_WRAPPER_BASE_ADDRESS,
    SDIOD_CORE_BASE_ADDRESS,
    PMU_BASE_ADDRESS,
    CHIP_RAM_SIZE,
    ATCM_RAM_BASE_ADDRESS,
    SOCRAM_SRMEM_SIZE,
    CHANSPEC_BAND_MASK,
    CHANSPEC_BAND_2G,
    CHANSPEC_BAND_5G,
    CHANSPEC_BAND_6G,
    CHANSPEC_BAND_SHIFT,
    CHANSPEC_BW_10,
    CHANSPEC_BW_20,
    CHANSPEC_BW_40,
    CHANSPEC_BW_80,
    CHANSPEC_BW_160,
    CHANSPEC_BW_MASK,
    CHANSPEC_BW_SHIFT,
    CHANSPEC_CTL_SB_NONE,
    CHANSPEC_CTL_SB_LLL,
    CHANSPEC_CTL_SB_LLU,
    CHANSPEC_CTL_SB_LUL,
    CHANSPEC_CTL_SB_LUU,
    CHANSPEC_CTL_SB_ULL,
    CHANSPEC_CTL_SB_ULU,
    CHANSPEC_CTL_SB_UUL,
    CHANSPEC_CTL_SB_UUU,
    CHANSPEC_CTL_SB_MASK,
    NVRAM_DNLD_ADDR
} chip_var_t;
#define CHANSPEC_CTL_SB_LL          CHANSPEC_CTL_SB_LLL
#define CHANSPEC_CTL_SB_LU          CHANSPEC_CTL_SB_LLU
#define CHANSPEC_CTL_SB_UL          CHANSPEC_CTL_SB_LUL
#define CHANSPEC_CTL_SB_UU          CHANSPEC_CTL_SB_LUU
#define CHANSPEC_CTL_SB_L           CHANSPEC_CTL_SB_LLL
#define CHANSPEC_CTL_SB_U           CHANSPEC_CTL_SB_LLU

#define VERIFY_RESULT(x) { whd_result_t verify_result = WHD_SUCCESS; verify_result = (x); \
                           if (verify_result != WHD_SUCCESS){ \
                               WPRINT_WHD_ERROR( ("Function %s failed at line %d \n", __func__, __LINE__) ); \
                               return verify_result; } }
#define GET_C_VAR(whd_driver, var) get_whd_var(whd_driver, var)

#define WL_CHANSPEC_CHAN_MASK           (0x00ff)
#define CHSPEC_IS6G(chspec)          ( (chspec & \
                                        GET_C_VAR(whd_driver, \
                                                  CHANSPEC_BAND_MASK) ) == GET_C_VAR(whd_driver, CHANSPEC_BAND_6G) )
#define CHSPEC_IS5G(chspec)          ( (chspec & \
                                        GET_C_VAR(whd_driver, \
                                                  CHANSPEC_BAND_MASK) ) == GET_C_VAR(whd_driver, CHANSPEC_BAND_5G) )
#define CHSPEC_IS2G(chspec)          ( (chspec & \
                                        GET_C_VAR(whd_driver, \
                                                  CHANSPEC_BAND_MASK) ) == GET_C_VAR(whd_driver, CHANSPEC_BAND_2G) )
#define CHSPEC_CHANNEL(chspec)          ( (chanspec_t)( (chspec) & WL_CHANSPEC_CHAN_MASK ) )
#define CH20MHZ_CHSPEC(chspec)          (chanspec_t)( (chanspec_t)CHSPEC_CHANNEL(chspec) | \
                                                      GET_C_VAR(whd_driver, CHANSPEC_BW_20) | \
                                                      GET_C_VAR(whd_driver, CHANSPEC_CTL_SB_NONE) | \
                                                      (CHSPEC_IS2G(chspec) ? GET_C_VAR(whd_driver, CHANSPEC_BAND_2G) \
                                                       : (CHSPEC_IS5G(chspec) ? GET_C_VAR(whd_driver, CHANSPEC_BAND_5G) \
                                                          : GET_C_VAR(whd_driver, CHANSPEC_BAND_6G) ) ) )
#define CH_70MHZ_APART              14
#define CH_50MHZ_APART              10
#define CH_30MHZ_APART              6
#define CH_20MHZ_APART              4
#define CH_10MHZ_APART              2
#define CH_5MHZ_APART               1 /* 2G band channels are 5 Mhz apart */

uint32_t get_whd_var(whd_driver_t whd_driver, chip_var_t var);

whd_result_t get_arm_core_base_address(uint16_t, uint32_t *);
whd_result_t get_socsram_base_address(uint16_t, uint32_t *, whd_bool_t);
whd_result_t get_sdiod_core_base_address(uint16_t, uint32_t *);
whd_result_t get_pmu_base_address(uint16_t, uint32_t *);
whd_result_t get_chip_ram_size(uint16_t, uint32_t *);
whd_result_t get_atcm_ram_base_address(uint16_t, uint32_t *);
whd_result_t get_socsram_srmem_size(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_band_mask(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_band_2G(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_band_5G(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_band_6G(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_band_shift(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_10(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_20(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_40(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_80(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_160(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_mask(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_bw_shift(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_none(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_LLL(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_LLU(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_LUL(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_LUU(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_ULL(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_ULU(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_UUL(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_UUU(uint16_t, uint32_t *);
whd_result_t get_wl_chanspec_ctl_sb_mask(uint16_t, uint32_t *);
whd_result_t get_nvram_dwnld_start_address(uint16_t wlan_chip_id, uint32_t *addr);

uint32_t whd_chip_set_chip_id(whd_driver_t whd_driver, uint16_t id);
uint16_t whd_chip_get_chip_id(whd_driver_t whd_driver);
uint32_t whd_chip_set_chiprev_id(whd_driver_t whd_driver, uint8_t id);
uint8_t whd_chip_get_chiprev_id(whd_driver_t whd_driver);

whd_result_t whd_chip_get_chanspec_ctl_channel_num(whd_driver_t whd_driver, uint16_t chanspec, uint16_t *ctrl_ch_num);

#undef CHIP_FIRMWARE_SUPPORTS_PM_LIMIT_IOVAR

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef INCLUDED_CHIP_CONSTANTS_H_ */
