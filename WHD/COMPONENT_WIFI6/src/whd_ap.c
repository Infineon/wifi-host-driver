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

/** @file
 *  Provides generic APSTA functionality that chip specific files use
 */
#include "whd_debug.h"
#include "whd_ap.h"
#include "bus_protocols/whd_chip_reg.h"
#include "whd_chip_constants.h"
#include "whd_chip.h"
#ifndef PROTO_MSGBUF
#include "whd_sdpcm.h"
#else
#include "whd_flowring.h"
#endif /* PROTO_MSGBUF */
#include "whd_thread_internal.h"
#include "whd_events_int.h"
#include "whd_int.h"
#include "whd_utils.h"
#include "whd_wifi_api.h"
#include "whd_buffer_api.h"
#include "whd_wlioctl.h"
#include "whd_proto.h"

/******************************************************
** @cond               Constants
*******************************************************/

#define WLC_EVENT_MSG_LINK      (0x01)
#define RATE_SETTING_11_MBPS    (11000000 / 500000)
#define AMPDU_AP_DEFAULT_BA_WSIZE   8
#define AMPDU_STA_DEFAULT_BA_WSIZE  8
#define AMPDU_STA_DEFAULT_MPDU      4   /* STA default num MPDU per AMPDU */
#define PWE_LOOP_COUNT              5
/******************************************************
**                   Enumerations
*******************************************************/
typedef enum
{
    BSS_AP   = 3,
    BSS_STA  = 2,
    BSS_UP   = 1,
    BSS_DOWN = 0
} bss_arg_option_t;

/******************************************************
 *  *               Function Declarations
 *   ******************************************************/
static void *whd_handle_apsta_event(whd_interface_t ifp, const whd_event_header_t *event_header,
                                    const uint8_t *event_data, void *handler_user_data);

/******************************************************
 *        Variables Definitions
 *****************************************************/
static const whd_event_num_t apsta_events[] = { WLC_E_IF, WLC_E_LINK, WLC_E_NONE };
/******************************************************
*               Function Definitions
******************************************************/

void whd_ap_info_init(whd_driver_t whd_driver)
{
    whd_driver->ap_info.ap_is_up = WHD_FALSE;
    whd_driver->ap_info.is_waiting_event = WHD_FALSE;
}

void whd_wifi_set_ap_is_up(whd_driver_t whd_driver, whd_bool_t new_state)
{
    if (whd_driver->ap_info.ap_is_up != new_state)
    {
        whd_driver->ap_info.ap_is_up = new_state;
    }
}

whd_bool_t whd_wifi_get_ap_is_up(whd_driver_t whd_driver)
{
    return whd_driver->ap_info.ap_is_up;
}

#ifdef PROTO_MSGBUF
void whd_wifi_update_addr_mode(whd_driver_t whd_driver, uint8_t idx)
{
    struct whd_msgbuf *msgbuf = whd_driver->msgbuf;
    msgbuf->flow->addr_mode[idx] = ADDR_DIRECT;
}
#endif

whd_result_t whd_wifi_set_block_ack_window_size_common(whd_interface_t ifp, uint16_t ap_win_size, uint16_t sta_win_size)
{
    whd_result_t retval;
    uint16_t block_ack_window_size = ap_win_size;

    /* If the AP interface is already up then don't change the Block Ack window size */
    if (ifp->role == WHD_AP_ROLE)
    {
        return WHD_SUCCESS;
    }

    if (ifp->role == WHD_STA_ROLE)
    {
        block_ack_window_size = sta_win_size;
    }

    retval = whd_wifi_set_iovar_value(ifp, IOVAR_STR_AMPDU_BA_WINDOW_SIZE, ( uint32_t )block_ack_window_size);

    whd_assert("set_block_ack_window_size: Failed to set block ack window size\r\n", retval == WHD_SUCCESS);

    return retval;
}

whd_result_t whd_wifi_set_ampdu_parameters_common(whd_interface_t ifp, uint8_t ba_window_size, int8_t ampdu_mpdu,
                                                  uint8_t rx_factor)
{
    CHECK_RETURN(whd_wifi_set_iovar_value(ifp, IOVAR_STR_AMPDU_BA_WINDOW_SIZE, ba_window_size) );

    /* Set number of MPDUs available for AMPDU */
    CHECK_RETURN(whd_wifi_set_iovar_value(ifp, IOVAR_STR_AMPDU_MPDU, ( uint32_t )ampdu_mpdu) );

    if (rx_factor != AMPDU_RX_FACTOR_INVALID)
    {
        CHECK_RETURN(whd_wifi_set_iovar_value(ifp, IOVAR_STR_AMPDU_RX_FACTOR, rx_factor) );
    }
    return WHD_SUCCESS;
}

/** Sets the chip specific AMPDU parameters for AP and STA
 *  For SDK 3.0, and beyond, each chip will need it's own function for setting AMPDU parameters.
 */
whd_result_t whd_wifi_set_ampdu_parameters(whd_interface_t ifp)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    /* Get the chip number */
    uint16_t wlan_chip_id = whd_chip_get_chip_id(whd_driver);

    if ( (wlan_chip_id == 43012) || (wlan_chip_id == 0x4373) )
    {
        return whd_wifi_set_ampdu_parameters_common(ifp, AMPDU_STA_DEFAULT_BA_WSIZE, AMPDU_MPDU_AUTO,
                                                    AMPDU_RX_FACTOR_64K);
    }
    else if ( (wlan_chip_id == 43909) || (wlan_chip_id == 43907) || (wlan_chip_id == 54907) )
    {
        return whd_wifi_set_ampdu_parameters_common(ifp, AMPDU_STA_DEFAULT_BA_WSIZE, AMPDU_MPDU_AUTO,
                                                    AMPDU_RX_FACTOR_INVALID);
    }
    else
    {
        return whd_wifi_set_ampdu_parameters_common(ifp, AMPDU_STA_DEFAULT_BA_WSIZE, AMPDU_STA_DEFAULT_MPDU,
                                                    AMPDU_RX_FACTOR_8K);
    }
}

/* All chips */
static void *whd_handle_apsta_event(whd_interface_t ifp, const whd_event_header_t *event_header,
                                    const uint8_t *event_data, void *handler_user_data)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    whd_ap_int_info_t *ap;

    UNUSED_PARAMETER(event_header);
    UNUSED_PARAMETER(event_data);
    UNUSED_PARAMETER(handler_user_data);

    ap = &whd_driver->ap_info;

    if (ap->is_waiting_event == WHD_TRUE)
    {
        if ( (event_header->event_type == (whd_event_num_t)WLC_E_LINK) ||
             (event_header->event_type == WLC_E_IF) )
        {
            whd_result_t result;
            result = cy_rtos_set_semaphore(&ap->whd_wifi_sleep_flag, WHD_FALSE);
            WPRINT_WHD_DEBUG( ("%s failed to post AP link semaphore at %d\n", __func__, __LINE__) );
            REFERENCE_DEBUG_ONLY_VARIABLE(result);
        }
    }
    return handler_user_data;
}

/* All chips */
whd_result_t whd_wifi_init_ap(whd_interface_t ifp, whd_ssid_t *ssid, whd_security_t auth_type,
                          const uint8_t *security_key, uint8_t key_length, uint16_t chanspec)
{
    whd_driver_t whd_driver;
    whd_bool_t wait_for_interface = WHD_FALSE;
    whd_result_t result;
    whd_buffer_t response;
    whd_buffer_t buffer;
    whd_interface_t prim_ifp;
    whd_ap_int_info_t *ap;
    uint32_t *data;
    uint32_t bss_index;
    uint16_t wlan_chip_id;
    uint32_t auth_mfp = WL_MFP_NONE;
    uint16_t event_entry = (uint16_t)0xFF;


    CHECK_IFP_NULL(ifp);

    whd_driver = ifp->whd_driver;

    CHECK_DRIVER_NULL(whd_driver);

    /* Get the Chip Number */
    wlan_chip_id = whd_chip_get_chip_id(whd_driver);

    if(((auth_type == WHD_SECURITY_WPA_TKIP_PSK) || (auth_type == WHD_SECURITY_WPA2_TKIP_PSK) || (auth_type == WHD_SECURITY_WPA_TKIP_ENT)) &&
                 ((wlan_chip_id == 55500) || (wlan_chip_id == 55900) || (wlan_chip_id == 55530)))
    {
        WPRINT_WHD_ERROR(("WPA_TKIP security type is not supported for H1 Combo and H1CP , %s failed at line %d \n", __func__, __LINE__));
        return WHD_UNSUPPORTED;
    }

    if ( (auth_type & WEP_ENABLED) != 0 )
    {
        WPRINT_WHD_ERROR( ("WEP auth type is not allowed , %s failed at line %d \n", __func__, __LINE__) );
        return WHD_WEP_NOT_ALLOWED;
    }

    if ( (auth_type & WPA3_SECURITY) &&
         ( (wlan_chip_id == 43430) || (wlan_chip_id == 43909) || (wlan_chip_id == 43907) || (wlan_chip_id == 54907) ||
           (wlan_chip_id == 43012) ) )
    {
        WPRINT_WHD_ERROR( ("WPA3 is not supported, %s failed at line %d \n", __func__, __LINE__) );
        return WHD_UNSUPPORTED;
    }

    ap = &whd_driver->ap_info;

    prim_ifp = whd_get_primary_interface(whd_driver);
    if (prim_ifp == NULL)
    {
        WPRINT_WHD_ERROR( ("%s failed at %d \n", __func__, __LINE__) );
        return WHD_UNKNOWN_INTERFACE;
    }

    if (ssid->length > (size_t)SSID_NAME_SIZE)
    {
        WPRINT_WHD_ERROR( ("%s: failure: SSID length(%u) error\n", __func__, ssid->length) );
        return WHD_WLAN_BADSSIDLEN;
    }

    /* Turn off APSTA when creating AP mode on primary interface */
    if (ifp == prim_ifp)
    {
        CHECK_RETURN(whd_wifi_set_ioctl_buffer(prim_ifp, WLC_DOWN, NULL, 0) );
        data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)4, IOVAR_STR_APSTA);
        CHECK_IOCTL_BUFFER(data);
        *data = 0;
        result = whd_proto_set_iovar(ifp, buffer, 0);
        if ( (result != WHD_SUCCESS) && (result != WHD_WLAN_UNSUPPORTED) )
        {
            WPRINT_WHD_ERROR( ("Could not turn off apsta\n") );
            return result;
        }
        CHECK_RETURN(whd_wifi_set_ioctl_buffer(prim_ifp, WLC_UP, NULL, 0) );
    }

    bss_index = ifp->bsscfgidx;

    ifp->role = WHD_AP_ROLE;

    if ( ( (auth_type == WHD_SECURITY_WPA_TKIP_PSK) || (auth_type == WHD_SECURITY_WPA2_AES_PSK) ||
           (auth_type == WHD_SECURITY_WPA2_MIXED_PSK) ) &&
         ( (key_length < (uint8_t)WSEC_MIN_PSK_LEN) || (key_length > (uint8_t)WSEC_MAX_PSK_LEN) ) )
    {
        WPRINT_WHD_ERROR( ("Error: WPA security key length must be between %d and %d\n", WSEC_MIN_PSK_LEN,
                           WSEC_MAX_PSK_LEN) );
        return WHD_WPA_KEYLEN_BAD;
    }

    if ( (whd_wifi_get_ap_is_up(whd_driver) == WHD_TRUE) )
    {
        WPRINT_WHD_ERROR( ("Error: Soft AP or Wi-Fi Direct group owner already up\n") );
        return WHD_AP_ALREADY_UP;
    }

    /* Query bss state (does it exist? if so is it UP?) */
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)4, IOVAR_STR_BSS);
    CHECK_IOCTL_BUFFER(data);

    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        *data = htod32( (uint32_t)CHIP_AP_INTERFACE );
    }
    else
    {
        *data = htod32( (uint32_t)bss_index );
    }

    if ( (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        if (whd_proto_get_iovar(ifp, buffer, &response) != WHD_SUCCESS)
        {
            /* Note: We don't need to release the response packet since the iovar failed */
            wait_for_interface = WHD_TRUE;
        }
        else
        {
            /* Check if the BSS is already UP, if so return */
            uint32_t *data2 = (uint32_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, response);
            CHECK_PACKET_NULL(data2, WHD_NO_REGISTER_FUNCTION_POINTER);
            *data2 = dtoh32 (*data2);
            if (*data2 == (uint32_t)BSS_UP)
            {
                CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );
                whd_wifi_set_ap_is_up(whd_driver, WHD_TRUE);
                ap->is_waiting_event = WHD_FALSE;
                return WHD_SUCCESS;
            }
            else
            {
                CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );
            }
        }
    }

    if (whd_proto_get_iovar(prim_ifp, buffer, &response) != WHD_SUCCESS)
    {
        /* Note: We don't need to release the response packet since the iovar failed */
        wait_for_interface = WHD_TRUE;
    }
    else
    {
        /* Check if the BSS is already UP, if so return */
        uint32_t *data2 = (uint32_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, response);
        CHECK_PACKET_NULL(data2, WHD_NO_REGISTER_FUNCTION_POINTER);
        *data2 = dtoh32 (*data2);
        if (*data2 == (uint32_t)BSS_UP)
        {
            CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );
            whd_wifi_set_ap_is_up(whd_driver, WHD_TRUE);
            ap->is_waiting_event = WHD_FALSE;
            return WHD_SUCCESS;
        }
        else
        {
            CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );
        }
    }

    CHECK_RETURN(cy_rtos_init_semaphore(&ap->whd_wifi_sleep_flag, 1, 0) );

    ap->is_waiting_event = WHD_TRUE;
    /* Register for interested events */
    CHECK_RETURN_WITH_SEMAPHORE(whd_management_set_event_handler(ifp, apsta_events, whd_handle_apsta_event,
                                                                 NULL, &event_entry), &ap->whd_wifi_sleep_flag);
    if (event_entry >= WHD_EVENT_HANDLER_LIST_SIZE)
    {
        WPRINT_WHD_DEBUG( ("Event handler registration failed for AP events in function %s and line %d\n",
                           __func__, __LINE__) );
        return WHD_UNFINISHED;
    }
    ifp->event_reg_list[WHD_AP_EVENT_ENTRY] = event_entry;

    if (wait_for_interface == WHD_TRUE)
    {
        CHECK_RETURN_WITH_SEMAPHORE(cy_rtos_get_semaphore(&ap->whd_wifi_sleep_flag, (uint32_t)10000,
                                                          WHD_FALSE), &ap->whd_wifi_sleep_flag);
    }
    ap->is_waiting_event = WHD_FALSE;

    if (prim_ifp == ifp)
    {
        /* Set AP mode */
        data = (uint32_t *)whd_proto_get_ioctl_buffer(whd_driver, &buffer, (uint16_t)4);
        CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
        *data = 1; /* Turn on AP */
        CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_ioctl(ifp, WLC_SET_AP, buffer, 0),
                                    &ap->whd_wifi_sleep_flag);
    }

    if (NULL_MAC(ifp->mac_addr.octet) )
    {
        /* Change the AP MAC address to be different from STA MAC */
        if ( (result = whd_wifi_get_mac_address(prim_ifp, &ifp->mac_addr) ) != WHD_SUCCESS )
        {
            WPRINT_WHD_INFO ( (" Get STA MAC address failed result=%" PRIu32 "\n", result) );
            return result;
        }
        else
        {
            WPRINT_WHD_INFO ( (" Get STA MAC address success\n") );
        }
    }

    if ( (result = whd_wifi_set_mac_address(ifp, ifp->mac_addr) ) != WHD_SUCCESS )
    {
        WPRINT_WHD_INFO ( (" Set AP MAC address failed result=%" PRIu32 "\n", result) );
        return result;
    }

    /* Set the SSID */
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)40, "bsscfg:" IOVAR_STR_SSID);
    CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
    if (wlan_chip_id == 4334)
    {
        data[0] = htod32( (uint32_t)CHIP_AP_INTERFACE );  /* Set the bsscfg index */
    }
    else
    {
        data[0] = htod32(bss_index); /* Set the bsscfg index */
    }
    data[1] = htod32(ssid->length); /* Set the ssid length */
    whd_mem_memcpy(&data[2], (uint8_t *)ssid->value, ssid->length);
    if ( (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(ifp, buffer, 0), &ap->whd_wifi_sleep_flag);
    }
    else
    {
        CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(prim_ifp, buffer, 0), &ap->whd_wifi_sleep_flag);
    }

    /* Set the chanspec */
    CHECK_RETURN_WITH_SEMAPHORE(whd_wifi_set_chanspec(ifp, CH20MHZ_CHSPEC(chanspec) ), &ap->whd_wifi_sleep_flag);

    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)8, "bsscfg:" IOVAR_STR_WSEC);
    CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        data[0] = htod32( (uint32_t)CHIP_AP_INTERFACE );
    }
    else
    {
        data[0] = htod32(bss_index);
    }
    if ( (auth_type & WPS_ENABLED) != 0 )
    {
        data[1] = htod32( (uint32_t)( (auth_type & (~WPS_ENABLED) ) | SES_OW_ENABLED ) );
    }
    else
    {
        data[1] = htod32( (uint32_t)auth_type & 0xFF );
    }
    CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(prim_ifp, buffer, 0), &ap->whd_wifi_sleep_flag);
    if (auth_type == WHD_SECURITY_WPA3_SAE)
    {
        auth_mfp = WL_MFP_REQUIRED;

    }
    else if (auth_type == WHD_SECURITY_WPA3_WPA2_PSK)
    {
        auth_mfp = WL_MFP_CAPABLE;
    }
    CHECK_RETURN(whd_wifi_set_iovar_value(ifp, IOVAR_STR_MFP, auth_mfp) );

    if (wlan_chip_id == 4334)
    {
        if (auth_type != WHD_SECURITY_OPEN)
        {
            wsec_pmk_t *psk;

            /* Set the wpa auth */
            data =
                (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)8, "bsscfg:" IOVAR_STR_WPA_AUTH);
            CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
            data[0] = htod32( (uint32_t)CHIP_AP_INTERFACE );
            data[1] = htod32( (uint32_t)(auth_type == WHD_SECURITY_WPA_TKIP_PSK) ?
                              (WPA_AUTH_PSK) : (WPA2_AUTH_PSK | WPA_AUTH_PSK) );
            CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(prim_ifp, buffer, 0), &ap->whd_wifi_sleep_flag);

            /* Set the passphrase */
            psk = (wsec_pmk_t *)whd_proto_get_ioctl_buffer(whd_driver, &buffer, sizeof(wsec_pmk_t) );
            CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(psk, &ap->whd_wifi_sleep_flag);
            whd_mem_memcpy(psk->key, security_key, key_length);
            psk->key_len = htod16(key_length);
            psk->flags = htod16( (uint16_t)WSEC_PASSPHRASE );
            CHECK_RETURN(cy_rtos_delay_milliseconds(1) );
            /* Delay required to allow radio firmware to be ready to receive PMK and avoid intermittent failure */
            CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_ioctl(ifp, WLC_SET_WSEC_PMK, buffer, 0),
                                        &ap->whd_wifi_sleep_flag);
        }
    }
    else
    {
        wsec_pmk_t *psk;

        /* Set the wpa auth */
        data =
            (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)8, "bsscfg:" IOVAR_STR_WPA_AUTH);
        CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
        if ( (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
        {
            data[0] = htod32( (uint32_t)CHIP_AP_INTERFACE );
        }
        else
        {
            data[0] = htod32(bss_index);
        }
	if (auth_type == WHD_SECURITY_OPEN)
        {
            data[1] = WHD_SECURITY_OPEN;
        }
        else if ( (auth_type == WHD_SECURITY_WPA3_SAE) || (auth_type == WHD_SECURITY_WPA3_WPA2_PSK) )
        {
            data[1] =
                htod32( (uint32_t)( (auth_type ==
                                     WHD_SECURITY_WPA3_SAE) ? (WPA3_AUTH_SAE_PSK) : (WPA3_AUTH_SAE_PSK |
                                                                                     WPA2_AUTH_PSK) ) );
        }
        else
        {
            data[1] =
                htod32( (uint32_t)(auth_type ==
                                   WHD_SECURITY_WPA_TKIP_PSK) ? (WPA_AUTH_PSK) : (WPA2_AUTH_PSK | WPA_AUTH_PSK) );
        }
        if ( (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
        {
            CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(ifp, buffer, 0), &ap->whd_wifi_sleep_flag);
        }
        else
        {
            CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(prim_ifp, buffer, 0),
                                        &ap->whd_wifi_sleep_flag);
        }
        if (auth_type != WHD_SECURITY_OPEN)
        {
            if ( (auth_type == WHD_SECURITY_WPA3_SAE) && (whd_driver->chip_info.fwcap_flags & (1 << WHD_FWCAP_SAE) ) )
            {
                whd_wifi_sae_password(ifp, security_key, key_length);
            }
            else
            {
                if ( (auth_type == WHD_SECURITY_WPA3_WPA2_PSK) &&
                     (whd_driver->chip_info.fwcap_flags & (1 << WHD_FWCAP_SAE) ) )
                {
                    whd_wifi_sae_password(ifp, security_key, key_length);
                }
                /* Set the passphrase */
                psk = (wsec_pmk_t *)whd_proto_get_ioctl_buffer(whd_driver, &buffer, sizeof(wsec_pmk_t) );
                CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(psk, &ap->whd_wifi_sleep_flag);
                whd_mem_memcpy(psk->key, security_key, key_length);
                psk->key_len = htod16(key_length);
                psk->flags = htod16( (uint16_t)WSEC_PASSPHRASE );
                CHECK_RETURN(cy_rtos_delay_milliseconds(1) );
                /* Delay required to allow radio firmware to be ready to receive PMK and avoid intermittent failure */
                CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_ioctl(ifp, WLC_SET_WSEC_PMK, buffer,
                                                                0), &ap->whd_wifi_sleep_flag);
            }
        }
    }

    /* Adjust PWE Loop Count(WPA3-R1 Compatibility Issue) */
    if (auth_type & WPA3_SECURITY)
    {
        data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)4, IOVAR_STR_SAE_PWE_LOOP);
        CHECK_IOCTL_BUFFER(data);
        *data = htod32( (uint32_t)PWE_LOOP_COUNT );
        if (whd_proto_set_iovar(ifp, buffer, NULL) != WHD_SUCCESS)
        {
            WPRINT_WHD_DEBUG( ("Some chipsets might not support PWE_LOOP_CNT..Ignore result\n") );
        }
    }

    if (CHSPEC_IS2G(chanspec))
{
    /* Set the multicast transmission rate to 11 Mbps rather than the default 1 Mbps */
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)4, IOVAR_STR_2G_MULTICAST_RATE);
    CHECK_IOCTL_BUFFER(data);
    *data = htod32( (uint32_t)RATE_SETTING_11_MBPS );
    if ( (wlan_chip_id == 4334) || (wlan_chip_id == 43340) || (wlan_chip_id == 43342) )
    {
        result = whd_proto_set_iovar(ifp, buffer, NULL);
        whd_assert("start_ap: Failed to set multicast transmission rate\r\n", result == WHD_SUCCESS);
    }
    else
    {
        CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(ifp, buffer, NULL), &ap->whd_wifi_sleep_flag);
    }
}

    return WHD_SUCCESS;
}

whd_result_t whd_wifi_start_ap(whd_interface_t ifp)
{
    whd_buffer_t buffer;
    uint32_t *data;
    whd_ap_int_info_t *ap;
    whd_interface_t prim_ifp;
    whd_driver_t whd_driver;
    uint32_t arp = 0;
    uint32_t nd = 0;

    CHECK_IFP_NULL(ifp);

    whd_driver = ifp->whd_driver;

    CHECK_DRIVER_NULL(whd_driver);

    prim_ifp = whd_get_primary_interface(whd_driver);

    if (prim_ifp == NULL)
    {
        return WHD_UNKNOWN_INTERFACE;
    }

    /* Turn off arp and nd offloads to ensure AP functionality is not meddled with*/
    whd_wifi_get_iovar_value(ifp, IOVAR_STR_ARPOE, &arp);
    if(arp)
    {
        WPRINT_WHD_INFO ( ("Disabling ARPOE during AP bringup\n") );
        whd_wifi_set_iovar_value(ifp, IOVAR_STR_ARPOE, WHD_FALSE);
    }

    whd_wifi_get_iovar_value(ifp, IOVAR_STR_NDOE, &nd);
    if(nd)
    {
        WPRINT_WHD_INFO ( ("Disabling NDOE during AP bringup\n") );
        whd_wifi_set_iovar_value(ifp, IOVAR_STR_NDOE, WHD_FALSE);
    }

    ap = &whd_driver->ap_info;
    ap->is_waiting_event = WHD_TRUE;
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)8, IOVAR_STR_BSS);
    CHECK_IOCTL_BUFFER_WITH_SEMAPHORE(data, &ap->whd_wifi_sleep_flag);
    data[0] = htod32(ifp->bsscfgidx);
    data[1] = htod32( (uint32_t)BSS_UP );
    CHECK_RETURN_WITH_SEMAPHORE(whd_proto_set_iovar(prim_ifp, buffer, 0), &ap->whd_wifi_sleep_flag);

    /* Wait until AP is brought up */
    CHECK_RETURN_WITH_SEMAPHORE(cy_rtos_get_semaphore(&ap->whd_wifi_sleep_flag, (uint32_t)10000,
                                                      WHD_FALSE), &ap->whd_wifi_sleep_flag);
    ap->is_waiting_event = WHD_FALSE;

    whd_wifi_set_ap_is_up(whd_driver, WHD_TRUE);
#ifdef PROTO_MSGBUF
    whd_wifi_update_addr_mode(ifp->whd_driver, ifp->bsscfgidx);
    whd_wifi_ap_set_max_assoc(ifp, MAX_CLIENT_SUPPORT);
#endif
    return WHD_SUCCESS;
}

whd_result_t whd_wifi_stop_ap(whd_interface_t ifp)
{
    uint32_t *data;
    whd_buffer_t buffer;
    whd_buffer_t response;
    whd_result_t result;
    whd_result_t result2;
    whd_interface_t prim_ifp;
    whd_driver_t whd_driver;
    whd_ap_int_info_t *ap;
#ifdef PROTO_MSGBUF
    whd_maclist_t *client_list;
    uint32_t buf_len = 0;
    whd_mac_t *current_mac;
    struct whd_flowring *flow;
#endif

    CHECK_IFP_NULL(ifp);

    whd_driver = ifp->whd_driver;

    CHECK_DRIVER_NULL(whd_driver);

    ap = &whd_driver->ap_info;

    prim_ifp = whd_get_primary_interface(whd_driver);

    if (prim_ifp == NULL)
    {
        return WHD_UNKNOWN_INTERFACE;
    }

    /* Get Chip Number */
    uint16_t wlan_chip_id = whd_chip_get_chip_id(whd_driver);
    /* Query bss state (does it exist? if so is it UP?) */
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)4, IOVAR_STR_BSS);
    CHECK_IOCTL_BUFFER(data);
    *data = ifp->bsscfgidx;

    result = whd_proto_get_iovar(prim_ifp, buffer, &response);
    if (result == WHD_WLAN_NOTFOUND)
    {
        /* AP interface does not exist - i.e. it is down */
        whd_wifi_set_ap_is_up(whd_driver, WHD_FALSE);
        return WHD_SUCCESS;
    }

    CHECK_RETURN(result);

    *data = dtoh32(*(uint32_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, response) );
    CHECK_PACKET_NULL(data, WHD_NO_REGISTER_FUNCTION_POINTER);
    if (data[0] != (uint32_t)BSS_UP)
    {
        /* AP interface indicates it is not up - i.e. it is down */
        CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );
        whd_wifi_set_ap_is_up(whd_driver, WHD_FALSE);
        return WHD_SUCCESS;
    }

    CHECK_RETURN(whd_buffer_release(whd_driver, response, WHD_NETWORK_RX) );

#ifdef PROTO_MSGBUF
    /* Clear the flowring for STA clients, if created before doing BSS down */
    buf_len = (sizeof(whd_mac_t) * MAX_CLIENT_SUPPORT) + sizeof(client_list->count);
    client_list = (whd_maclist_t*)whd_mem_malloc(buf_len);

    if (!client_list)
    {
        return WHD_MALLOC_FAILURE;
    }

    whd_mem_memset(client_list, 0, buf_len);
    client_list->count = MAX_CLIENT_SUPPORT;

    if((whd_wifi_get_associated_client_list(ifp, client_list, buf_len)) != CY_RSLT_SUCCESS)
    {
        WPRINT_WHD_ERROR(("Error while getting the associated STA list \n"));
        whd_mem_free(client_list);
    }

    flow = whd_driver->msgbuf->flow;
    current_mac = &client_list->mac_list[0];

    while ( (client_list->count > 0) && (!(NULL_MAC(current_mac->octet))) )
    {
        whd_flowring_delete_peers(flow, current_mac->octet, ifp->ifidx);
        --client_list->count;
        ++current_mac;
    }

    whd_mem_free(client_list);
#endif    /* PROTO_MSGBUF */

    ap->is_waiting_event = WHD_TRUE;
    /* set BSS down */
    data = (uint32_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, (uint16_t)8, IOVAR_STR_BSS);
    CHECK_IOCTL_BUFFER(data);
    data[0] = htod32(ifp->bsscfgidx);
    data[1] = htod32( (uint32_t)BSS_DOWN );
    CHECK_RETURN(whd_proto_set_iovar(prim_ifp, buffer, 0) );

    /* Wait until AP is brought down */
    result = cy_rtos_get_semaphore(&ap->whd_wifi_sleep_flag, (uint32_t)10000, WHD_FALSE);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error getting a semaphore, %s failed at %d \n", __func__, __LINE__) );
        goto sema_fail;
    }

    /* Disable AP mode only if AP is on primary interface */
    if (prim_ifp == ifp)
    {
        data = (uint32_t *)whd_proto_get_ioctl_buffer(whd_driver, &buffer, (uint16_t)4);
        CHECK_IOCTL_BUFFER(data);
        *data = 0;
        CHECK_RETURN(whd_proto_set_ioctl(ifp, WLC_SET_AP, buffer, 0) );
        if ( (wlan_chip_id != 43430) && (wlan_chip_id != 43439) &&
             (wlan_chip_id != 43909) && (wlan_chip_id != 43907) &&
             (wlan_chip_id != 54907) )
        {
            result = cy_rtos_get_semaphore(&ap->whd_wifi_sleep_flag, (uint32_t)10000, WHD_FALSE);
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("Error getting a semaphore, %s failed at %d \n", __func__, __LINE__) );
                goto sema_fail;
            }
        }
    }

sema_fail:
    ap->is_waiting_event = WHD_FALSE;
    result2 = cy_rtos_deinit_semaphore(&ap->whd_wifi_sleep_flag);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error getting a semaphore, %s failed at %d \n", __func__, __LINE__) );
        return result;
    }
    if (result2 != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error deleting semaphore, %s failed at %d \n", __func__, __LINE__) );
        return result2;
    }

    CHECK_RETURN(whd_wifi_deregister_event_handler(ifp, ifp->event_reg_list[WHD_AP_EVENT_ENTRY]) );
    ifp->event_reg_list[WHD_AP_EVENT_ENTRY] = WHD_EVENT_NOT_REGISTERED;
    whd_wifi_set_ap_is_up(whd_driver, WHD_FALSE);

    ifp->role = WHD_INVALID_ROLE;

    /* Turning on arp and nd offloads again as these were disabled during start_ap*/
    if ( whd_driver->chip_info.fwcap_flags & (1 << WHD_FWCAP_OFFLOADS) )
    {
        whd_wifi_set_iovar_value(ifp, IOVAR_STR_ARPOE, WHD_TRUE);
        whd_wifi_set_iovar_value(ifp, IOVAR_STR_NDOE, WHD_TRUE);
    }

    return WHD_SUCCESS;
}
