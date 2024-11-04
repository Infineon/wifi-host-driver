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

#if defined(COMPONENT_SDIO_HM)

#include "sdio_hosted_support.h"
#include "sdio_command.h"

#define SDIO_CMD_STACK_SIZE     (1024 * 4)
#define SDIO_CMD_TASK_PRIORITY  (CY_RTOS_PRIORITY_NORMAL)
__attribute__((aligned(8))) uint8_t sdio_cmd_thread_stack[SDIO_CMD_STACK_SIZE] = {0};

#define PRINT_IP(addr)    (int)((addr >> 0) & 0xFF), (int)((addr >> 8) & 0xFF), (int)((addr >> 16) & 0xFF), (int)((addr >> 24) & 0xFF)
#define IP_ADDR_STR_LEN   (20)

/*
 * Callback function which receives the scan results.
 */
static void
sdio_cmd_scan_handler(cy_wcm_scan_result_t *result_ptr, void *user_data, cy_wcm_scan_status_t status)
{
    sdio_command_t sdio_cmd = (sdio_command_t)user_data;
    sdio_handler_t sdio_hm = (sdio_handler_t)sdio_cmd->sdio_hm;
    inf_scan_event_t *inf_event = NULL;
    inf_event_base_t *base = NULL;
    scan_event_t *scan = NULL;
    cy_rslt_t result;

    inf_event = (inf_scan_event_t *)whd_mem_malloc(sizeof(*inf_event));
    if (!inf_event) {
        PRINT_HM_ERROR(("inf_scan_event_t malloc failed\n"));
        return;
    }

    whd_mem_memset(inf_event, 0, sizeof(*inf_event));

    base = &inf_event->base;
    base ->ver = SCAN_EVENT_VER;
    base ->type = INF_SCAN_EVENT;
    base ->len = sizeof(*scan);
    scan = &inf_event->scan;

    if (status == CY_WCM_SCAN_INCOMPLETE) {
        /*
         * Copy the scan result into the message structure.
         */
        scan->ssid_length = strlen((char *)result_ptr->SSID);
        whd_mem_memcpy(scan->ssid, result_ptr->SSID, scan->ssid_length);
        whd_mem_memcpy(scan->macaddr, result_ptr->BSSID, CY_WCM_MAC_ADDR_LEN);
        scan->channel = result_ptr->channel;
        scan->band = result_ptr->band;
        scan->signal_strength = result_ptr->signal_strength;
        scan->security_type = result_ptr->security;
    } else {
        /*
         * Just mark that the scan is complete.
         */
        scan->scan_complete = true;
    }

    result = SDIO_HM_TX_EVENT(sdio_hm, inf_event, sizeof(*inf_event));
    if (result != SDIOD_STATUS_SUCCESS) {
        sdio_hm->tx_info->err_event++;
        PRINT_HM_ERROR(("tx event failed: 0x%lx\n", result));
        free(inf_event);
    }
}

static void *
sdio_cmd_get_rsp(uint8_t rsp_len, sdio_bcdc_header_t *cmd_bcdc)
{
    void *rsp = NULL;
    sdio_bcdc_header_t *bcdc = NULL;

    rsp = whd_mem_malloc(rsp_len);
    if (!rsp) {
        PRINT_HM_ERROR(("rsp malloc failed\n"));
        return NULL;
    }

    whd_mem_memset(rsp, 0, rsp_len);
    bcdc = (sdio_bcdc_header_t *)rsp;
    bcdc->cmd = cmd_bcdc->cmd;
    bcdc->len = rsp_len - sizeof(*bcdc);
    bcdc->flags = (BCDC_PROTO_VER << BCDC_FLAG_VER_SHIFT);
    bcdc->flags |= (BCDC_FLAG_ID(cmd_bcdc->flags) << BCDC_FLAG_ID_SHIFT);

    return rsp;
}

static void
sdio_cmd_send_rsp(sdio_command_t sdio_cmd, void *rsp, uint8_t rsp_len)
{
    cy_rslt_t result;

    result = SDIO_HM_TX_CMD(sdio_cmd->sdio_hm, rsp, rsp_len);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("Send event failed 0x%lx\n", result));
        free(rsp);
    }
}

static void
sdio_cmd_get_mac(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_handler_t sdio_hm = (sdio_handler_t)sdio_cmd->sdio_hm;
    sdio_cmd_mac_rsp_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    whd_result_t result;
    whd_mac_t mac;

    rsp = (sdio_cmd_mac_rsp_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    result = whd_wifi_get_mac_address(sdio_hm->ifp, &mac);
    if (result != WHD_SUCCESS) {
        PRINT_HM_ERROR(("Get mac address 0x%lx\n", result));
        return;
    }

    whd_mem_memcpy(rsp->mac_addr, mac.octet, sizeof(rsp->mac_addr));

    rsp->bcdc.status = CY_RSLT_SUCCESS;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_get_ip(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_cmd_ip_rsp_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_wcm_ip_address_t ip_addr;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_cmd_ip_rsp_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    do {
        if (!cy_wcm_is_connected_to_ap()) {
            PRINT_HM_ERROR(("Not connected to AP\n"));
            result = CYHAL_SDIO_RSLT_ERR_CONFIG;
            break;
        }

        if (cmd_bcdc->cmd == INF_C_IPv4) {
            result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        } else if (cmd_bcdc->cmd == INF_C_GATEWAY_IP) {
            result = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        } else if (cmd_bcdc->cmd == INF_C_NETMASK) {
            result = cy_wcm_get_ip_netmask(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        }
    } while (false);

    if (result == CY_RSLT_SUCCESS) {
        rsp->ip = ip_addr.ip.v4;
    } else {
        rsp->bcdc.status = result;
    }

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_get_ap_info(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_cmd_ap_info_rsp_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_wcm_associated_ap_info_t ap_info;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_cmd_ap_info_rsp_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    do {
        if (!cy_wcm_is_connected_to_ap()) {
            PRINT_HM_ERROR(("Not connected to AP\n"));
            result = CYHAL_SDIO_RSLT_ERR_CONFIG;
            break;
        }

        whd_mem_memset(&ap_info, 0, sizeof(ap_info));
        result = cy_wcm_get_associated_ap_info(&ap_info);
        if (result != CY_RSLT_SUCCESS) {
            PRINT_HM_ERROR(("Get AP info failed %ld\n", result));
            break;
        }

        rsp->ssid_length = strlen((char *)ap_info.SSID);
        whd_mem_memcpy(rsp->ssid, ap_info.SSID, rsp->ssid_length);
        whd_mem_memcpy(rsp->bssid, ap_info.BSSID, CY_WCM_MAC_ADDR_LEN);

        rsp->channel = ap_info.channel;
        rsp->channel_width = ap_info.channel_width;
        rsp->signal_strength = ap_info.signal_strength;
        rsp->security_type = ap_info.security;
    } while (false);

    rsp->bcdc.status = result;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_get_ip_info(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_cmd_ip_info_rsp_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_wcm_ip_address_t ip_addr;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_cmd_ip_info_rsp_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    do {
        if (!cy_wcm_is_connected_to_ap()) {
            PRINT_HM_ERROR(("Not connected to AP\n"));
            result = CYHAL_SDIO_RSLT_ERR_CONFIG;
            break;
        }

        /*
         * For now setting method dhcp always. Need to fix it once the static method is supported.
         */
        rsp->dhcp = true;

        /*
         * Retrieve IPv4 address from WCM library.
         */
        result = cy_wcm_get_ip_addr(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        if (result == CY_RSLT_SUCCESS) {
            PRINT_HM_INFO(("WCM GetIP address successful\n"));
            rsp->address = ip_addr.ip.v4;
        }

        /*
         * Retrieve netmask address from WCM library.
         */
        result = cy_wcm_get_ip_netmask(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        if (result == CY_RSLT_SUCCESS) {
            PRINT_HM_INFO(("WCM Netmask address successful\n"));
            rsp->netmask = ip_addr.ip.v4;
        }

        /*
         * Retrieve gateway address from WCM library.
         */
        result = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        if (result == CY_RSLT_SUCCESS) {
            PRINT_HM_INFO(("WCM Get gateway address successful\n"));
            rsp->gateway = ip_addr.ip.v4;
        }
    } while (false);

    rsp->bcdc.status = result;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_scan(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_bcdc_header_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_bcdc_header_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    result = cy_wcm_start_scan(sdio_cmd_scan_handler, (void *)sdio_cmd, NULL);
    if (result == CY_RSLT_SUCCESS) {
        PRINT_HM_INFO(("Start Scan successful\n"));
    } else {
        PRINT_HM_INFO(("Start Scan failed %lx\n", result));
    }

    rsp->status = result;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_connect(sdio_command_t sdio_cmd, uint8_t *cmd_msg)
{
    sdio_bcdc_header_t *cmd_bcdc = (sdio_bcdc_header_t *)cmd_msg;
    sdio_cmd_connect_t *connect = (sdio_cmd_connect_t *)(cmd_msg + sizeof(*cmd_bcdc));
    sdio_bcdc_header_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_setting_t static_ip_settings;
    cy_wcm_ip_address_t ip_addr;
    char ip_addr_str[IP_ADDR_STR_LEN];
    cy_rslt_t result = CY_RSLT_SUCCESS;

    PRINT_HM_INFO(("Connect SSID: %s (len %d), security: 0x%lx, band: %d\n",
        connect->ssid, connect->ssid_length, connect->security_type, connect->band));

    rsp = (sdio_bcdc_header_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    /*
     * Setup WCM connect parameters.
    */
    whd_mem_memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
    whd_mem_memcpy(connect_param.ap_credentials.SSID, connect->ssid, connect->ssid_length);
    connect_param.ap_credentials.security = connect->security_type;

    if (connect_param.ap_credentials.security != CY_WCM_SECURITY_OPEN) {
        whd_mem_memcpy(connect_param.ap_credentials.password, connect->password, strlen(connect->password) + 1);
    } else if (connect_param.ap_credentials.security == CY_WCM_SECURITY_WPA2_AES_PSK) {
        PRINT_HM_INFO(("auth type is set to CY_WCM_SECURITY_WPA2_AES_PSK\n"));
    }

    if (!NULL_MAC(connect->macaddr)) {
        whd_mem_memcpy(connect_param.BSSID, connect->macaddr, CY_WCM_MAC_ADDR_LEN);
    }

    connect_param.band = connect->band;

    if (connect->ip_address && connect->gateway && connect->netmask)
    {
        connect_param.static_ip_settings = &static_ip_settings;

        sprintf(ip_addr_str, "%d.%d.%d.%d", PRINT_IP(connect->ip_address));
        PRINT_HM_INFO(("%10s: %s\n", "IP address", ip_addr_str));
        static_ip_settings.ip_address.version = CY_WCM_IP_VER_V4;
        static_ip_settings.ip_address.ip.v4 = connect->ip_address;

        sprintf(ip_addr_str, "%d.%d.%d.%d", PRINT_IP(connect->gateway));
        PRINT_HM_INFO(("%10s: %s\n", "Gateway", ip_addr_str));
        static_ip_settings.gateway.version = CY_WCM_IP_VER_V4;
        static_ip_settings.gateway.ip.v4 = connect->gateway;

        sprintf(ip_addr_str, "%d.%d.%d.%d", PRINT_IP(connect->netmask));
        PRINT_HM_INFO(("%10s: %s\n", "Netmask", ip_addr_str));
        static_ip_settings.netmask.version = CY_WCM_IP_VER_V4;
        static_ip_settings.netmask.ip.v4 = connect->netmask;
    }

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);

    /*
     * Connect to AP.
     */
    result = cy_wcm_connect_ap(&connect_param, &ip_addr);
    if (result == CY_RSLT_SUCCESS) {
        sprintf(ip_addr_str, "%d.%d.%d.%d", PRINT_IP(ip_addr.ip.v4));
        PRINT_HM_INFO(("network Connection successful IP:%s \n", ip_addr_str));
    } else {
        PRINT_HM_INFO(("network Connection failed %lx\n", result));
    }
}

static void
sdio_cmd_disconnect(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_bcdc_header_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_bcdc_header_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    result = cy_wcm_disconnect_ap();
    if (result == CY_RSLT_SUCCESS) {
        PRINT_HM_INFO(("network Disconnection successful\n"));
    } else {
        PRINT_HM_INFO(("network Disconnection failed %lx\n", result));
    }

    rsp->status = result;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

static void
sdio_cmd_ping(sdio_command_t sdio_cmd, sdio_bcdc_header_t *cmd_bcdc)
{
    sdio_cmd_ping_rsp_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);
    cy_wcm_ip_address_t ip_addr;
    uint32_t timeout_ms = 1000;
    uint32_t elapsed_time = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    rsp = (sdio_cmd_ping_rsp_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    do {
        if (!cy_wcm_is_connected_to_ap()) {
            PRINT_HM_ERROR(("Not connected to AP\n"));
            result = CYHAL_SDIO_RSLT_ERR_CONFIG;
            break;
        }

        /* Retrieve gateway address from WCM library */
        result = cy_wcm_get_gateway_ip_address(CY_WCM_INTERFACE_TYPE_STA, &ip_addr);
        if (result != CY_RSLT_SUCCESS) {
            PRINT_HM_ERROR(("Cannot get IP address %ld\n", result));
            result = result;
            break;
        }

        PRINT_HM_INFO(("WCM Get gateway address successful\n"));
        result = cy_wcm_ping(CY_WCM_INTERFACE_TYPE_STA, &ip_addr, timeout_ms, &elapsed_time);
        if (result != CY_RSLT_SUCCESS) {
            PRINT_HM_ERROR(("Ping failed %ld\n", result));
            break;
        }

        PRINT_HM_INFO(("Ping successful\n"));
        rsp->elapsed_time = elapsed_time;
    } while (false);

    rsp->bcdc.status = result;

    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

#if defined(SDIO_HM_AT_CMD)

/**
 * AT CMD transport is data ready callback
 */
bool sdio_cmd_at_is_data_ready()
{
    sdio_handler_t sdio_hm = sdio_hm_get_sdio_handler();
    sdio_command_t sdio_cmd = NULL;

    if (!sdio_hm)
        return false;

    sdio_cmd = sdio_hm->sdio_cmd;
    if (!sdio_cmd)
        return false;

    if (strlen(sdio_cmd->at_cmd_buf))
        return true;
    else
        return false;
}

/**
 * AT CMD transport read callback
 */
uint32_t sdio_cmd_at_read_data(uint8_t *buffer, uint32_t size)
{
    sdio_handler_t sdio_hm = sdio_hm_get_sdio_handler();
    sdio_command_t sdio_cmd = NULL;
    uint32_t len = 0;

    if (!sdio_hm)
        return CYHAL_SDIO_RSLT_ERR_CONFIG;

    sdio_cmd = sdio_hm->sdio_cmd;
    if (!sdio_cmd)
        return CYHAL_SDIO_RSLT_ERR_CONFIG;

    if (size < strlen(sdio_cmd->at_cmd_buf)) {
        PRINT_HM_INFO(("buffer size %ld isn't enough to get AT command %d\n", size, strlen(sdio_cmd->at_cmd_buf)));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    PRINT_HM_INFO(("AT CMD read data: %s\n", sdio_cmd->at_cmd_buf));

    len = strlen(sdio_cmd->at_cmd_buf);

    whd_mem_memcpy(buffer, sdio_cmd->at_cmd_buf, len);
    whd_mem_memset(sdio_cmd->at_cmd_buf, 0, AT_CMD_BUF_SIZE);

    return len;
}

/**
 * AT CMD transport write callback
 */
cy_rslt_t sdio_cmd_at_write_data(uint8_t *buffer, uint32_t length)
{
    sdio_handler_t sdio_hm = sdio_hm_get_sdio_handler();
    inf_at_event_t *inf_event = NULL;
    inf_event_base_t *base = NULL;
    cy_rslt_t result;

    if (!sdio_hm)
        return CYHAL_SDIO_RSLT_ERR_CONFIG;

    PRINT_HM_INFO(("AT CMD write data: %s\n", buffer));

    if (length > AT_CMD_BUF_SIZE) {
        PRINT_HM_ERROR(("write length %ld over max size %d\n", length, AT_CMD_BUF_SIZE));
        length = AT_CMD_BUF_SIZE;
    }

    inf_event = (inf_at_event_t *)whd_mem_malloc(sizeof(*inf_event));
    if (!inf_event) {
        PRINT_HM_ERROR(("inf_at_event_t malloc failed\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    whd_mem_memset(inf_event, 0, sizeof(*inf_event));

    base = &inf_event->base;
    base ->ver = AT_EVENT_VER;
    base ->type = INF_AT_EVENT;
    base ->len = AT_CMD_BUF_SIZE;
    whd_mem_memcpy(inf_event->string, buffer, length);

    result = SDIO_HM_TX_EVENT(sdio_hm, inf_event, sizeof(*inf_event));
    if (result != SDIOD_STATUS_SUCCESS) {
        sdio_hm->tx_info->err_event++;
        PRINT_HM_ERROR(("tx event failed: 0x%lx\n", result));
        free(inf_event);
    }

    return result;
}

static void sdio_cmd_at_cmd(sdio_command_t sdio_cmd, uint8_t *cmd_msg)
{
    sdio_bcdc_header_t *cmd_bcdc = (sdio_bcdc_header_t *)cmd_msg;
    sdio_cmd_at_t *at = (sdio_cmd_at_t *)(cmd_msg + sizeof(*cmd_bcdc));
    sdio_bcdc_header_t *rsp = NULL;
    uint8_t rsp_len = sizeof(*rsp);

    if (sdio_cmd_at_is_data_ready()) {
        PRINT_HM_ERROR(("Give up command %s\n", at->string));
        return;
    }

    rsp = (sdio_bcdc_header_t *)sdio_cmd_get_rsp(rsp_len, cmd_bcdc);
    if (!rsp) {
        return;
    }

    whd_mem_memcpy(sdio_cmd->at_cmd_buf, at->string, strlen(at->string));

    rsp->status = CY_RSLT_SUCCESS;
    sdio_cmd_send_rsp(sdio_cmd, rsp, rsp_len);
}

#endif /* defined(SDIO_HM_AT_CMD) */

static void sdio_cmd_get(sdio_command_t sdio_cmd, uint8_t *cmd_msg)
{
    sdio_bcdc_header_t *bcdc = (sdio_bcdc_header_t *)cmd_msg;

    switch (bcdc->cmd)
    {
        case INF_C_MAC:
            sdio_cmd_get_mac(sdio_cmd, bcdc);
            break;
        case INF_C_IPv4:
        case INF_C_GATEWAY_IP:
        case INF_C_NETMASK:
            sdio_cmd_get_ip(sdio_cmd, bcdc);
            break;
        case INF_C_AP_INFO:
            sdio_cmd_get_ap_info(sdio_cmd, bcdc);
            break;
        case INF_C_IP_INFO:
            sdio_cmd_get_ip_info(sdio_cmd, bcdc);
            break;
        default:
            PRINT_HM_ERROR(("unsupported get command %ld \n", bcdc->cmd));
            break;
    }

}

static void sdio_cmd_set(sdio_command_t sdio_cmd, uint8_t *cmd_msg)
{
    sdio_bcdc_header_t *bcdc = (sdio_bcdc_header_t *)cmd_msg;

    switch (bcdc->cmd)
    {
        case INF_C_SCAN:
            sdio_cmd_scan(sdio_cmd, bcdc);
            break;
        case INF_C_CONNECT:
            sdio_cmd_connect(sdio_cmd, cmd_msg);
            break;
        case INF_C_DISCONNECT:
            sdio_cmd_disconnect(sdio_cmd, bcdc);
            break;
        case INF_C_PING:
            sdio_cmd_ping(sdio_cmd, bcdc);
            break;
#if defined(SDIO_HM_AT_CMD)
        case INF_C_AT_CMD:
            sdio_cmd_at_cmd(sdio_cmd, cmd_msg);
            break;
#endif /* defined(SDIO_HM_AT_CMD) */
        default:
            PRINT_HM_ERROR(("unsupported set command %ld \n", bcdc->cmd));
            break;
    }

}

static void sdio_cmd_handler(sdio_command_t sdio_cmd, uint8_t *cmd_msg)
{
    sdio_bcdc_header_t *bcdc = (sdio_bcdc_header_t *)cmd_msg;

    PRINT_HM_INFO(("SDIO cmd %ld, len %ld, ver %ld, set %ld\n",
        bcdc->cmd, bcdc->len, BCDC_FLAG_VER(bcdc->flags), bcdc->flags & BCDC_FLAG_SET));

    if (BCDC_FLAG_VER(bcdc->flags) != BCDC_PROTO_VER) {
        PRINT_HM_ERROR(("BCDC version mismatch %d\n", BCDC_PROTO_VER));
        return;
    }

    if (bcdc->flags & BCDC_FLAG_SET)
        sdio_cmd_set(sdio_cmd, cmd_msg);
    else
        sdio_cmd_get(sdio_cmd, cmd_msg);
}

static void sdio_cmd_thread_func(cy_thread_arg_t arg)
{
    sdio_command_t sdio_cmd = (sdio_command_t)arg;
    sdio_cmd_msg_queue_t msg_queue_entry;
    cy_rslt_t result;

    do {
        whd_mem_memset(&msg_queue_entry, 0, sizeof(msg_queue_entry));
        result = cy_rtos_queue_get(&sdio_cmd->msgq, &msg_queue_entry, SDIO_CMD_WAITFOREVER);
        if (result != CY_RSLT_SUCCESS) {
            PRINT_HM_ERROR(("Message queue timeout.. \n"));
            continue;
        }

        sdio_cmd_handler(sdio_cmd, msg_queue_entry.cmd_msg);
        free(msg_queue_entry.cmd_msg);
    } while (true);

    /* Should never get here */
}

cy_rslt_t sdio_cmd_init(void *sdio_handler)
{
    sdio_handler_t sdio_hm = (sdio_handler_t)sdio_handler;
    sdio_command_t sdio_cmd = NULL;

    sdio_cmd = (sdio_command_t)whd_mem_malloc(sizeof(*sdio_cmd));
    if (!sdio_cmd) {
        PRINT_HM_ERROR(("sdio_cmd malloc failed\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }
    whd_mem_memset(sdio_cmd, 0, sizeof(*sdio_cmd));

    sdio_cmd->sdio_hm = sdio_hm;
    sdio_hm->sdio_cmd = sdio_cmd;

    CHK_RET(cy_rtos_queue_init(&sdio_cmd->msgq, SDIO_CMD_QUEUE_LEN, sizeof(sdio_cmd_msg_queue_t)));

    CHK_RET(cy_rtos_thread_create(&sdio_cmd->thread, sdio_cmd_thread_func, "SDIO command task",
        (void *)sdio_cmd_thread_stack, SDIO_CMD_STACK_SIZE, SDIO_CMD_TASK_PRIORITY, (cy_thread_arg_t)sdio_cmd));

    return CY_RSLT_SUCCESS;
}

/**
 * send data into message queue
 */
cy_rslt_t sdio_cmd_data_enq(sdio_command_t sdio_cmd, uint8_t *data, uint16_t data_len)
{
    uint8_t *msg = NULL;
    sdio_cmd_msg_queue_t msg_queue_entry;
    cy_rslt_t result;

    if (!sdio_cmd) {
        PRINT_HM_ERROR(("sdio cmd isn't initialized\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    msg = (uint8_t *)whd_mem_malloc(data_len);
    if (!msg) {
        PRINT_HM_ERROR(("msg malloc failed\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }
    whd_mem_memset(msg, 0, sizeof(*msg));
    whd_mem_memcpy(msg, data, data_len);

    msg_queue_entry.cmd_msg = msg;

    result = cy_rtos_put_queue(&sdio_cmd->msgq, &msg_queue_entry, 0, false);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("unable to put msg on queue\n"));
    }

    return result;
}

#endif /* COMPONENT_SDIO_HM */
