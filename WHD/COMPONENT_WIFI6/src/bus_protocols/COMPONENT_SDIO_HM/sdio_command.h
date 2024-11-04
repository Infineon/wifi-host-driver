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

#ifndef _SDIO_GENERAL_COMMAND_H_
#define _SDIO_GENERAL_COMMAND_H_

#if defined(COMPONENT_SDIO_HM)

#include "cy_wcm.h"

#define SDIO_CMD_WAITFOREVER ( (uint32_t)0xffffffffUL )
#define SDIO_CMD_QUEUE_LEN   (10)

/**
 * Enumeration of Infineon command
 */
enum {
    INF_C_MAC            = 1,
    INF_C_IPv4           = 2,
    INF_C_GATEWAY_IP     = 3,
    INF_C_NETMASK        = 4,
    INF_C_AP_INFO        = 5,
    INF_C_IP_INFO        = 6,
    INF_C_SCAN           = 7,
    INF_C_CONNECT        = 8,
    INF_C_DISCONNECT     = 9,
    INF_C_PING           = 10,
    INF_C_AT_CMD         = 11,
};

/**
 * SDIO command queue structure
 */
typedef struct
{
    uint8_t *cmd_msg;
} sdio_cmd_msg_queue_t;


#define BCDC_PROTO_VER      2           /* Protocol version */

/* BCDC flag definitions */
#define BCDC_FLAG_SET       0x02        /* 0=get, 1=set cmd */
#define BCDC_FLAG_ID_MASK   0xFFFF0000  /* id an cmd pairing */
#define BCDC_FLAG_ID_SHIFT  16          /* ID Mask shift bits */
#define BCDC_FLAG_ID(flags) \
    (((flags) & BCDC_FLAG_ID_MASK) >> BCDC_FLAG_ID_SHIFT)

#define BCDC_FLAG_VER_MASK  0xf0        /* Protocol version mask */
#define BCDC_FLAG_VER_SHIFT 4           /* Protocol version shift */
#define BCDC_FLAG_VER(flags) \
    (((flags) & BCDC_FLAG_VER_MASK) >> BCDC_FLAG_VER_SHIFT)

/* BCDC header */
typedef struct  {
    uint32_t cmd;        /* command value */
    uint32_t len;        /* buffer length (excludes header) */
    uint32_t flags;      /* flag definitions */
    uint32_t status;     /* status code returned from the device */
} sdio_bcdc_header_t;

/**
 * wifi connect structure
 */
typedef struct
{
    uint8_t          ssid_length;                         /* SSID length */
    char             ssid[CY_WCM_MAX_SSID_LEN + 1];       /* SSID of the access point */
    uint32_t         security_type;                       /* WiFi security type */
    char             password[CY_WCM_MAX_PASSPHRASE_LEN]; /* Length of WIFI password */
    uint8_t          macaddr[CY_WCM_MAC_ADDR_LEN];        /* MAC Address of the access point */
    uint8_t          band;                                /* WiFi band of access point */
    uint32_t         ip_address;                          /* static IP address */
    uint32_t         gateway;                             /* static gateway */
    uint32_t         netmask;                             /* static netmask */
} __packed sdio_cmd_connect_t;

#if defined(SDIO_HM_AT_CMD)

#define AT_CMD_BUF_SIZE 200

/**
 * AT command structure
 */
typedef struct
{
    char string[AT_CMD_BUF_SIZE];
} __packed sdio_cmd_at_t;

#endif /* SDIO_HM_AT_CMD */

/**
 * Get mac structure
 */
typedef struct
{
    sdio_bcdc_header_t bcdc;                              /* BCDC header structure */
    uint8_t            mac_addr[CY_WCM_MAC_ADDR_LEN];
} __packed sdio_cmd_mac_rsp_t;

/**
 * Get mac structure
 */
typedef struct
{
    sdio_bcdc_header_t bcdc;                              /* BCDC header structure */
    uint32_t           ip;
} __packed sdio_cmd_ip_rsp_t;

/**
 * Get associated AP information
 */
typedef struct
{
    sdio_bcdc_header_t bcdc;                              /* BCDC header structure */
    uint8_t            ssid_length;                       /* SSID length */
    char               ssid[CY_WCM_MAX_SSID_LEN + 1];     /* SSID of the access point */
    uint32_t           security_type;                     /* WiFi security type */
    uint8_t            bssid[CY_WCM_MAC_ADDR_LEN];        /* MAC Address of the access point */
    uint16_t           channel_width;                     /* channel width   */
    int16_t            signal_strength;                   /* signal strength */
    uint8_t            channel;                           /* channel number  */
} __packed sdio_cmd_ap_info_rsp_t;

/**
 * Get IPv4 Address, Netmask, Gateway and DNS Addresses
 */
typedef struct
{
    sdio_bcdc_header_t bcdc;       /* BCDC header structure */
    bool               dhcp;       /* true if DHCP is enabled else false      */
    uint32_t           address;    /* IP Address of the device                */
    uint32_t           netmask;    /* netmask address                         */
    uint32_t           gateway;    /* gateway address                         */
    uint32_t           primary;    /* primary DNS address                     */
    uint32_t           secondary;  /* secondary DNS address                   */
} __packed sdio_cmd_ip_info_rsp_t ;

/**
 * ICMP ping structure
 */
typedef struct
{
    sdio_bcdc_header_t bcdc;          /* BCDC header structure */
    uint32_t           elapsed_time;  /* elapsed time */
} __packed sdio_cmd_ping_rsp_t;

struct sdio_command
{
    void *sdio_hm;
    cy_queue_t msgq;
    cy_thread_t thread;
#if defined(SDIO_HM_AT_CMD)
    char at_cmd_buf[AT_CMD_BUF_SIZE];
#endif
};

typedef struct sdio_command *sdio_command_t;

cy_rslt_t sdio_cmd_init(void *sdio_handler);
cy_rslt_t sdio_cmd_data_enq(sdio_command_t sdio_cmd, uint8_t *data, uint16_t data_len);

#endif /* COMPONENT_SDIO_HM */

#endif /* _SDIO_GENERAL_COMMAND_H_ */
