/*
 * Copyright 2025, Cypress Semiconductor Corporation (an Infineon company)
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

/** @file whd_wlansense_core.c
 *  Provides event handler functions for CSI functionality.
 */

#if defined(COMPONENT_WLANSENSE)
#include "whd_wlansense_core.h"
#include "whd_wlansense_api.h"
#include "whd_utils.h"
#include "whd_proto.h"
#include "whd_buffer_api.h"


/******************************************************
*                   Constants
******************************************************/
#define NETLINK_USER 31 // not yet used by now
#define CSI_GRP  22 // not yet used by now
#define CSI_DATA_BUFFER_SIZE 2048


/******************************************************
*             Static Variables
******************************************************/
static whd_interface_t s_prim_ifp;
static whd_interface_t s_csi_ifp;
static whd_mac_t wlansense_mac_addr;
static const whd_event_num_t vif_event[] = { WLC_E_IF, WLC_E_NONE };
static const whd_event_num_t csi_events[] =
{ WLC_E_CSI_ENABLE, WLC_E_CSI_DATA, WLC_E_CSI_DISABLE, WLC_E_NONE };


/******************************************************
*             Function declaration
******************************************************/

/** Handle the CSI Event notifications from Firmware.
 *
 * @param ifp:                  Pointer to handle instance of whd interface.
 * @param event_header:         Pointer to event message header.
 * @param event_data:           Pointer to event data.
 * @param handler_user_data:    A pointer value which will be passed to the event handler function (NULL is allowed).
 *
 * @return void:                No error code returns.
 */
static void *whd_wlansense_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header,const uint8_t *event_data, void *handler_user_data);

static whd_result_t whd_wlansense_attach_and_handshake(whd_csi_info_t csi_info);
static whd_result_t whd_wlansense_process_csi_data(whd_csi_info_t csi_info, const uint8_t *event_data, uint32_t datalen);
static whd_result_t whd_wlansense_detach_and_release_uart(whd_csi_info_t csi_info);


/******************************************************
*             Function definitions
******************************************************/
whd_interface_t
whd_wlansense_get_interface(void)
{
    return s_csi_ifp;
}

whd_result_t
whd_wlansense_register_handler(void* user_data)
{
    uint16_t event_entry = 0xFF;

    CHECK_IFP_NULL(s_csi_ifp);
    if (s_csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY] != WHD_EVENT_NOT_REGISTERED)
    {
        whd_wifi_deregister_event_handler(s_csi_ifp, s_csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY]);
        s_csi_ifp->event_reg_list[WHD_CSI_EVENT_ENTRY] = WHD_EVENT_NOT_REGISTERED;
    }

    CHECK_RETURN (whd_management_set_event_handler(s_csi_ifp, csi_events,
                whd_wlansense_events_handler, user_data, &event_entry));
    return WHD_SUCCESS;
}

static void
*whd_wlansense_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header,
                const uint8_t *event_data, void *handler_user_data)
{
    whd_csi_info_t csi_info;

    /* Note: The interface returned by registered event is always primary */
    /* But Wlansense event need to work on the Wlansense interface */
    /* So unsed the ifp parameter and use the pre-stored csi_ifp */
    UNUSED_PARAMETER(ifp);
    UNUSED_PARAMETER(handler_user_data);

    CHECK_IFP_NULL(s_csi_ifp);
    csi_info = s_csi_ifp->csi_info;
    if (csi_info == NULL)
    {
        WPRINT_WHD_ERROR(("%s: The wlansense info struct is not provided!\n", __func__));
        return WHD_BADARG;
    }

    switch (event_header->event_type)
    {
        case WLC_E_CSI_ENABLE:
            whd_wlansense_attach_and_handshake(csi_info);
            break;
        case WLC_E_CSI_DATA:
            whd_wlansense_process_csi_data(csi_info, event_data, event_header->datalen);
            break;
        case WLC_E_CSI_DISABLE:
            whd_wlansense_detach_and_release_uart(csi_info);
            break;
        default:
            WPRINT_WHD_ERROR(("Event type is not registered !"));
    }
    return 0;
}

whd_result_t
whd_wlansense_attach_and_handshake(whd_csi_info_t csi_info)
{
    /* Allocating memory for csi_info data buffer for CSI message */
    csi_info->data = whd_mem_malloc(CSI_DATA_BUFFER_SIZE * sizeof(char));
    if (csi_info->data == NULL) {
        WPRINT_WHD_ERROR(("%s: Failed to allocate buffer for CSI message \n", __func__));
        return WHD_BUFFER_ALLOC_FAIL;
    }

    /* HandShake byte */
    char *data = "b";
    csi_info->csi_data_cb_func(data, 1);
    return WHD_SUCCESS;
}

whd_result_t
whd_wlansense_process_csi_data(whd_csi_info_t csi_info, const uint8_t *event_data, uint32_t datalen)
{
    struct wlc_csi_fragment_hdr *frag_hdr = (struct wlc_csi_fragment_hdr *)event_data;
    char *data = csi_info->data;
    static char *last;
    int hdrlen = sizeof(struct wlc_csi_fragment_hdr);
    datalen = datalen -hdrlen;
    event_data = event_data + hdrlen;
    /* TODO: Handle sequence number, fragment number mismatches. Check header version. */
    if (frag_hdr->fragment_num == 0) {
        last = data;
    }
    whd_mem_memcpy(last, event_data, datalen);
    last = last + datalen;
    /* Handling the last frame */
    if (frag_hdr->fragment_num == (frag_hdr->total_fragments - 1)) {
        csi_info->csi_data_cb_func(data,(last - data));
        last = data;
    }
    return WHD_SUCCESS;
}

whd_result_t
whd_wlansense_detach_and_release_uart(whd_csi_info_t csi_info)
{
    /* TODO: Send some signal that would indicate the script that disable event is being received */
    char *data = "BYE";
    csi_info->csi_data_cb_func(data, 3);
    whd_mem_free(csi_info->data);
    whd_mem_free(csi_info);
    return WHD_SUCCESS;
}

void
whd_wlansense_init_cfg_params(wlc_csi_cfg_t *csi_cfg)
{
    uint32_t i;
    const whd_mac_t ether_null = {{0,0,0,0,0,0}};

    /* Initializes the params which will be sent to FW */
    csi_cfg->csi_enable = WHD_TRUE;
    csi_cfg->capture_period_ms = CSI_CAPTURE_PERIOD_NON_PERIODIC;
    csi_cfg->capture_window_dur_ms = CSI_DEFAULT_CAPTURE_WINDOW_DUR;
    csi_cfg->solicit_mode = CSI_SOLICIT_MODE_UNSOL;
    csi_cfg->assoc_mode = CSI_AS_MODE_ASSOC;
    csi_cfg->bss_mode = CSI_BSS_MODE_MY_DEVICE;
    csi_cfg->ignore_fcs = WHD_FALSE;
    csi_cfg->frmtyp_subtyp[0] = INVALID_FT;
    csi_cfg->frmtyp_subtyp[1] = INVALID_FT;
    csi_cfg->multi_csi_per_mac = WHD_TRUE;
    csi_cfg->link_protection = WHD_FALSE;
    csi_cfg->subcarriers = 0;
    csi_cfg->chanspec = INV_CHANSPEC;
    for(i=0; i < CSI_MAX_RX_MACADDR; i++)
    {
        whd_mem_memcpy(&csi_cfg->rx_macaddr[i], &ether_null, ETHER_ADDR_LEN);
    }
    return;
}

void
whd_wlansense_validate_cfg_params(wlc_csi_cfg_t *csi_cfg)
{
    /* If no frame type is configured, use beacon as default frame filter */
    if ((csi_cfg->frmtyp_subtyp[0] == INVALID_FT) && (csi_cfg->frmtyp_subtyp[1] == INVALID_FT))
    {
        csi_cfg->frmtyp_subtyp[0] = CSI_FRAMETYPE_SUBTYPE_BEACON;
    }

    return;
}

static void
*whd_wlansense_handle_vif_event(whd_interface_t ifp, const whd_event_header_t *event_header,
                                    const uint8_t *event_data, void *handler_user_data)
{
    whd_interface_t prim_ifp = ifp;
    wl_vif_event_t *vif_evt = (wl_vif_event_t *)event_data;
    whd_result_t result;

    if (prim_ifp == NULL)
    {
        if (s_prim_ifp != NULL)
        {
            prim_ifp = s_prim_ifp;
        }
        else
        {
            WPRINT_WHD_ERROR(("Primary Interface is not up/NULL and failed in function %s at line %d \n", __func__, __LINE__));
            return handler_user_data;
        }
    }

    if ((event_header->event_type == WLC_E_IF) && (vif_evt->action == WHD_IF_E_ADD))
    {
        WPRINT_WHD_INFO(("VIF ADD - idx[%d], bsscfgidx[%d], role[%d]\n", vif_evt->ifidx, vif_evt->bsscfgidx, vif_evt->role));
        s_csi_ifp = whd_mem_malloc(sizeof(whd_interface_t));
        result = whd_add_interface(prim_ifp->whd_driver, vif_evt->bsscfgidx, vif_evt->ifidx, "wlansense0",
                                  (whd_mac_t *)handler_user_data, &s_csi_ifp);
        if (result == WHD_SUCCESS)
        {
            WPRINT_WHD_INFO(("WLANSENSE0 interface created!!! \n"));
            s_csi_ifp->role = vif_evt->role;
        }
        else
        {
            whd_mem_free (s_csi_ifp);
        }
    }
    else
    {
        WPRINT_WHD_ERROR(("Unhandled VIF event \n"));
        handler_user_data = NULL;
    }
    return handler_user_data;
}

whd_result_t whd_wlansense_create_interface (whd_driver_t whd_driver)
{
    whd_interface_t prim_ifp;
    whd_result_t result;
    whd_buffer_t buffer;
    wl_wlan_sense_if_t *csi_intf_req;
    uint16_t event_entry = (uint16_t)0xFF;

    CHECK_DRIVER_NULL(whd_driver);
    prim_ifp = whd_get_primary_interface(whd_driver);
    CHECK_IFP_NULL(prim_ifp);
    s_prim_ifp = prim_ifp;

    /* Register for virtual interface events */
    CHECK_RETURN(whd_management_set_event_handler(prim_ifp, vif_event, whd_wlansense_handle_vif_event,
                                                                 &wlansense_mac_addr, &event_entry));

    if ( (result = whd_wifi_get_mac_address(prim_ifp, &wlansense_mac_addr) ) != WHD_SUCCESS )
    {
        WPRINT_WHD_INFO ((" Get STA MAC address failed result=%" PRIu32 "\n", result));
        return result;
    }
    else
    {
        WPRINT_WHD_INFO ((" Get STA MAC address success\n"));
    }

    if (wlansense_mac_addr.octet[0] & (0x02))
    {
        wlansense_mac_addr.octet[0] &= (uint8_t) ~(0x02);
    }
    else
    {
        wlansense_mac_addr.octet[0] |= (0x02);
    }

    csi_intf_req = (wl_wlan_sense_if_t *)whd_proto_get_iovar_buffer(whd_driver, &buffer, sizeof(wl_wlan_sense_if_t), IOVAR_STR_CSI_IFADD);
    CHECK_IOCTL_BUFFER(csi_intf_req);
    whd_mem_memcpy(&csi_intf_req->csi_macaddr, &wlansense_mac_addr, sizeof(whd_mac_t) );
    result = whd_proto_set_iovar(prim_ifp, buffer, NULL);

    return result;
}

whd_result_t
whd_wlansense_get_config(wlc_csi_cfg_t *csi_cfg)
{
    uint8_t buffer[WLC_IOCTL_MEDLEN];
    uint32_t i;
    char ea_buf[WHD_ETHER_ADDR_STR_LEN];
    wlc_csi_cfg_t *csi_cfg_buf;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    CHECK_IFP_NULL(s_prim_ifp);
    CHECK_RETURN(whd_wifi_get_iovar_buffer(s_prim_ifp, IOVAR_STR_CSI, buffer, WLC_IOCTL_MEDLEN));
    csi_cfg_buf = (wlc_csi_cfg_t *)buffer;
    whd_mem_memcpy(csi_cfg, csi_cfg_buf, sizeof(wlc_csi_cfg_t));

    /* Printing the contents to console */
    WPRINT_WHD_INFO(("CSI Status: %s\n", csi_cfg->csi_enable == 1 ? "Enabled":"Disabled"));
    if (csi_cfg->capture_period_ms < 0)
    {
        WPRINT_WHD_INFO(("\tCapture period: Non-periodic\n"));
    } else
    {
        WPRINT_WHD_INFO(("\tcapture_period_ms: %ld\n", csi_cfg->capture_period_ms));
        WPRINT_WHD_INFO(("\tcapture_window_dur_ms: %d\n", csi_cfg->capture_window_dur_ms));
    }
    WPRINT_WHD_INFO(("\tsolicit_mode: %d\n", csi_cfg->solicit_mode));
    WPRINT_WHD_INFO(("\tassoc_mode: %d\n", csi_cfg->assoc_mode));
    WPRINT_WHD_INFO(("\tbss_mode: %d\n", csi_cfg->bss_mode));
    WPRINT_WHD_INFO(("\tignore_fcs: %d\n", csi_cfg->ignore_fcs));
    if (csi_cfg->frmtyp_subtyp[0] == (uint8)INVALID_FT)
    {
        WPRINT_WHD_INFO(("\tfrmtyp_subtyp[0]: Not configured\n"));
    } else
    {
        WPRINT_WHD_INFO(("\tfrmtyp_subtyp[0]: %d\n", csi_cfg->frmtyp_subtyp[0]));
    }
    if (csi_cfg->frmtyp_subtyp[1] == (uint8)INVALID_FT)
    {
        WPRINT_WHD_INFO(("\tfrmtyp_subtyp[1]: Not configured\n"));
    } else
    {
        WPRINT_WHD_INFO(("\tfrmtyp_subtyp[1]: %d\n", csi_cfg->frmtyp_subtyp[1]));
    }
    WPRINT_WHD_INFO(("\tmulti_csi_per_mac: %d\n", csi_cfg->multi_csi_per_mac));
    WPRINT_WHD_INFO(("\tlink_protection: %d\n", csi_cfg->link_protection));
    WPRINT_WHD_INFO(("\tsubcarriers: %d\n", csi_cfg->subcarriers));

    for(i = 0; i < CSI_MAX_RX_MACADDR; ++i) {
        whd_ether_ntoa((const uint8_t *)csi_cfg->rx_macaddr[i].octet, ea_buf, sizeof(ea_buf));
        WPRINT_WHD_INFO(("\trx_macaddr[%ld]: %s\n", i, ea_buf));
    }
    return result;
}

#endif /* defined(COMPONENT_WLANSENSE) */
