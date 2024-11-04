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

#ifndef INCLUDED_WHD_MSGBUF_H
#define INCLUDED_WHD_MSGBUF_H

#include "whd.h"
#include "whd_events_int.h"
#include "whd_network_types.h"
#include "whd_types_int.h"
#include "whd_wlioctl.h"
#include "whd_commonring.h"

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************
*             Constants
******************************************************/
#define IOCTL_OFFSET (sizeof(whd_buffer_header_t) + 12 + 16)

#define DATA_AFTER_HEADER(x)   ( (void *)(&x[1]) )

#define WHD_IOCTL_PACKET_TIMEOUT      (5000)     /* IOCTL get buffer Timeout */
#define WHD_IOCTL_TIMEOUT_MS          (3000)     /** Need to give enough time for coming out of Deep sleep (was 5000) */
#define WHD_IOCTL_NO_OF_RETRIES       (4)        /** If the IOCTL reponse not received, do these number of retry **/

#define WHD_EVENT_HANDLER_LIST_SIZE   (5)        /** Maximum number of simultaneously registered event handlers */

/* The below two macro values, depends on WLAN FW configuration,
 * update these macros, in case it is changed by WLAN FW,
 * Firmware will need FW_WL_D11_BUF_POST buffers for D11 DMA and FW_M2MDMA_BUF_POST buffers
 * for packets that takes the M2M DMA path to CM33 SRAM */
#define FW_WL_D11_BUF_POST            (8)
#define FW_M2MDMA_BUF_POST            (4)

#define MSGBUF_TYPE_GEN_STATUS                  0x1
#define MSGBUF_TYPE_RING_STATUS                 0x2
#define MSGBUF_TYPE_FLOW_RING_CREATE            0x3
#define MSGBUF_TYPE_FLOW_RING_CREATE_CMPLT      0x4
#define MSGBUF_TYPE_FLOW_RING_DELETE            0x5
#define MSGBUF_TYPE_FLOW_RING_DELETE_CMPLT      0x6
#define MSGBUF_TYPE_FLOW_RING_FLUSH             0x7
#define MSGBUF_TYPE_FLOW_RING_FLUSH_CMPLT       0x8
#define MSGBUF_TYPE_IOCTLPTR_REQ                0x9
#define MSGBUF_TYPE_IOCTLPTR_REQ_ACK            0xA
#define MSGBUF_TYPE_IOCTLRESP_BUF_POST          0xB
#define MSGBUF_TYPE_IOCTL_CMPLT                 0xC
#define MSGBUF_TYPE_EVENT_BUF_POST              0xD
#define MSGBUF_TYPE_WL_EVENT                    0xE
#define MSGBUF_TYPE_TX_POST                     0xF
#define MSGBUF_TYPE_TX_STATUS                   0x10
#define MSGBUF_TYPE_RXBUF_POST                  0x11
#define MSGBUF_TYPE_RX_CMPLT                    0x12
#define MSGBUF_TYPE_LPBK_DMAXFER                0x13
#define MSGBUF_TYPE_LPBK_DMAXFER_CMPLT          0x14
#define MSGBUF_TYPE_H2D_MAILBOX_DATA            0x23
#define MSGBUF_TYPE_D2H_MAILBOX_DATA            0x24

#define NR_TX_PKTIDS                            24
#define NR_RX_PKTIDS                            24

#define WHD_IOCTL_REQ_PKTID                     0xFFFE

#define WHD_MSGBUF_IOCTL_MAX_TX_SIZE           (1500)    /* Should be less than 2KB(IOCTL inbut Buffer) */
#define WHD_MSGBUF_IOCTL_MAX_RX_SIZE           (8192)
#define WHD_MSGBUF_EVENT_MAX_RX_SIZE           (1500)

#define WHD_MSGBUF_DATA_MAX_RX_SIZE            (2048 - sizeof(whd_buffer_header_t))

#define WHD_MSGBUF_MAX_IOCTLRESPBUF_POST        2
#define WHD_MSGBUF_MAX_EVENTBUF_POST            2

#define WHD_MSGBUF_RXBUFPOST_TIMER_DELAY        60000    /* RX buffer Timer expiry timeout in milliseconds */
#define WHD_MSGBUF_RXBUFPOST_RETRY_COUNT        10

#define WHD_MSGBUF_SLP_DETECT_TIME              2000     /* Sleep Detect Timer expiry timeout in milliseconds */

#define WHD_MSGBUF_PKT_FLAGS_FRAME_802_3        0x01
#define WHD_MSGBUF_PKT_FLAGS_FRAME_802_11       0x02
#define WHD_MSGBUF_PKT_FLAGS_FRAME_MASK         0x07
#define WHD_MSGBUF_PKT_FLAGS_PRIO_SHIFT         5

#define WHD_MSGBUF_TX_FLUSH_CNT1                32
#define WHD_MSGBUF_TX_FLUSH_CNT2                96
#define WHD_MSGBUF_UPDATE_RX_PTR_THRS           48

#define WHD_MAX_TXSTATUS_WAIT_RETRIES           10

#define WHD_EVENT_HANDLER_LIST_SIZE             (5)  /** Maximum number of simultaneously registered event handlers */
#define MAX_CLIENT_SUPPORT                      (2)  /* No of Max STA connection supported in AP Mode */

#define WHD_NROF_H2D_COMMON_MSGRINGS      2
#define WHD_NROF_D2H_COMMON_MSGRINGS      3
#define WHD_NROF_COMMON_MSGRINGS    (WHD_NROF_H2D_COMMON_MSGRINGS + \
                                     WHD_NROF_D2H_COMMON_MSGRINGS)

#if defined(RX_PACKET_POOL_SIZE) && (RX_PACKET_POOL_SIZE >= 16)
#define WHD_MSGBUF_RXBUFPOST_THRESHOLD          (RX_PACKET_POOL_SIZE - WHD_MSGBUF_MAX_EVENTBUF_POST - \
                                                     FW_WL_D11_BUF_POST - FW_M2MDMA_BUF_POST)
#elif defined(RX_PACKET_POOL_SIZE) && (RX_PACKET_POOL_SIZE < 16)
#error "RX_PACKET_POOL_SIZE Should be greater or equal to 16, as per WLAN design"
#else
#define WHD_MSGBUF_RXBUFPOST_THRESHOLD          2
#endif

#ifdef PROTO_MSGBUF
/** Error list element structure
 *
 * events : set event of error type
 * handler: A pointer to the whd_error_handler_t function that will receive the event
 * handler_user_data : User provided data that will be passed to the handler when a matching event occurs
 */
typedef struct
{
    whd_error_handler_t handler;
    void *handler_user_data;
    whd_bool_t event_set;
    uint8_t events;
} error_list_elem_t;

typedef struct whd_error_info
{
    /* Event list variables */
    error_list_elem_t whd_event_list[WHD_EVENT_HANDLER_LIST_SIZE];
    cy_semaphore_t event_list_mutex;
} whd_error_info_t;
#endif

/* Forward declarations */
struct whd_flowring;

typedef struct whd_msgbuf_info
{
    /* Event list variables (Must be at the begining) */
    event_list_elem_t whd_event_list[WHD_EVENT_HANDLER_LIST_SIZE];
    cy_semaphore_t event_list_mutex;

    /* IOCTL variables*/
    cy_semaphore_t ioctl_mutex;
    whd_buffer_t ioctl_response;
    cy_semaphore_t ioctl_sleep;
    uint8_t ioctl_response_status;
    uint32_t ioctl_response_length;
    uint32_t ioctl_response_pktid;
} whd_msgbuf_info_t;

typedef struct whd_msgbuftx_info
{
    /* Packet send queue variables */
    cy_semaphore_t send_queue_mutex;
    whd_buffer_t send_queue_head;
    whd_buffer_t send_queue_tail;
    uint32_t npkt_in_q;
} whd_msgbuftx_info_t;

typedef struct
{
    uint8_t destination_address[ETHER_ADDR_LEN];
    uint8_t source_address[ETHER_ADDR_LEN];
    uint16_t ethertype;
} ether_header_t;

struct msg_buf_addr
{
    uint32_t low_addr;
    uint32_t high_addr;
};

struct msgbuf_common_hdr
{
    uint8_t msgtype;
    uint8_t ifidx;
    uint8_t flags;
    uint8_t rsvd0;
    uint32_t request_ptr;
};

struct msgbuf_ioctl_req_hdr
{
    struct msgbuf_common_hdr msg;
    uint32_t cmd;
    uint16_t trans_id;
    uint16_t input_buf_len;
    uint16_t output_buf_len;
    uint16_t rsvd0[3];
    struct msg_buf_addr req_buf_addr;
    uint32_t rsvd1[2];
};

struct msgbuf_tx_msghdr
{
    struct msgbuf_common_hdr msg;
    uint8_t txhdr[WHD_ETHERNET_SIZE];
    uint8_t flags;
    uint8_t seg_cnt;
    struct msg_buf_addr metadata_buf_addr;
    struct msg_buf_addr data_buf_addr;
    uint16_t metadata_buf_len;
    uint16_t data_len;
    uint32_t rsvd0;
};

struct msgbuf_h2d_mbdata
{
    struct msgbuf_common_hdr msg;
    uint32_t mbdata;
    uint16_t rsvd0[7];
};

struct msgbuf_rx_bufpost
{
    struct msgbuf_common_hdr msg;
    uint16_t metadata_buf_len;
    uint16_t data_buf_len;
    uint32_t rsvd0;
    struct msg_buf_addr metadata_buf_addr;
    struct msg_buf_addr data_buf_addr;
};

struct msgbuf_rx_ioctl_resp_or_event
{
    struct msgbuf_common_hdr msg;
    uint16_t host_buf_len;
    uint16_t rsvd0[3];
    struct msg_buf_addr host_buf_addr;
    uint32_t rsvd1[4];
};

struct msgbuf_completion_hdr
{
    uint16_t status;
    uint16_t flow_ring_id;
};

/* Data struct for the MSGBUF_TYPE_GEN_STATUS */
struct msgbuf_gen_status
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t write_idx;
    uint32_t rsvd0[3];
};

/* Data struct for the MSGBUF_TYPE_RING_STATUS */
struct msgbuf_ring_status
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t write_idx;
    uint16_t rsvd0[5];
};

struct msgbuf_rx_event
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t event_data_len;
    uint16_t seqnum;
    uint16_t rsvd0[4];
};

struct msgbuf_ioctl_resp_hdr
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t resp_len;
    uint16_t trans_id;
    uint32_t cmd;
    uint32_t rsvd0;
};

struct msgbuf_tx_status
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t metadata_len;
    uint16_t tx_status;
};

struct msgbuf_rx_complete
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint16_t metadata_len;
    uint16_t data_len;
    uint16_t data_offset;
    uint16_t flags;
    uint32_t rx_status_0;
    uint32_t rx_status_1;
    uint32_t rsvd0;
};

struct msgbuf_tx_flowring_delete_req
{
    struct msgbuf_common_hdr msg;
    uint16_t flow_ring_id;
    uint16_t reason;
    uint32_t rsvd0[7];
};

struct msgbuf_tx_flowring_create_req
{
    struct msgbuf_common_hdr msg;
    uint8_t da[ETHER_ADDR_LEN];
    uint8_t sa[ETHER_ADDR_LEN];
    uint8_t tid;
    uint8_t if_flags;
    uint16_t flow_ring_id;
    uint8_t tc;
    uint8_t priority;
    uint16_t int_vector;
    uint16_t max_items;
    uint16_t len_item;
    struct msg_buf_addr flow_ring_addr;
};

struct msgbuf_flowring_create_resp
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint32_t rsvd0[3];
};

struct msgbuf_flowring_delete_resp
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint32_t rsvd0[3];
};

struct msgbuf_flowring_flush_resp
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint32_t rsvd0[3];
};

struct msgbuf_d2h_mailbox_data
{
    struct msgbuf_common_hdr msg;
    struct msgbuf_completion_hdr compl_hdr;
    uint32_t mbdata;
    uint32_t rsvd0[2];
};

struct whd_msgbuf_work_item
{
    uint32_t flowid;
    int ifidx;
    uint8_t sa[ETHER_ADDR_LEN];
    uint8_t da[ETHER_ADDR_LEN];
};

struct whd_msgbuf_pktid
{
    uint16_t allocated;
    uint16_t data_offset;
    whd_buffer_t skb;
    uint32_t physaddr;
};

struct whd_msgbuf_pktids
{
    uint32_t array_size;
    uint32_t last_allocated_idx;
    struct whd_msgbuf_pktid *array;
    cy_semaphore_t pktid_mutex;
};

struct whd_msgbuf
{
    struct whd_driver *drvr;
    struct whd_commonring *commonrings[WHD_NROF_COMMON_MSGRINGS];
    struct whd_commonring **flowrings;
    void *ioctbuf;
    whd_buffer_t ioctl_queue;
    whd_buffer_t ioctl_buffer;
    uint32_t ioctl_cmd;
    uint8_t ifidx; // used for ioctl to disguish which interface
    uint8_t resrv[3];
    uint32_t ioctbuf_phys_hi;
    uint32_t ioctbuf_phys_lo;
    uint32_t *flowring_handle;
    uint16_t max_flowrings;
    uint16_t max_submissionrings;
    uint16_t max_completionrings;
    uint16_t rx_dataoffset;
    uint32_t max_rxbufpost;
    uint16_t rx_metadata_offset;
    uint32_t rxbufpost;
    uint32_t max_ioctlrespbuf;
    uint32_t cur_ioctlrespbuf;
    uint32_t max_eventbuf;
    uint32_t cur_eventbuf;
    uint16_t reqid;
    struct whd_msgbuf_pktids *tx_pktids;
    struct whd_msgbuf_pktids *rx_pktids;
    struct whd_flowring *flow;
    uint8_t *flow_map;
    uint32_t priority;
    uint32_t current_flowring_count;
};

extern whd_result_t whd_msgbuf_send_mbdata(struct whd_driver *drvr, uint32_t mbdata);
extern whd_result_t whd_msgbuf_ioctl_dequeue(struct whd_driver *whd_driver);
extern uint32_t whd_msgbuf_process_rx_packet(struct whd_driver *dev);
extern void whd_msgbuf_delete_flowring(struct whd_driver *drvr, uint16_t flowid);
extern whd_result_t whd_msgbuf_txflow(struct whd_driver *drvr, uint16_t flowid);
extern whd_result_t whd_get_high_priority_flowring(whd_driver_t whd_driver, uint32_t num_flowring, uint16_t *prio_ring_id);
extern whd_result_t whd_msgbuf_txflow_dequeue(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t flowid);
extern whd_result_t whd_msgbuf_txflow_init(whd_msgbuftx_info_t *msgtx_info);
extern whd_result_t whd_msgbuf_txflow_deinit(whd_msgbuftx_info_t *msgtx_info);
extern whd_result_t whd_msgbuf_info_init(whd_driver_t whd_driver);
extern void whd_msgbuf_info_deinit(whd_driver_t whd_driver);

extern void whd_msgbuf_indicate_to_fill_buffers(cy_timer_callback_arg_t arg);
extern void whd_msgbuf_rxbuf_fill_all(struct whd_msgbuf *msgbuf);
extern void whd_wifi_rxbuf_fill_timer_init(whd_driver_t whd_driver);
extern void whd_wifi_rxbuf_fill_timer_start(whd_driver_t whd_driver);
extern void whd_wifi_rxbuf_fill_timer_deinit(whd_driver_t whd_driver);
extern void whd_wifi_rxbuf_fill_timer_stop(whd_driver_t whd_driver);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
