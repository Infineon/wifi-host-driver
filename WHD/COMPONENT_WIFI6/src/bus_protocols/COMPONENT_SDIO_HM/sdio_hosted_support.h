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

#ifndef _SDIO_HOSTED_SUPPORT_H_
#define _SDIO_HOSTED_SUPPORT_H_

#include "cy_linked_list.h"

#include "whd_int.h"

#include "sdio_command.h"

/* Enabling Info and Error msgs by default */
#define ENABLE_SDIO_HM_INFO
#define ENABLE_SDIO_HM_ERROR
/* #define ENABLE_SDIO_HM_DEBUG */
/* #define ENABLE_SDIO_HM_HEX */

/* DEBUG related Macros */
#define PRINT_MACRO(args) do { printf args; } while (0 == 1)

#ifdef ENABLE_SDIO_HM_INFO
#define PRINT_HM_INFO(args) PRINT_MACRO(args)
#else
#define PRINT_HM_INFO(args)
#endif

#ifdef ENABLE_SDIO_HM_DEBUG
#define PRINT_HM_DEBUG(args) PRINT_MACRO(args)
#else
#define PRINT_HM_DEBUG(args)
#endif

#ifdef ENABLE_SDIO_HM_ERROR
#define PRINT_HM_ERROR(args) PRINT_MACRO(args)
#else
#define PRINT_HM_ERROR(args)
#endif

#ifdef ENABLE_SDIO_HM_HEX
#define PRINT_HM_HEX(args, data, data_len) PRINT_HM_HEX_DUMP(args, data, data_len)
#else
#define PRINT_HM_HEX(args, data, data_len)
#endif

#define PRINT_HM_HEX_DUMP(args, data, data_len) { \
        uint16_t i, j; \
        PRINT_MACRO(args); \
        for (i = 0; i < data_len; i += 16) { \
            PRINT_MACRO("%08x:", i); \
            for (j = i; (j < data_len && j < (i + 16)); j++) { \
                PRINT_MACRO(" %02x", data[j]); \
            } \
            PRINT_MACRO("\n"); \
        } \
} \

#define CHK_RET(expr)  { \
        cy_rslt_t check_res = (expr); \
        if (check_res != CY_RSLT_SUCCESS) \
        { \
            PRINT_HM_ERROR(("Function %s failed at line %d type 0x%lx module 0x%lx submodule 0x%lx code 0x%lx\n", \
                __func__, __LINE__, \
                (check_res >> CY_RSLT_TYPE_POSITION) & CY_RSLT_TYPE_MASK, \
                (check_res >> CY_RSLT_MODULE_POSITION) & CY_RSLT_MODULE_MASK, \
                (check_res >> CY_RSLT_SUBMODULE_POSITION) & CY_RSLT_SUBMODULE_MASK, \
                (check_res >> CY_RSLT_CODE_POSITION) & CY_RSLT_CODE_MASK)); \
            return check_res; \
        } \
}

/* SDIO channel definition */

#define SDPCM_CONTROL_CHANNEL      0    /* Control */
#define SDPCM_EVENT_CHANNEL        1    /* Asyc Event Indication */
#define SDPCM_DATA_CHANNEL         2    /* Data Xmit/Recv */
#define SDPCM_GLOM_CHANNEL         3    /* Coalesced packets */
#define SDPCM_AT_CMD_CHANNEL       4    /* AT Command packets */
#define SDPCM_LOG_CHANNEL         12    /* LOG */
#define SDPCM_WRITE_CHANNEL       13    /* Test write packets */
#define SDPCM_LOOPBACK_CHANNEL    14    /* Test loopback packets */
#define SDPCM_TEST_CMD_CHANNEL    15    /* Test command packets */

/* SDIO Tx */

#define SDIO_TX_Q_CNT 10

#define SDIO_HM_TX_WHD(sdio_hm, buffer, length) sdio_hm_tx_buf_enq(sdio_hm, true, SDPCM_DATA_CHANNEL, buffer, length)
#define SDIO_HM_TX_EVENT(sdio_hm, buffer, length) sdio_hm_tx_buf_enq(sdio_hm, false, SDPCM_EVENT_CHANNEL, buffer, length)
#define SDIO_HM_TX_CMD(sdio_hm, buffer, length) sdio_hm_tx_buf_enq(sdio_hm, false, SDPCM_CONTROL_CHANNEL, buffer, length)

struct sdio_tx_info
{
    cy_mutex_t q_mutex;
    cy_linked_list_t tx_idle_q;
    cy_linked_list_t tx_wait_q;

    /* statistic */
    uint16_t err_enq;
    uint16_t err_busy;
    uint16_t err_deq;
    uint16_t err_send;
    uint16_t err_release;
    uint16_t err_whd;
    uint16_t err_event;
};

typedef struct sdio_tx_info *sdio_tx_info_t;

struct sdio_tx_q_node
{
    cy_linked_list_node_t node;  /* linked list node         */
    bool is_whd_buf;             /* whether use WHD buffer   */
    uint8_t channel;             /* use which channel        */
    void *buf_ptr;               /* pointer to actual buffer */
    uint16_t buf_len;            /* buffer length            */
};

typedef struct sdio_tx_q_node *sdio_tx_q_node_t;

/* SDIO Rx */

struct sdio_rx_info
{
    whd_buffer_t current_buffer;
    whd_buffer_t reserved_buffer;

    uint8_t next_seq;

    /* statistic */
    uint16_t err_len;
    uint16_t err_data;
    uint16_t err_hdr;
};

typedef struct sdio_rx_info *sdio_rx_info_t;

/* SDIO SW header */

typedef struct
{
    uint8_t sequence;               /* Rx/Tx sequence number */
    uint8_t channel_and_flags;      /*  4 MSB Channel number, 4 LSB arbitrary flag */
    uint8_t next_length;            /* Length of next data frame, reserved for Tx */
    uint8_t header_length;          /* Data offset */
    uint8_t wireless_flow_control;  /* Flow control bits, reserved for Tx */
    uint8_t bus_data_credit;        /* Maximum Sequence number allowed by firmware for Tx */
    uint8_t _reserved[2];           /* Reserved */
} sdio_sw_header_t;

struct sdio_handler
{
    sdiod_dma_descs_buf_t* _sdio_dma_desc;
    cyhal_sdio_t sdio_instance;
    whd_interface_t ifp;
    bool txpool_threshold;
    cy_thread_t thread;
    cy_semaphore_t thread_sema;
    cy_timer_t thread_timer;
    sdio_tx_info_t tx_info;
    sdio_rx_info_t rx_info;
    sdio_command_t sdio_cmd;
    bool sdio_thread_active;
    bool sdio_rx_timer_active;
};

typedef struct sdio_handler *sdio_handler_t;

/**
 * Enumeration of Infineon events
 */
enum {
    INF_NW_EVENT   = 0,
    INF_SCAN_EVENT = 1,
    INF_AT_EVENT   = 2,
};

/**
 * struct inf_event - contents of event packet.
 *
 * @type: event type.
 * @len: event data length.
 * @version: event version.
 * @u: pointer to event data.
 */
typedef struct {
    uint16_t type;
    uint16_t len;
    uint8_t ver;
} __packed inf_event_base_t;

/* Network Event */

#define NW_EVENT_VER 1

/**
 * Enumeration of network events
 */
enum {
    NW_EVENT_CONNECTED       = 0, /* STA connected to the AP.         */
    NW_EVENT_CONNECT_FAILED  = 1, /* STA connection to the AP failed. */
    NW_EVENT_RECONNECTED     = 2, /* STA reconnected to the AP.       */
    NW_EVENT_DISCONNECTED    = 3, /* STA disconnected from the AP.    */
    NW_EVENT_IP_CHANGED      = 4, /* IP address change event.         */
};

/**
 * struct nw_event - contents of network event.
 *
 * @type: nw event type.
 * @u: pointer to nw event data.
 */
typedef struct {
    uint8_t type;
    union {
        uint32_t ipv4;   /**< IPv4 address in network byte order. */
        uint32_t reason;
    } u;
} __packed nw_event_t ;

typedef struct {
    inf_event_base_t base;
    nw_event_t nw;
} __packed inf_nw_event_t;

/* SDIO Event */

#define SCAN_EVENT_VER 1

/**
 * struct scan_event - contents of scan event.
 */
typedef struct
{
    uint8_t            ssid_length;                   /* SSID length                          */
    char               ssid[CY_WCM_MAX_SSID_LEN + 1]; /* SSID of the access point             */
    uint32_t           security_type;                 /* WiFi security type                   */
    uint8_t            macaddr[CY_WCM_MAC_ADDR_LEN];  /* MAC Address of the access point      */
    int16_t            signal_strength;               /* signal strength                      */
    uint8_t            channel;                       /* channel number                       */
    uint8_t            band;                          /* WiFi band of access point            */
    bool               scan_complete;                 /* WiFi scan complete/incomplete        */
} __packed scan_event_t;

typedef struct {
    inf_event_base_t base;
    scan_event_t scan;
} __packed inf_scan_event_t;

#if defined(SDIO_HM_AT_CMD)

/* AT Command Event */

#define AT_EVENT_VER 1

typedef struct {
    inf_event_base_t base;
    char string[AT_CMD_BUF_SIZE];
} __packed inf_at_event_t;

#endif /* SDIO_HM_AT_CMD */

/* Function prototypes */

cy_rslt_t sdio_hm_tx_buf_enq(sdio_handler_t sdio_hm, bool is_whd, uint8_t channel, void *buffer, uint16_t length);
cy_rslt_t sdio_hm_init(void);         //will be moved to only internal, now allowing application to call.
sdio_handler_t sdio_hm_get_sdio_handler();

#endif /* _SDIO_HOSTED_SUPPORT_H_ */
