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

/* Header file includes */
#include "cyhal.h"
#include "cybsp.h"
#include "cyhal_sdio.h"
#include "sdiod_api.h"
#include "cyabs_rtos.h"
#include "cy_network_mw_core.h"
#include "cy_wcm.h"
#include "cy_linked_list.h"
#include "cy_network_buffer.h"
#include "whd_hw.h"
#include "whd_utils.h"
#include "whd_network_if.h"
#include "whd_buffer_api.h"
#include "sdio_hosted_support.h"
#include "sdio_arbitration.h"

/******************************************************************************
* Macros
******************************************************************************/

/* RTOS related macros */
#define SDIO_STACK_SIZE        (4 * 1024)
#define SDIO_TASK_PRIORITY     (CY_RTOS_PRIORITY_REALTIME)
__attribute__((aligned(8)))    uint8_t sdio_stack[SDIO_STACK_SIZE] = {0};

#define SDIO_SW_HEADER_LEN     8
#define SDIO_FRAME_MAX_PAYLOAD (SDIO_F2_FRAME_MAX_PAYLOAD - SDIO_SW_HEADER_LEN)

/* SDPCM Software Header */
/* 31:24 (Byte 7)   23:16 (Byte 6)  15:12 (Byte 5)   11:8 (Byte 5)    7:0 (Byte 4) */
/* DataOffset       NextLen         Flags            Channel          Sequence     */
#define SDPCM_SEQ_MASK            0x000000ff
#define SDPCM_SEQ_WRAP            256
#define SDPCM_CHANNEL_MASK        0x00000f00
#define SDPCM_CHANNEL_SHIFT       8
#define SDPCM_FLAGS_MASK          0x0000f000
#define SDPCM_FLAGS_SHIFT         12
#define SDPCM_NEXT_LEN_MASK       0x00ff0000
#define SDPCM_NEXT_LEN_SHIFT      16
#define SDPCM_DATA_OFFSET_MASK    0xff000000
#define SDPCM_DATA_OFFSET_SHIFT   24

/* 31:24 (Byte 11)  23:16 (Byte 10)  15:8 (Byte 9)     7:0 (Byte 8)            */
/* Reserved2        Reserved1        Bus Data Credit   Wireless Flow Control   */
#define SDPCM_FLOW_CONTROL_MASK    0x000000ff
#define SDPCM_FLOW_CONTROL_SHIFT   0
#define SDPCM_DATA_CREDIT_MASK     0x0000ff00
#define SDPCM_DATA_CREDIT_SHIFT    8
#define SDPCM_RESERVED1_MASK       0x00ff0000
#define SDPCM_RESERVED1_SHIFT      16
#define SDPCM_RESERVED2_MASK       0xff000000
#define SDPCM_RESERVED2_SHIFT      24

#define THREAD_TIME                10    /* 10ms to check buffer availability */

/******************************************************************************
* Global variables
******************************************************************************/
extern whd_interface_t whd_ifs[2];
static sdio_handler_t sdio_hm = NULL;

#if defined(SDIO_HM_TEST)

/* Error Count */
uint32_t rx_err_hdr, rx_err_data;

/* Throughput Test */
bool tp_test = false;
cy_timer_t tp_test_timer;
uint32_t tp_test_ok, tp_test_err, tp_test_cnt;
#define TEST_TOTAL_TIME 60000 /* unit: ms */

#endif /* defined(SDIO_HM_TEST) */

/******************************************************************************
* Enumeartions
******************************************************************************/
typedef enum {
    TEST_CMD_TP_START = 1,
    TEST_CMD_TP_STOP,
    TEST_CMD_RX_INC_START,
    TEST_CMD_RX_DEC_START,
    TEST_CMD_RX_TP_START,
} ifx_test_cmd;

/******************************************************************************
* Structures
******************************************************************************/

struct inf_test_msg_t {
    uint8_t cmd;
};

/******************************************************************************
* Function prototypes
*******************************************************************************/
static void sdio_hm_rx_process(sdio_handler_t sdio_hm, uint8_t *payload, uint16_t length);
static cy_rslt_t sdio_hm_build_pkt_for_host(sdio_handler_t sdio_hm,
                                                     uint8_t *data, uint16_t data_len, uint8_t channel);
static cy_rslt_t sdio_hm_tx_prepare(sdio_handler_t sdio_hm,
                                                      uint8_t *data, uint16_t data_len, uint8_t channel);
static cy_rslt_t sdio_hm_data_to_host(sdio_handler_t sdio_hm, uint8_t *data, uint16_t data_len);
static cy_rslt_t sdio_hm_data_from_host(sdio_handler_t sdio_hm);

#if defined(SDIO_HM_TEST)
static void sdio_hm_tp_timer_cb(cy_timer_callback_arg_t arg);
static void sdio_hm_process_at_cmd(at_cmd_msg_base_t *host_cmd, uint16_t length);
static void sdio_hm_process_test_cmd(sdio_handler_t sdio_hm,
                                                              struct inf_test_msg_t *msg, uint16_t length);
#endif /* defined(SDIO_HM_TEST) */

/******************************************************************************
* Function Definitions
******************************************************************************/

static void sdio_hm_thread_trigger(sdio_handler_t sdio_hm, bool in_isr)
{
    cy_rtos_set_semaphore(&sdio_hm->thread_sema, in_isr);
}

static cy_rslt_t sdio_hm_node_enq(sdio_tx_info_t txi, cy_linked_list_t *q, sdio_tx_q_node_t q_node)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    CHK_RET(cy_rtos_mutex_get(&txi->q_mutex, CY_RTOS_NEVER_TIMEOUT));

    result = cy_linked_list_insert_node_at_rear(q, &q_node->node);
    if (result != CY_RSLT_SUCCESS) {
        txi->err_enq++;
        PRINT_HM_ERROR(("insert node into q failed\n"));
    }

    CHK_RET(cy_rtos_mutex_set(&txi->q_mutex));

    return result;
}

static cy_rslt_t sdio_hm_node_deq(sdio_tx_info_t txi, cy_linked_list_t *q, sdio_tx_q_node_t *q_node)
{
    cy_linked_list_node_t *node = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    CHK_RET(cy_rtos_mutex_get(&txi->q_mutex, CY_RTOS_NEVER_TIMEOUT));

    result = cy_linked_list_remove_node_from_front(q, &node);
    if (result != CY_RSLT_SUCCESS) {
        txi->err_deq++;
        PRINT_HM_DEBUG(("q is empty\n"));
    }

    CHK_RET(cy_rtos_mutex_set(&txi->q_mutex));

    *q_node = (sdio_tx_q_node_t)node;

    return result;
}

static cy_rslt_t sdio_hm_q_init(sdio_tx_info_t txi)
{
    sdio_tx_q_node_t txq_node = NULL;
    uint8_t i;

    CHK_RET(cy_rtos_mutex_init(&txi->q_mutex, false));
    CHK_RET(cy_linked_list_init(&txi->tx_idle_q));
    CHK_RET(cy_linked_list_init(&txi->tx_wait_q));

    for (i = 0; i < SDIO_TX_Q_CNT; i++) {
        txq_node = (sdio_tx_q_node_t)whd_mem_malloc(sizeof(*txq_node));
        if (!txq_node) {
            PRINT_HM_ERROR(("txq_node malloc failed\n"));
            return CYHAL_SDIO_RSLT_ERR_UNSUPPORTED;
        }
        CHK_RET(cy_linked_list_insert_node_at_front(&txi->tx_idle_q, &txq_node->node));
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_tx_from_q(sdio_handler_t sdio_hm)
{
    sdio_tx_info_t txi = sdio_hm->tx_info;
    whd_driver_t whd_driver = sdio_hm->ifp->whd_driver;
    sdio_tx_q_node_t q_node = NULL;
    uint8_t *data = NULL;
    uint16_t data_len = 0;
    uint32_t wait_q_cnt;
    sdiod_status_t sdiod_status;
    whd_result_t whd_result;
    cy_rslt_t result;

    /* check queue isn't empty */
    cy_linked_list_get_count(&txi->tx_wait_q, &wait_q_cnt);
    if (!wait_q_cnt)
        return CYHAL_SDIO_RSLT_CANCELED;

    /* check sdio bus status */
    sdiod_status = sdiod_get_TxStatus();
    if (sdiod_status != SDIOD_STATUS_SUCCESS) {
        txi->err_busy++;
        sdio_hm_thread_trigger(sdio_hm, false);
        return CYHAL_SDIO_DEV_RSLT_WRITE_ERROR;
    }

    CHK_RET(sdio_hm_node_deq(sdio_hm->tx_info, &txi->tx_wait_q, &q_node));

    CY_ASSERT(NULL != q_node);

    if (q_node->is_whd_buf) {
        data = whd_buffer_get_current_piece_data_pointer(whd_driver, q_node->buf_ptr);
        data_len = whd_buffer_get_current_piece_size(whd_driver, q_node->buf_ptr);
    } else {
        data = (uint8_t *)q_node->buf_ptr;
        data_len = q_node->buf_len;
    }

    /* send data to external host via sdio bus */
    result = sdio_hm_tx_prepare(sdio_hm, data, data_len, q_node->channel);
    if (result != SDIOD_STATUS_SUCCESS) {
        txi->err_send++;
        PRINT_HM_ERROR(("tx data failed: %ld\n", result));
        return result;
    }

    if (q_node->is_whd_buf) {
        whd_result = whd_buffer_release(whd_driver, q_node->buf_ptr, WHD_NETWORK_RX);
        if (whd_result != WHD_SUCCESS) {
            txi->err_release++;
            PRINT_HM_ERROR(("release whd buffer failed: %ld\n", whd_result));
            return CYHAL_SDIO_DEV_RSLT_WRITE_ERROR;
        }
    } else {
        free(q_node->buf_ptr);
    }

    CHK_RET(sdio_hm_node_enq(txi, &txi->tx_idle_q, q_node));

    return CY_RSLT_SUCCESS;
}

static void sdio_hm_process_ethernet_data(whd_interface_t ifp, whd_buffer_t buffer)
{
    sdio_tx_info_t txi = sdio_hm->tx_info;
    uint8_t *data = whd_buffer_get_current_piece_data_pointer(ifp->whd_driver, buffer);
    uint16_t data_len = whd_buffer_get_current_piece_size(ifp->whd_driver, buffer);
    cy_rslt_t result = CY_RSLT_SUCCESS;

    if (sdio_arb_eth_packet_recv_handle(data))
    {
        /* forward buffer to internal network stack */
        cy_network_process_ethernet_data(ifp, buffer);
        return;
    }
    else
    {
        result = SDIO_HM_TX_WHD(sdio_hm, buffer, data_len);
        if (result != CY_RSLT_SUCCESS)
        {
            txi->err_whd++;
            result = whd_buffer_release(ifp->whd_driver, buffer, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
            {
                txi->err_release++;
                PRINT_HM_ERROR(("release whd buffer failed: %ld\n", result));
            }
        }
        return;
    }
}

/**
 *  event callback for handling wcm connection events
 */
static void sdio_hm_wcm_event_callback(cy_wcm_event_t event, cy_wcm_event_data_t *event_data)
{
    inf_nw_event_t *inf_event = NULL;
    inf_event_base_t *base = NULL;
    nw_event_t *nw = NULL;
    cy_rslt_t result;

    if (!sdio_hm->sdio_instance.is_ready)
        return;

    PRINT_HM_DEBUG(("Receive WCM event = %d\n", event));

    inf_event = (inf_nw_event_t *)whd_mem_malloc(sizeof(*inf_event));
    if (!inf_event) {
        PRINT_HM_ERROR(("inf_nw_event_t malloc failed\n"));
        return;
    }

    whd_mem_memset(inf_event, 0, sizeof(*inf_event));

    base = &inf_event->base;
    base ->ver = NW_EVENT_VER;
    base ->type = INF_NW_EVENT;
    base ->len = sizeof(*nw);
    nw = &inf_event->nw;

    switch (event)
    {
        case CY_WCM_EVENT_CONNECTED:
#ifdef SDIO_HM_TRACK_SESSION
            sdio_arb_network_timer_start();
#endif /* SDIO_HM_TRACK_SESSION */
            nw->type = NW_EVENT_CONNECTED;
            break;
        case CY_WCM_EVENT_CONNECT_FAILED:
            nw->type = NW_EVENT_CONNECT_FAILED;
            break;
        case CY_WCM_EVENT_RECONNECTED:
#ifdef SDIO_HM_TRACK_SESSION
            sdio_arb_network_timer_start();
#endif /* SDIO_HM_TRACK_SESSION */
            nw->type = NW_EVENT_RECONNECTED;
            break;
        case CY_WCM_EVENT_DISCONNECTED:
#ifdef SDIO_HM_TRACK_SESSION
            sdio_arb_network_timer_stop();
#endif /* SDIO_HM_TRACK_SESSION */
            nw->type = NW_EVENT_DISCONNECTED;
            nw->u.reason = event_data->reason;
            break;
        case CY_WCM_EVENT_IP_CHANGED:
            nw->type = NW_EVENT_IP_CHANGED;
            nw->u.ipv4 = event_data->ip_addr.ip.v4;
            break;
        default:
            PRINT_HM_DEBUG(("unsupported event %d\n", event));
            free(inf_event);
            return;
    }

    result = SDIO_HM_TX_EVENT(sdio_hm, inf_event, sizeof(*inf_event));
    if (result != SDIOD_STATUS_SUCCESS) {
        sdio_hm->tx_info->err_event++;
        PRINT_HM_ERROR(("tx event failed: 0x%lx\n", result));
        free(inf_event);
    }
}

static void sdio_hm_rx_notify_host()
{
    static uint32_t to_host_mailbox_data = 0;
    sdiod_status_t dev_status = SDIOD_STATUS_SUCCESS;

    dev_status = sdiod_set_ToHostMailboxData(to_host_mailbox_data);
    if (dev_status != SDIOD_STATUS_SUCCESS) {
        PRINT_HM_ERROR(("failed to write mailbox data 0x%lx: %d\n",
            to_host_mailbox_data, dev_status));
        return;
    }

    to_host_mailbox_data++;
}

static cy_rslt_t sdio_hm_request_buffer(sdio_handler_t sdio_hm, whd_buffer_t *buffer)
{
    whd_driver_t whd = sdio_hm->ifp->whd_driver;
    whd_result_t whd_result;

    whd_result = whd_host_buffer_get(whd, buffer, WHD_NETWORK_TX,
                                    (uint16_t) WHD_LINK_MTU, (uint32_t)0);
    if (whd_result != WHD_SUCCESS) {
        *buffer = NULL;
        PRINT_HM_DEBUG(("buffer get failed 0x%lx\n", whd_result));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    whd_result = whd_buffer_add_remove_at_front(whd, buffer,
                                             (int32_t)(WHD_LINK_HEADER));
    if (whd_result != WHD_SUCCESS) {
        *buffer = NULL;
        PRINT_HM_ERROR(("remove node failed 0x%lx\n", whd_result));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    return CY_RSLT_SUCCESS;
}

void sdio_hm_int_evt_info(sdio_handler_t sdio_hm, sdiod_event_data_host_info_t       *host_info)
{
    if (host_info->io_enabled && !(sdio_hm->sdio_instance.is_ready)) {
        PRINT_HM_DEBUG(("IO Enabled: %d\n", host_info->io_enabled));
        if(cyhal_sdio_dev_is_ready(&sdio_hm->sdio_instance)) {
            PRINT_HM_DEBUG(("F2 Ready done: %d\n", (sdio_hm->sdio_instance.is_ready)));
            cyhal_sdio_dev_read_async(&sdio_hm->sdio_instance, sdio_hm->sdio_instance.buffer.rx_header, SDIOD_RX_FRAME_HDR_LENGTH);
        }
    }
}

void sdio_hm_int_evt_cb(sdiod_event_code_t event_code, void *event_data)
{
    sdiod_event_data_t *sdiod_event_data = (sdiod_event_data_t*)event_data;

    switch (event_code)
    {
        case SDIOD_EVENT_CODE_HOST_INFO:
            sdio_hm_int_evt_info(sdio_hm, &sdiod_event_data->host_info);
            break;
        case SDIOD_EVENT_CODE_RX_DONE:
            sdio_hm_thread_trigger(sdio_hm, true);
            break;
        case SDIOD_EVENT_CODE_TX_DONE:
            /* Implement if needed */
            break;
        case SDIOD_EVENT_CODE_RX_ERROR:
            /* Implement if needed */
            PRINT_HM_ERROR(("SDIOD_EVENT_CODE_RX_ERROR\n"));
            break;
        case SDIOD_EVENT_CODE_TX_ERROR:
            /* Implement if needed */
            PRINT_HM_ERROR(("SDIOD_EVENT_CODE_TX_ERROR\n"));
            break;
        case SDIOD_EVENT_CODE_BUS_ERROR:
            PRINT_HM_ERROR(("SDIO Interrupt Error[%d] \n", event_code));
            break;
        default:
            break;
    }
}

static void sdio_hm_thread_timer_start(sdio_handler_t sdio_hm)
{
    bool timer_state;
    cy_rslt_t result;

    sdio_hm->sdio_rx_timer_active = true;
    result = cy_rtos_is_running_timer(&sdio_hm->thread_timer, &timer_state);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("SDIO watchdog timer is running: %ld\n", result));
        return;
    }

    if (timer_state)
        return;

    result = cy_rtos_timer_start(&sdio_hm->thread_timer, THREAD_TIME);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("SDIO start watchdog timer failed: %ld\n", result));
    }
}

static void sdio_hm_thread_timer_cb(cy_timer_callback_arg_t arg)
{
    sdio_handler_t sdio_hm = (sdio_handler_t)arg;
    cy_network_packet_pool_info_t txpool_info;

    cy_network_get_packet_pool_info(CY_NETWORK_PACKET_TX, &txpool_info);

    if (txpool_info.free_packets == 0) {
        sdio_hm_thread_timer_start(sdio_hm);
    } else {
        sdio_hm_thread_trigger(sdio_hm, false);
    }
    sdio_hm->sdio_rx_timer_active = false;
}

static void sdio_hm_thread_func(cy_thread_arg_t arg)
{
    sdio_handler_t sdio_hm = (sdio_handler_t)arg;
    sdio_rx_info_t rxi = sdio_hm->rx_info;
    cy_rslt_t result;

    do {
        cy_rtos_get_semaphore(&sdio_hm->thread_sema, CY_RTOS_NEVER_TIMEOUT, false);

        sdio_hm->sdio_thread_active = true;

        /* get whd buffer for next Rx data */
        if (!rxi->reserved_buffer) {
            result = sdio_hm_request_buffer(sdio_hm, &rxi->reserved_buffer);
            if (result == CY_RSLT_SUCCESS) {
                sdio_hm_rx_notify_host();
            }
        }

        /* Rx data from host */
        if (rxi->reserved_buffer)
            while (sdio_hm_data_from_host(sdio_hm) == CY_RSLT_SUCCESS);

        /* Tx data to host */
        while (sdio_hm_tx_from_q(sdio_hm) == CY_RSLT_SUCCESS);

        /* schedule timer if Rx pool isn't enough */
        if (!rxi->reserved_buffer)
            sdio_hm_thread_timer_start(sdio_hm);

        sdio_hm->sdio_thread_active = false;

#ifdef COMPONENT_CAT5
        tx_thread_relinquish();
#endif

    } while (true);
    /* Should never get here */
}

#if defined(COMPONENT_CAT5) && !defined(SDIOHM_DISABLE_PDS)
bool sdio_hm_is_busy(void)
{
    return (sdio_hm->sdio_thread_active | sdio_hm->sdio_rx_timer_active);
}

/* Deep Sleep Callback Service Routine for SDIO HM */
static bool sdio_hm_syspm_callback(cyhal_syspm_callback_state_t state, cyhal_syspm_callback_mode_t mode, void *callback_arg)
{
    bool allow = false; /* Default should be not allow */
    cyhal_sdio_t *sdio = (cyhal_sdio_t *)callback_arg;

    if (state == CYHAL_SYSPM_CB_CPU_DEEPSLEEP)
    {
        switch (mode)
        {
            case CYHAL_SYSPM_CHECK_READY:
            {
                if(!(sdio_hm_is_busy()))
                {
                    /* if device is not busy the sleepCSR is checked to decide if sleep is allowed */
                    sdio->pm_transition_pending = (SDIOD_STATUS_SUCCESS == sdiod_IsSleepAllowed()) ? true : false;
                }
                else
                {
                    sdio->pm_transition_pending = false;
                }
                /* pm_transition_pending is true: pending SDIO data trasition */
                allow = sdio->pm_transition_pending;
                break;
            }

            case CYHAL_SYSPM_BEFORE_TRANSITION:
            {
#ifdef SDIO_HM_TRACK_SESSION
                if (cy_wcm_is_connected_to_ap() == WHD_TRUE)
                {
                    sdio_arb_network_timer_stop();
                }
#endif /* SDIO_HM_TRACK_SESSION */
                (void)sdiod_preSleep();
                break;
            }

            case CYHAL_SYSPM_AFTER_TRANSITION:
            {
#ifdef SDIO_HM_TRACK_SESSION
                if (cy_wcm_is_connected_to_ap() == WHD_TRUE)
                {
                    sdio_arb_network_timer_start();
                }
#endif /* SDIO_HM_TRACK_SESSION */
                (void)sdiod_postSleep();
                /* allow SDIO data trasition */
                sdio->pm_transition_pending = false;
                break;
            }

            case CYHAL_SYSPM_CHECK_FAIL:
            {
                sdio->pm_transition_pending = false;
                break;
            }

            default:
                break;
        }
    }

    return allow;
}
#endif /* defined(COMPONENT_CAT5) && !defined(SDIOHM_DISABLE_PDS) */

static cy_rslt_t sdio_hm_configure(sdio_handler_t *sdio_hm)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    sdio_handler_t sdio_driver_tmp = NULL;

    sdio_driver_tmp = (sdio_handler_t)whd_mem_malloc(sizeof(struct sdio_handler));
    whd_mem_memset(sdio_driver_tmp, 0, sizeof(struct sdio_handler) );

    sdio_driver_tmp->_sdio_dma_desc = (sdiod_dma_descs_buf_t*)whd_dmapool_alloc(SDIO_F2_DMA_BUFFER_SIZE);

    if (sdio_driver_tmp->_sdio_dma_desc == NULL) {
        PRINT_HM_ERROR(("whd_dmapool_alloc failed: %ld\n", result));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    result = (SDIOD_STATUS_SUCCESS == sdiod_Init(sdio_driver_tmp->_sdio_dma_desc)) ? CY_RSLT_SUCCESS : CYHAL_SDIO_RSLT_ERR_CONFIG;
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("sdiod_Init failed: %ld\n", result));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    sdio_driver_tmp->sdio_instance.buffer.rx_header = (uint8_t*)sdio_driver_tmp->_sdio_dma_desc + sizeof(sdiod_dma_descs_buf_t);
    sdio_driver_tmp->sdio_instance.buffer.rx_payload = sdio_driver_tmp->sdio_instance.buffer.rx_header + sizeof(sdiod_f2_rx_frame_hdr_t);
    sdio_driver_tmp->sdio_instance.buffer.tx_payload = sdio_driver_tmp->sdio_instance.buffer.rx_payload + SDIO_F2_FRAME_MAX_PAYLOAD;

    sdio_driver_tmp->sdio_instance.hw_inited = true;
    sdio_driver_tmp->sdio_instance.is_ready = false;
    *sdio_hm = sdio_driver_tmp;

    result = (SDIOD_STATUS_SUCCESS == sdiod_RegisterCallback(sdio_hm_int_evt_cb)) ? CY_RSLT_SUCCESS : CYHAL_SDIO_RSLT_ERR_CONFIG;
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("sdiod_RegisterCallback failed: %ld\n", result));
        return CYHAL_SDIO_RSLT_ERR_UNSUPPORTED;
    }

    result = (SDIOD_STATUS_SUCCESS == sdiod_EnableInterrupt()) ? CY_RSLT_SUCCESS : CYHAL_SDIO_RSLT_ERR_CONFIG;
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("sdiod_EnableInterrupt failed: %ld\n", result));
        return CYHAL_SDIO_RSLT_ERR_UNSUPPORTED;
    }

#if defined(COMPONENT_CAT5) && !defined(SDIOHM_DISABLE_PDS)
    /* Enable SysPM callback for the SDIO code to decide on whether to sleep or not */
    sdio_driver_tmp->sdio_instance.pm_transition_pending = false;
    sdio_driver_tmp->sdio_instance.pm_callback_data.callback = &sdio_hm_syspm_callback;
    sdio_driver_tmp->sdio_instance.pm_callback_data.states = (cyhal_syspm_callback_state_t)(CYHAL_SYSPM_CB_CPU_DEEPSLEEP);
    sdio_driver_tmp->sdio_instance.pm_callback_data.next = NULL;
    sdio_driver_tmp->sdio_instance.pm_callback_data.args = (void *)&sdio_driver_tmp->sdio_instance;
    sdio_driver_tmp->sdio_instance.pm_callback_data.ignore_modes = (cyhal_syspm_callback_mode_t)0;
    cyhal_syspm_register_callback(&(sdio_driver_tmp->sdio_instance.pm_callback_data));
#endif /* defined(COMPONENT_CAT5) && !defined(SDIOHM_DISABLE_PDS) */

    return result;
}

static cy_rslt_t sdio_hm_init_tx(sdio_handler_t sdio_hm)
{
    whd_driver_t whd = sdio_hm->ifp->whd_driver;
    sdio_tx_info_t txi;

    txi = (sdio_tx_info_t)malloc(sizeof(*txi));
    if (!txi) {
        PRINT_HM_ERROR(("alloc tx_info failed\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    memset(txi, 0, sizeof(*txi));
    sdio_hm->tx_info = txi;

    CHK_RET(sdio_hm_q_init(txi));

    /* init snet for arbitration */
    sdio_arb_network_init();
    sdio_arb_add_whitelist_port(5001);

    whd->network_if->whd_network_process_ethernet_data = sdio_hm_process_ethernet_data;

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_init_rx(sdio_handler_t sdio_hm)
{
    sdio_rx_info_t rxi;

    rxi = (sdio_rx_info_t)malloc(sizeof(*rxi));
    if (!rxi) {
        PRINT_HM_ERROR(("alloc rx_info failed\n"));
        return CYHAL_SDIO_RSLT_ERR_CONFIG;
    }

    memset(rxi, 0, sizeof(*rxi));
    sdio_hm->rx_info = rxi;

    rxi->reserved_buffer = NULL;
    rxi->next_seq = 0;

    CHK_RET(sdio_hm_request_buffer(sdio_hm, &rxi->current_buffer));

    return CY_RSLT_SUCCESS;
}

cy_rslt_t sdio_hm_init(void)
{
    CHK_RET(sdio_hm_configure(&sdio_hm));

    CY_ASSERT(NULL != sdio_hm);

    sdio_hm->ifp = whd_ifs[CY_WCM_INTERFACE_TYPE_STA];

    /* init general function */
    CHK_RET(cy_rtos_init_semaphore(&sdio_hm->thread_sema, 1, 0));
    CHK_RET(cy_rtos_timer_init(&sdio_hm->thread_timer, CY_TIMER_TYPE_ONCE,
        sdio_hm_thread_timer_cb, (cy_timer_callback_arg_t)sdio_hm));

    /* init Tx/Rx function */
    CHK_RET(sdio_hm_init_tx(sdio_hm));
    CHK_RET(sdio_hm_init_rx(sdio_hm));

#if defined(SDIO_HM_TEST)
    rx_err_hdr = 0;
    rx_err_data = 0;

    CHK_RET(cy_rtos_timer_init(&tp_test_timer, CY_TIMER_TYPE_ONCE,
        sdio_hm_tp_timer_cb, (cy_timer_callback_arg_t)&sdio_hm->sdio_instance));
#endif /* defined(SDIO_HM_TEST) */

    /* init major thread */
    CHK_RET(cy_rtos_thread_create(&sdio_hm->thread, sdio_hm_thread_func, "SDIO task",
        (void *)sdio_stack, SDIO_STACK_SIZE, SDIO_TASK_PRIORITY, (cy_thread_arg_t)sdio_hm));

    sdio_cmd_init(sdio_hm);

    /* register network event change callback */
    CHK_RET(cy_wcm_register_event_callback(sdio_hm_wcm_event_callback));

    PRINT_HM_INFO(("SDIO for HM configured successfully\n"));

    return CY_RSLT_SUCCESS;
}

static void sdio_hm_fill_sw_header(uint8_t *payload, uint8_t channel)
{
    static uint8_t tx_seq = 0;
    sdio_sw_header_t *sw_hdr = (sdio_sw_header_t *)payload;
    cy_network_packet_pool_info_t txpool_info;

    cy_network_get_packet_pool_info(CY_NETWORK_PACKET_TX, &txpool_info);

    sw_hdr->sequence = tx_seq++;
    sw_hdr->channel_and_flags = channel;

    sw_hdr->bus_data_credit = (txpool_info.free_packets);
}

static cy_rslt_t sdio_hm_data_to_host(sdio_handler_t sdio_hm, uint8_t *data, uint16_t data_len)
{
    cy_rslt_t result;

    result = cyhal_sdio_dev_write_async(&sdio_hm->sdio_instance, data, data_len);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("cyhal_sdio_dev_write_async: %ld\n", result));
        return result;
    }

    result = cyhal_sdio_dev_mailbox_write(&sdio_hm->sdio_instance, 0x4, NULL);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("cyhal_sdio_dev_mailbox_write: %ld\n", result));
        return result;
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_build_pkt_for_host(sdio_handler_t sdio_hm, uint8_t *data, uint16_t data_len, uint8_t channel)
{
    cy_rslt_t result;

    sdio_hm_fill_sw_header(data, channel);

    result = sdio_hm_data_to_host(sdio_hm, data, data_len);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("Failed to send data to host: %ld\n", result));
        return CYHAL_SDIO_DEV_RSLT_WRITE_ERROR;
    }

    return 0;
}

static cy_rslt_t sdio_hm_tx_prepare(sdio_handler_t sdio_hm, uint8_t *data, uint16_t data_len, uint8_t channel)
{
    uint8_t *payload = sdio_hm->sdio_instance.buffer.tx_payload;

    if(data != NULL) {
        memcpy(payload + SDIO_SW_HEADER_LEN, data, data_len);
    } else {
        data_len = 0;
    }

    return sdio_hm_build_pkt_for_host(sdio_hm, payload, SDIO_SW_HEADER_LEN + data_len, channel);
}

static void sdio_hm_proc_data(sdio_handler_t sdio_hm, uint8_t *data, uint16_t data_len, whd_buffer_t next_whd_buffer)
{
    sdio_rx_info_t rxi = sdio_hm->rx_info;
    uint8_t* packet = NULL;
    int ret;

    CY_ASSERT(NULL != next_whd_buffer);

    packet = whd_buffer_get_current_piece_data_pointer(sdio_hm->ifp->whd_driver, rxi->current_buffer);

    memcpy(packet, data, data_len);

    /* create hash value */
    ret = sdio_arb_eth_packet_send_handle(data, (int *)&data_len);
    if (ret)
    {
        PRINT_HM_DEBUG(("invaild packet %d\n", ret));
    }

    /* forward data to WHD */
    whd_network_send_ethernet_data(sdio_hm->ifp, rxi->current_buffer);

    rxi->current_buffer = next_whd_buffer;
}

static void sdio_hm_rx_process(sdio_handler_t sdio_hm, uint8_t *payload, uint16_t length)
{
    sdio_rx_info_t rxi = sdio_hm->rx_info;
    uint32_t *sw_hdr = (uint32_t *)payload;
    uint8_t seq ,channel, data_offset;
    uint8_t *data;
    uint16_t data_len;
    whd_buffer_t next_whd_buffer = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    data = payload + SDIO_SW_HEADER_LEN;
    data_len = length - SDIO_SW_HEADER_LEN;

    seq = *sw_hdr & SDPCM_SEQ_MASK;
    channel = (*sw_hdr & SDPCM_CHANNEL_MASK) >> SDPCM_CHANNEL_SHIFT;

    /* used for network data only */
    data_offset = (*sw_hdr & SDPCM_DATA_OFFSET_MASK) >> SDPCM_DATA_OFFSET_SHIFT;

    /* compare sequence number */
    PRINT_HM_DEBUG(("Rx seq: 0x%lx, channel: %d\n", seq, channel));

    if (seq != rxi->next_seq) {
        PRINT_HM_ERROR(("received seq %d != expected seq %d\n", seq, rxi->next_seq));
        rxi->next_seq = seq + 1;
    } else {
        rxi->next_seq++;
    }

    /* request whd buffer if data need to forward to WHD */
    if (channel == SDPCM_DATA_CHANNEL) {
        next_whd_buffer = rxi->reserved_buffer;
        rxi->reserved_buffer = NULL;
        result = sdio_hm_request_buffer(sdio_hm, &rxi->reserved_buffer);
    }

    /* notify host to transmit next data before processing received data */
    if (result == CY_RSLT_SUCCESS) {
        sdio_hm_rx_notify_host();
    }

    /* process received data */
    switch (channel) {
    case SDPCM_CONTROL_CHANNEL:
        sdio_cmd_data_enq(sdio_hm->sdio_cmd, data, data_len);
        break;
    case SDPCM_DATA_CHANNEL:
        sdio_hm_proc_data(sdio_hm, data + data_offset, data_len - data_offset, next_whd_buffer);
        break;
#if defined(SDIO_HM_TEST)
    case SDPCM_TEST_CMD_CHANNEL:
        _sdio_proc_test_cmd((struct ifx_test_msg_t *)data, data_len);
        break;
    case SDPCM_TEST_DATA_CHANNEL:
        /* compare result will affect performance significantly */
        if (tx_tp_test) {
            tp_test_ok++;
            tp_test_cnt += data_len;
        }
        break;
    case SDPCM_LOOPBACK_CHANNEL:
        sdio_hm_build_pkt_for_host(sdio_hm, payload, length, SDPCM_TEST_DATA_CHANNEL);
        break;
#endif /* defined(SDIO_HM_TEST) */
    default:
        break;
    }
}

static cy_rslt_t sdio_hm_data_from_host(sdio_handler_t sdio_hm)
{
    sdio_rx_info_t rxi = sdio_hm->rx_info;
    uint8_t *rx_header = sdio_hm->sdio_instance.buffer.rx_header;
    uint8_t *rx_payload = sdio_hm->sdio_instance.buffer.rx_payload;
    uint16_t length;
    cy_rslt_t result;

    if (sdiod_get_RxStatus() != SDIOD_STATUS_SUCCESS)
        return CYHAL_SDIO_DEV_RSLT_READ_ERROR;

    /* get data length from HW header*/
    length = SDIOD_RX_GET_FRAME_PAYLOAD_LENGTH(rx_header);

    if (length == 0) {
        rxi->err_len++;
        PRINT_HM_ERROR(("Error : Incoming length is 0\n"));
        return CYHAL_SDIO_DEV_RSLT_READ_ERROR;
    }

    /* get data */
    result = cyhal_sdio_dev_read_async(&sdio_hm->sdio_instance, rx_payload, length);
    if (result != CY_RSLT_SUCCESS) {
        rxi->err_data++;
        PRINT_HM_ERROR(("read payload failed: %ld\n", result));
        return CYHAL_SDIO_DEV_RSLT_READ_ERROR;
    }

    while (sdiod_get_RxStatus() != SDIOD_STATUS_SUCCESS) {};

    /* prepare buffer to receive next HW header */
    result = cyhal_sdio_dev_read_async(&sdio_hm->sdio_instance, rx_header, SDIOD_RX_FRAME_HDR_LENGTH);
    if (result != CY_RSLT_SUCCESS) {
        rxi->err_hdr++;
        PRINT_HM_ERROR(("read header failed: %ld\n", result));
        return CYHAL_SDIO_DEV_RSLT_READ_ERROR;
    }

    sdio_hm_rx_process(sdio_hm, rx_payload, length);

    return CY_RSLT_SUCCESS;
}

cy_rslt_t sdio_hm_tx_buf_enq(sdio_handler_t sdio_hm, bool is_whd, uint8_t channel, void *buffer, uint16_t length)
{
    sdio_tx_info_t txi = sdio_hm->tx_info;
    sdio_tx_q_node_t q_node = NULL;
    cy_rslt_t result;

    /* put buffer into Tx wait queue */
    result = sdio_hm_node_deq(txi, &txi->tx_idle_q, &q_node);
    if (result != CY_RSLT_SUCCESS)
        return result;

    CY_ASSERT(NULL != q_node);

    q_node->is_whd_buf = is_whd;
    q_node->channel = channel;
    q_node->buf_ptr = buffer;
    q_node->buf_len = length;

    result = sdio_hm_node_enq(txi, &txi->tx_wait_q, q_node);
    if (result != CY_RSLT_SUCCESS)
        return result;

    sdio_hm_thread_trigger(sdio_hm, false);

    return result;
}

sdio_handler_t sdio_hm_get_sdio_handler()
{
    return sdio_hm;
}

#if defined(SDIO_HM_TEST)
static void sdio_hm_tp_start(void)
{
    tp_test = true;
    tp_test_ok = 0;
    tp_test_err = 0;
    tp_test_cnt = 0;
}

static void sdio_hm_tp_stop(void)
{
    tp_test = false;
}

static void sdio_hm_tp_result(void)
{
    PRINT_HM_INFO(("TIME %2d s, cnt ok %ld/ err %ld, %ld bytes, %ld Kbps\n",
        TEST_TOTAL_TIME / 1000, tp_test_ok, tp_test_err, tp_test_cnt,
        (tp_test_cnt >> 7) / (TEST_TOTAL_TIME / 1000)));
    PRINT_HM_INFO(("Rx error, cnt hdr %ld/ data %ld\n",
        rx_err_hdr, rx_err_data));
}

static void sdio_hm_tp_timer_cb(cy_timer_callback_arg_t arg)
{
    sdio_hm_tp_stop();
}

void sdio_hm_tx_at_cmd_msg(uint8_t *data, uint16_t data_len)
{
    sdio_hm_tx_prepare(sdio_hm, data, data_len, SDPCM_AT_CMD_CHANNEL);
}

static void sdio_hm_process_at_cmd(at_cmd_msg_base_t *host_cmd, uint16_t length)
{
    at_cmd_msg_base_t *msg = NULL;
    at_cmd_ref_app_wcm_connect_specific_t *connect_config;
    at_cmd_ref_app_wcm_get_ip_type_t *get_ip_config;
    cy_rslt_t result;

    PRINT_HM_DEBUG(("AT CMD ID: %ld\n", host_cmd->cmd_id));

    /* copy from at_cmd_refapp_parse_wcm_cmd() */
    switch (host_cmd->cmd_id)
    {
        case CMD_ID_AP_DISCONNECT:
        case CMD_ID_SCAN_START:
        case CMD_ID_SCAN_STOP:
        case CMD_ID_AP_GET_INFO:
        case CMD_ID_GET_IPv4_ADDRESS:
        case CMD_ID_PING:
            /*
             * Commands with no arguments. We just need a basic config message structure.
             */
            msg = (at_cmd_msg_base_t *)malloc(sizeof(at_cmd_msg_base_t));
            if (msg == NULL) {
                PRINT_HM_ERROR(( "Error allocating WCM config message\n"));
                return;
            }
            memset(msg, 0, sizeof(at_cmd_msg_base_t));
            memcpy(msg, host_cmd, sizeof(at_cmd_msg_base_t));
            break;

        case CMD_ID_AP_CONNECT:
            connect_config = malloc(sizeof(at_cmd_ref_app_wcm_connect_specific_t));
            if (connect_config == NULL) {
                PRINT_HM_ERROR(( "error allocating WCM connect specific message\n"));
                return;
            }
            memset(connect_config, 0, sizeof(at_cmd_ref_app_wcm_connect_specific_t));
            memcpy(connect_config, host_cmd, sizeof(at_cmd_ref_app_wcm_connect_specific_t));
            msg = (at_cmd_msg_base_t *)connect_config;
            break;

        case CMD_ID_GET_IP_ADDRESS:
            get_ip_config = malloc(sizeof(at_cmd_ref_app_wcm_get_ip_type_t));
            if (get_ip_config == NULL) {
                PRINT_HM_ERROR(( "error allocating WCM get ip config message\n"));
                return;
            }
            memset(get_ip_config, 0, sizeof(at_cmd_ref_app_wcm_get_ip_type_t));
            memcpy(get_ip_config, host_cmd, sizeof(at_cmd_ref_app_wcm_get_ip_type_t));
            msg = (at_cmd_msg_base_t *)get_ip_config;
            break;

        default:
            AT_CMD_REFAPP_LOG_MSG(( "Unimplemented cmd \n"));
            return;
    }

    result = at_cmd_refapp_send_message(msg);
    if (result != CY_RSLT_SUCCESS) {
        PRINT_HM_ERROR(("at_cmd_refapp_send_message failed: %ld\n", result));
    }
}

static cy_rslt_t sdio_hm_tp_test_start(void)
{
    PRINT_HM_DEBUG(("Start SDIO HM TP test\n"));

    sdio_hm_tp_start();

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_tp_test_stop(void)
{
    PRINT_HM_DEBUG(("End SDIO HM TP test\n"));

    sdio_hm_tp_stop();
    sdio_hm_tp_result();

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_rx_inc_test(sdio_handler_t sdio_hm)
{
    uint8_t *payload = sdio_hm->sdio_instance.buffer.tx_payload;
    uint16_t tx_len = 0;
    uint16_t i;
    cy_rslt_t result;

    PRINT_HM_DEBUG(("Start RX INC Test\n"));

    sdio_hm_fill_sw_header(payload, SDPCM_DATA_CHANNEL);

    for (i = 0; i < SDIO_FRAME_MAX_PAYLOAD; i++)
        payload[i+SDIO_SW_HEADER_LEN] = (uint8_t)i;

    do {
        result = sdio_hm_data_to_host(sdio_hm, payload, tx_len+SDIO_SW_HEADER_LEN);
        if (result != CY_RSLT_SUCCESS)
            continue;
    } while (++tx_len <= SDIO_FRAME_MAX_PAYLOAD);

    PRINT_HM_DEBUG(("END RX INC Test\n"));

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t sdio_hm_rx_dec_test(sdio_handler_t sdio_hm)
{
    uint8_t *payload = sdio_hm->sdio_instance.buffer.tx_payload;
    uint16_t tx_len = SDIO_FRAME_MAX_PAYLOAD;
    uint16_t i;
    cy_rslt_t result;

    sdio_hm_fill_sw_header(payload, SDPCM_DATA_CHANNEL);

    for (i = 0; i < SDIO_FRAME_MAX_PAYLOAD; i++)
        payload[i+SDIO_SW_HEADER_LEN] = (uint8_t)i;

    do {
        result = sdio_hm_data_to_host(sdio_hm, payload, tx_len+SDIO_SW_HEADER_LEN);
        if (result != CY_RSLT_SUCCESS)
            continue;
    } while (tx_len-- > 0);

    return CY_RSLT_SUCCESS;
}

static void sdio_hm_rx_tp_test(sdio_handler_t sdio_hm)
{
    uint8_t *payload = sdio_hm->sdio_instance.buffer.tx_payload;
    uint16_t i;
    cy_rslt_t result;

    sdio_hm_fill_sw_header(payload, SDPCM_DATA_CHANNEL);

    for (i = 0; i < SDIO_FRAME_MAX_PAYLOAD; i++)
        payload[i+SDIO_SW_HEADER_LEN] = (uint8_t)i;

    sdio_hm_tp_test_start();
    cy_rtos_timer_start(&tp_test_timer, TEST_TOTAL_TIME);

    do {
        result = sdio_hm_data_to_host(sdio_hm, payload, SDIO_F2_FRAME_MAX_PAYLOAD);
        if (result == CY_RSLT_SUCCESS) {
            tp_test_ok++;
            tp_test_cnt += SDIO_FRAME_MAX_PAYLOAD;
        } else {
            tp_test_err++;
        }
    } while (tp_test);

    sdio_hm_tp_result();

    return;
}

static void sdio_hm_process_test_cmd(sdio_handler_t sdio_hm, struct inf_test_msg_t *msg, uint16_t length)
{
    if (length != sizeof(struct inf_test_msg_t)) {
        PRINT_HM_ERROR(("test cmd size mismatch %d != %d\n", length, sizeof(struct inf_test_msg_t)));
        return;
    }

    switch (msg->cmd) {
        case TEST_CMD_TP_START:
            sdio_hm_tp_test_start();
            break;
        case TEST_CMD_TP_STOP:
            sdio_hm_tp_test_stop();
            break;
        case TEST_CMD_RX_INC_START:
            sdio_hm_rx_inc_test(sdio_hm);
            break;
        case TEST_CMD_RX_DEC_START:
            sdio_hm_rx_dec_test(sdio_hm);
            break;
        case TEST_CMD_RX_TP_START:
            sdio_hm_rx_tp_test(sdio_hm);
            break;
        default:
            PRINT_HM_ERROR(("Test CMD not supported: %d\n", msg->cmd));
            break;
    }
}
#endif /* defined(SDIO_HM_TEST) */

#endif /* COMPONENT_SDIO_HM */
