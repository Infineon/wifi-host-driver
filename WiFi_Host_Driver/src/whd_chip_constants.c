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

#include "whd_chip_constants.h"
#include "whd_wlioctl.h"
#include "whd_int.h"
#include "whd_types_int.h"
#include "bus_protocols/whd_chip_reg.h"

/******************************************************
*               Function Definitions
******************************************************/

uint32_t whd_chip_set_chip_id(whd_driver_t whd_driver, uint16_t id)
{
    whd_driver->chip_info.chip_id = id;

    return 0;
}

uint16_t whd_chip_get_chip_id(whd_driver_t whd_driver)
{
    return whd_driver->chip_info.chip_id;
}

whd_result_t whd_chip_get_chanspec_ctl_channel_num(whd_driver_t whd_driver, uint16_t chanspec, uint16_t *ctrl_ch_num)
{
    uint16_t wlan_chip_id = whd_chip_get_chip_id(whd_driver);
    uint16_t bw = chanspec & get_whd_var(whd_driver, CHANSPEC_BW_MASK);
    uint16_t sb = chanspec & get_whd_var(whd_driver, CHANSPEC_CTL_SB_MASK);

    *ctrl_ch_num = CHSPEC_CHANNEL(chanspec);

    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_SUCCESS;
    }
    else
    {
        if (bw == get_whd_var(whd_driver, CHANSPEC_BW_20) )
        {
            return WHD_SUCCESS;
        }
        else if (bw == get_whd_var(whd_driver, CHANSPEC_BW_40) )
        {
            if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_L) )
                *ctrl_ch_num -= CH_10MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_U) )
                *ctrl_ch_num += CH_10MHZ_APART;
            else
            {
                WPRINT_WHD_ERROR( ("Error: Invalid chanspec 0x%04x\n", chanspec) );
                return WHD_BADARG;
            }
        }
        else if (bw == get_whd_var(whd_driver, CHANSPEC_BW_80) )
        {
            if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LL) )
                *ctrl_ch_num -= CH_30MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LU) )
                *ctrl_ch_num -= CH_10MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_UL) )
                *ctrl_ch_num += CH_10MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_UU) )
                *ctrl_ch_num += CH_30MHZ_APART;
            else
            {
                WPRINT_WHD_ERROR( ("Error: Invalid chanspec 0x%04x\n", chanspec) );
                return WHD_BADARG;
            }
        }
        else if (bw == get_whd_var(whd_driver, CHANSPEC_BW_160) )
        {
            if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LLL) )
                *ctrl_ch_num -= CH_70MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LLU) )
                *ctrl_ch_num -= CH_50MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LUL) )
                *ctrl_ch_num -= CH_30MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_LUU) )
                *ctrl_ch_num -= CH_10MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_ULL) )
                *ctrl_ch_num += CH_10MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_ULU) )
                *ctrl_ch_num += CH_30MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_UUL) )
                *ctrl_ch_num += CH_50MHZ_APART;
            else if (sb == get_whd_var(whd_driver, CHANSPEC_CTL_SB_UUU) )
                *ctrl_ch_num += CH_70MHZ_APART;
            else
            {
                WPRINT_WHD_ERROR( ("Error: Invalid chanspec 0x%04x\n", chanspec) );
                return WHD_BADARG;
            }
        }
        else
        {
            WPRINT_WHD_ERROR( ("Error: Invalid chanspec 0x%04x\n", chanspec) );
            return WHD_BADARG;
        }
    }

    return WHD_SUCCESS;
}

uint32_t get_whd_var(whd_driver_t whd_driver, chip_var_t var)
{
    uint32_t val = 0;

    uint16_t wlan_chip_id = whd_chip_get_chip_id(whd_driver);
    switch (var)
    {
        case ARM_CORE_BASE_ADDRESS:
            CHECK_RETURN(get_arm_core_base_address(wlan_chip_id, &val) );
            break;
        case SOCSRAM_BASE_ADDRESS:
            CHECK_RETURN(get_socsram_base_address(wlan_chip_id, &val, WHD_FALSE) );
            break;
        case SOCSRAM_WRAPPER_BASE_ADDRESS:
            CHECK_RETURN(get_socsram_base_address(wlan_chip_id, &val, WHD_TRUE) );
            break;
        case SDIOD_CORE_BASE_ADDRESS:
            CHECK_RETURN(get_sdiod_core_base_address(wlan_chip_id, &val) );
            break;
        case PMU_BASE_ADDRESS:
            CHECK_RETURN(get_pmu_base_address(wlan_chip_id, &val) );
            break;
        case CHIP_RAM_SIZE:
            CHECK_RETURN(get_chip_ram_size(wlan_chip_id, &val) );
            break;
        case ATCM_RAM_BASE_ADDRESS:
            CHECK_RETURN(get_atcm_ram_base_address(wlan_chip_id, &val) );
            break;
        case SOCRAM_SRMEM_SIZE:
            CHECK_RETURN(get_socsram_srmem_size(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BAND_MASK:
            CHECK_RETURN(get_wl_chanspec_band_mask(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BAND_2G:
            CHECK_RETURN(get_wl_chanspec_band_2G(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BAND_5G:
            CHECK_RETURN(get_wl_chanspec_band_5G(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BAND_6G:
            CHECK_RETURN(get_wl_chanspec_band_6G(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BAND_SHIFT:
            CHECK_RETURN(get_wl_chanspec_band_shift(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_10:
            CHECK_RETURN(get_wl_chanspec_bw_10(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_20:
            CHECK_RETURN(get_wl_chanspec_bw_20(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_40:
            CHECK_RETURN(get_wl_chanspec_bw_40(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_80:
            CHECK_RETURN(get_wl_chanspec_bw_80(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_160:
            CHECK_RETURN(get_wl_chanspec_bw_160(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_MASK:
            CHECK_RETURN(get_wl_chanspec_bw_mask(wlan_chip_id, &val) );
            break;
        case CHANSPEC_BW_SHIFT:
            CHECK_RETURN(get_wl_chanspec_bw_shift(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_NONE:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_none(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_LLL:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_LLL(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_LLU:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_LLU(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_LUL:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_LUL(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_LUU:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_LUU(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_ULL:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_ULL(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_ULU:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_ULU(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_UUL:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_UUL(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_UUU:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_UUU(wlan_chip_id, &val) );
            break;
        case CHANSPEC_CTL_SB_MASK:
            CHECK_RETURN(get_wl_chanspec_ctl_sb_mask(wlan_chip_id, &val) );
            break;
        case NVRAM_DNLD_ADDR:
            CHECK_RETURN(get_nvram_dwnld_start_address(wlan_chip_id, &val) );
            break;
        default:
            break;
    }
    return val;
}

whd_result_t get_arm_core_base_address(uint16_t wlan_chip_id, uint32_t *addr)
{
    switch (wlan_chip_id)
    {
        case 0x4373:
        case 55560:
        case 55500:
            *addr = 0x18002000 + WRAPPER_REGISTER_OFFSET;
            break;
        case 43012:
        case 43022:
        case 43430:
        case 43439:
            *addr = 0x18003000 + WRAPPER_REGISTER_OFFSET;
            break;
        case 43909:
        case 43907:
        case 54907:
            *addr = 0x18011000 + WRAPPER_REGISTER_OFFSET;
            break;
        default:
            return WHD_BADARG;
    }
    return WHD_SUCCESS;
}

whd_result_t get_socsram_base_address(uint16_t wlan_chip_id, uint32_t *addr, whd_bool_t wrapper)
{
    uint32_t offset = 0;
    if (wrapper)
    {
        offset = WRAPPER_REGISTER_OFFSET;
    }
    switch (wlan_chip_id)
    {
        case 43012:
        case 43022:
        case 43430:
        case 43439:
            *addr = 0x18004000 + offset;
            break;
        default:
            return WHD_BADARG;
    }
    return WHD_SUCCESS;
}

whd_result_t get_sdiod_core_base_address(uint16_t wlan_chip_id, uint32_t *addr)
{
    switch (wlan_chip_id)
    {
        case 55560:
            *addr = 0x18004000;
            break;
        case 55500:
            *addr = 0x18003000;
            break;
        case 0x4373:
            *addr = 0x18005000;
            break;
        case 43012:
        case 43022:
        case 43430:
        case 43439:
            *addr = 0x18002000;
            break;
        default:
            return WHD_BADARG;
    }
    return WHD_SUCCESS;
}

whd_result_t get_pmu_base_address(uint16_t wlan_chip_id, uint32_t *addr)
{
    switch (wlan_chip_id)
    {
        case 0x4373:
        case 43430:
        case 43439:
            *addr = CHIPCOMMON_BASE_ADDRESS;
            break;
        case 43012:
        case 43022:
        case 55560:
        case 55500:
            *addr = 0x18012000;
            break;
        case 43909:
        case 43907:
        case 54907:
            *addr = 0x18011000;
            break;
        default:
            return WHD_BADARG;
    }
    return WHD_SUCCESS;
}

whd_result_t get_chip_ram_size(uint16_t wlan_chip_id, uint32_t *size)
{
    *size = 0;
    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) || (wlan_chip_id == 43430) ||
         (wlan_chip_id == 43439) )
    {
        *size = (512 * 1024);
    }
    else if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4390) )
    {
        *size = 0x3C000;
    }
    else if ( (wlan_chip_id == 43909) || (wlan_chip_id == 43907) || (wlan_chip_id == 54907) )
    {
        *size = 0x90000;
    }
    else if ( (wlan_chip_id == 43012) || (wlan_chip_id == 43022) )
    {
        *size = 0xA0000;
    }
    else if (wlan_chip_id == 0x4373)
    {
        *size = 0xE0000;
    }
    else if (wlan_chip_id == 55560)
    {
        *size = 0x150000 - 0x800 - 0x2b4;
    }
#ifdef TRXV5
    else if ((wlan_chip_id == 55500) || (wlan_chip_id == 55900)) /* To be changed when CP is ROMed the ChipID is only 55900 */
    {
        *size = 0xE0000 - 0x20;
    }
#else
    else if (wlan_chip_id == 55500)
    {
        *size = 0xE0000 - 0x800 - 0x2b4;
    }
#endif
    else
    {
        *size = 0x80000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_atcm_ram_base_address(uint16_t wlan_chip_id, uint32_t *size)
{
    *size = 0;
    if (wlan_chip_id == 0x4373)
    {
        *size = 0x160000;
    }
    else if ( (wlan_chip_id == 43909) || (wlan_chip_id == 43907) || (wlan_chip_id == 54907) )
    {
        *size = 0x1B0000;
    }
    else if (wlan_chip_id == 55560)
    {
        *size = 0x370000 + 0x2b4 + 0x800;
    }
#ifdef TRXV5
    else if ((wlan_chip_id == 55500) || (wlan_chip_id == 55900)) /* To be changed when CP is ROMed the ChipID is only 55900 */
    {
        *size = 0x3a0000 + 0x20;
    }
#else
    else if (wlan_chip_id == 55500)
    {
        *size = 0x3a0000 + 0x2b4 + 0x800;
    }
#endif
    else
    {
        *size = 0;
    }
    return WHD_SUCCESS;
}

whd_result_t get_socsram_srmem_size(uint16_t wlan_chip_id, uint32_t *mem_size)
{
    *mem_size = 0;
    if ( (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *mem_size = (32 * 1024);
    }
    else if ( (wlan_chip_id == 43430) || (wlan_chip_id == 43439) )
    {
        *mem_size = (64 * 1024);
    }
    else
    {
        *mem_size = 0;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_band_mask(uint16_t wlan_chip_id, uint32_t *band_mask)
{
    *band_mask = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *band_mask = 0xf000;
    }
    else
    {
        *band_mask = 0xc000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_band_2G(uint16_t wlan_chip_id, uint32_t *band_2g)
{
    *band_2g = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *band_2g = 0x2000;
    }
    else
    {
        *band_2g = 0x0000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_band_5G(uint16_t wlan_chip_id, uint32_t *band_5g)
{
    *band_5g = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *band_5g = 0x1000;
    }
    else
    {
        *band_5g = 0xc000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_band_6G(uint16_t wlan_chip_id, uint32_t *band_6g)
{
    *band_6g = 0;
    if ( (wlan_chip_id == 55500) || (wlan_chip_id == 55560) )
    {
        *band_6g = 0x8000;
    }
    else
    {
        *band_6g = 0;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_band_shift(uint16_t wlan_chip_id, uint32_t *band_shift)
{
    *band_shift = 0;
    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *band_shift = 12;
    }
    else
    {
        *band_shift = 14;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_10(uint16_t wlan_chip_id, uint32_t *bw_10)
{
    *bw_10 = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *bw_10 = 0x0400;
    }
    else
    {
        *bw_10 = 0x0800;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_20(uint16_t wlan_chip_id, uint32_t *bw_20)
{
    *bw_20 = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *bw_20 = 0x0800;
    }
    else
    {
        *bw_20 = 0x1000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_40(uint16_t wlan_chip_id, uint32_t *bw_40)
{
    *bw_40 = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *bw_40 = 0x0C00;
    }
    else
    {
        *bw_40 = 0x1800;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_80(uint16_t wlan_chip_id, uint32_t *bw_80)
{
    *bw_80 = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *bw_80 = 0x2000;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_160(uint16_t wlan_chip_id, uint32_t *bw_160)
{
    *bw_160 = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *bw_160 = 0x2800;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_mask(uint16_t wlan_chip_id, uint32_t *bw_mask)
{
    *bw_mask = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *bw_mask = 0x0C00;
    }
    else
    {
        *bw_mask = 0x3800;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_bw_shift(uint16_t wlan_chip_id, uint32_t *bw_shift)
{
    *bw_shift = 0;
    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *bw_shift = 10;
    }
    else
    {
        *bw_shift = 11;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_none(uint16_t wlan_chip_id, uint32_t *sb_none)
{
    *sb_none = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *sb_none = 0x0300;
    }
    else
    {
        *sb_none = WL_CHANSPEC_CTL_SB_LLL;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_LLL(uint16_t wlan_chip_id, uint32_t *sb_lll)
{
    *sb_lll = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *sb_lll = 0x0100;
    }
    else
    {
        *sb_lll = WL_CHANSPEC_CTL_SB_LLL;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_LLU(uint16_t wlan_chip_id, uint32_t *sb_llu)
{
    *sb_llu = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *sb_llu = 0x0200;
    }
    else
    {
        *sb_llu = WL_CHANSPEC_CTL_SB_LLU;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_LUL(uint16_t wlan_chip_id, uint32_t *sb_lul)
{
    *sb_lul = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_lul = WL_CHANSPEC_CTL_SB_LUL;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_LUU(uint16_t wlan_chip_id, uint32_t *sb_luu)
{
    *sb_luu = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_luu = WL_CHANSPEC_CTL_SB_LUU;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_ULL(uint16_t wlan_chip_id, uint32_t *sb_ull)
{
    *sb_ull = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_ull = WL_CHANSPEC_CTL_SB_ULL;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_ULU(uint16_t wlan_chip_id, uint32_t *sb_ulu)
{
    *sb_ulu = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_ulu = WL_CHANSPEC_CTL_SB_ULU;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_UUL(uint16_t wlan_chip_id, uint32_t *sb_uul)
{
    *sb_uul = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_uul = WL_CHANSPEC_CTL_SB_UUL;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_UUU(uint16_t wlan_chip_id, uint32_t *sb_uuu)
{
    *sb_uuu = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        return WHD_UNSUPPORTED;
    }
    else
    {
        *sb_uuu = WL_CHANSPEC_CTL_SB_UUU;
    }
    return WHD_SUCCESS;
}

whd_result_t get_wl_chanspec_ctl_sb_mask(uint16_t wlan_chip_id, uint32_t *sb_mask)
{
    *sb_mask = 0;
    if ( (wlan_chip_id == 43362) || (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *sb_mask = 0x0300;
    }
    else
    {
        *sb_mask = 0x0700;
    }
    return WHD_SUCCESS;
}

whd_result_t get_nvram_dwnld_start_address(uint16_t wlan_chip_id, uint32_t *addr)
{
    switch (wlan_chip_id)
    {
        case 43022:
            *addr = 0x80000;
            break;
        default:
            return WHD_BADARG;
    }
    return WHD_SUCCESS;
}
