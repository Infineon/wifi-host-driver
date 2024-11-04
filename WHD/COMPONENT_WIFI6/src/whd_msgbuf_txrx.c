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
#ifdef PROTO_MSGBUF

#include "whd_buffer_api.h"
#include "whd_msgbuf.h"
#include "whd_flowring.h"
#include "whd_proto.h"
#include "whd_ring.h"
#include "whd_network_if.h"
#include "whd_utils.h"
#include "bus_protocols/whd_bus_m2m_protocol.h"
#include "cy_network_mw_core.h"

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
#include "cyhal_syspm.h"
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

#define WHD_RX_BUF_TIMEOUT          (10)

#define ETHER_TYPE_BRCM           (0x886C)      /** Broadcom Ethertype for identifying event packets - Copied from DHD include/proto/ethernet.h */
#define BRCM_OUI            "\x00\x10\x18"      /** Broadcom OUI (Organizationally Unique Identifier): Used in the proprietary(221) IE (Information Element) in all Broadcom devices */
#define ALIGNED_ADDRESS            ( (uint32_t)0x3 )

/* QoS related definitions (type of service) */
#define IPV4_DSCP_OFFSET              (15)      /** Offset for finding the DSCP field in an IPv4 header */

static const uint8_t dscp_to_wmm_qos[] =
{ 0, 0, 0, 0, 0, 0, 0, 0,                                       /* 0  - 7 */
  1, 1, 1, 1, 1, 1, 1,                                          /* 8  - 14 */
  1, 1, 1, 1, 1, 1, 1,                                          /* 15 - 21 */
  1, 1, 0, 0, 0, 0, 0,                                          /* 22 - 28 */
  0, 0, 0, 5, 5, 5, 5,                                          /* 29 - 35 */
  5, 5, 5, 5, 5, 5, 5,                                          /* 36 - 42 */
  5, 5, 5, 5, 5, 7, 7,                                          /* 43 - 49 */
  7, 7, 7, 7, 7, 7, 7,                                          /* 50 - 56 */
  7, 7, 7, 7, 7, 7, 7,                                          /* 57 - 63 */
};

static void whd_msgbuf_update_rxbufpost_count(struct whd_msgbuf *msgbuf, uint16_t rxcnt);
static void whd_msgbuf_rxbuf_ioctlresp_post(struct whd_msgbuf *msgbuf);
static void whd_msgbuf_set_next_buffer_in_queue(whd_driver_t whd_driver, whd_buffer_t buffer, whd_buffer_t prev_buffer);
static void whd_msgbuf_rxbuf_event_post(struct whd_msgbuf *msgbuf);
static int whd_msgbuf_schedule_txdata(struct whd_msgbuf *msgbuf, uint32_t flowid, whd_bool_t force);


/** Map a DSCP value from an IP header to a WMM QoS priority
 *
 * @param dscp_val : DSCP value from IP header
 *
 * @return wmm_qos : WMM priority
 *
 */
static uint8_t whd_map_dscp_to_priority(whd_driver_t whd_driver, uint8_t val)
{
    uint8_t dscp_val = (uint8_t)(val >> 2); /* DSCP field is the high 6 bits of the second byte of an IPv4 header */

    return dscp_to_wmm_qos[dscp_val];
}

static void
whd_msgbuf_release_array(struct whd_driver *whd_driver,
                         struct whd_msgbuf_pktids *pktids, whd_buffer_dir_t direction)
{
    struct whd_msgbuf_pktid *array;
    struct whd_msgbuf_pktid *pktid;
    uint32_t result;
    uint32_t count;

    array = pktids->array;
    count = 0;
    do
    {
        if (array[count].allocated == 1)
        {
            pktid = &array[count];
            result = whd_buffer_release(whd_driver, pktid->skb, direction);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
        }
        count++;
    } while (count < pktids->array_size);

    whd_mem_free(array);
    whd_mem_free(pktids);
}

static void whd_msgbuf_release_pktids(struct whd_driver *whd_driver, struct whd_msgbuf *msgbuf)
{
    if (msgbuf->rx_pktids)
    {
        (void)cy_rtos_deinit_semaphore(&msgbuf->rx_pktids->pktid_mutex);
        whd_msgbuf_release_array(whd_driver, msgbuf->rx_pktids, WHD_NETWORK_RX);
    }
    if (msgbuf->tx_pktids)
    {
        (void)cy_rtos_deinit_semaphore(&msgbuf->tx_pktids->pktid_mutex);
        whd_msgbuf_release_array(whd_driver, msgbuf->tx_pktids, WHD_NETWORK_TX);
    }
}

static int
whd_msgbuf_alloc_pktid(struct whd_driver *whd_driver,
                       struct whd_msgbuf_pktids *pktids,
                       whd_buffer_t skb, uint16_t data_offset,
                       uint32_t *physaddr, uint32_t *idx)
{
    struct whd_msgbuf_pktid *array;
    uint32_t count;

    array = pktids->array;

    *physaddr = (uint32_t)(whd_buffer_get_current_piece_data_pointer(whd_driver, skb) + data_offset);
    *idx = pktids->last_allocated_idx;

    count = 0;
    /* Acquire mutex which prevents race condition on pktid->allocated */
    (void)cy_rtos_get_semaphore(&pktids->pktid_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    do
    {
        (*idx)++;
        if (*idx == pktids->array_size)
            *idx = 0;
        if (array[*idx].allocated == 0)
        {
            array[*idx].allocated = 1;
            break;
        }
        count++;
    } while (count < pktids->array_size);
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&pktids->pktid_mutex, WHD_FALSE);

    if (count == pktids->array_size)
        return WHD_WLAN_NOMEM;

    array[*idx].data_offset = data_offset;
    array[*idx].physaddr = *physaddr;
    array[*idx].skb = skb;

    pktids->last_allocated_idx = *idx;

    return WHD_SUCCESS;
}

static whd_buffer_t
whd_msgbuf_get_pktid(struct whd_driver *whd_driver, struct whd_msgbuf_pktids *pktids,
                     uint32_t idx)
{
    struct whd_msgbuf_pktid *pktid;
    whd_buffer_t skb;

    if (idx >= pktids->array_size)
    {
        WPRINT_WHD_ERROR( ("Invalid packet id %u (max %u)\n", (unsigned int)idx,
                           (unsigned int)pktids->array_size) );
        return NULL;
    }

    /* Acquire mutex which prevents race condition on pktid->allocated */
    (void)cy_rtos_get_semaphore(&pktids->pktid_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    if (pktids->array[idx].allocated == 1)
    {
        pktid = &pktids->array[idx];
        skb = pktid->skb;
        pktid->allocated = 0;
        /* Ignore return - not much can be done about failure */
        (void)cy_rtos_set_semaphore(&pktids->pktid_mutex, WHD_FALSE);
        return skb;
    }
    else
    {
        WPRINT_WHD_ERROR( ("Invalid packet id %u (not in use)\n", (unsigned int)idx) );
    }
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&pktids->pktid_mutex, WHD_FALSE);

    return NULL;
}

/** A helper function to easily acquire and initialise a buffer destined for use as an iovar
 *
 * @param  buffer      : A pointer to a whd_buffer_t object where the created buffer will be stored
 * @param  data_length : The length of space reserved for user data
 * @param  name        : The name of the iovar
 *
 * @return A pointer to the start of user data with data_length space available
 */
static void *whd_msgbuf_get_iovar_buffer(whd_driver_t whd_driver,
                                         whd_buffer_t *buffer,
                                         uint16_t data_length,
                                         const char *name)
{
    uint32_t name_length = (uint32_t)strlen(name) + 1;    /* + 1 for terminating null */

    if (whd_host_buffer_get(whd_driver, buffer, WHD_NETWORK_TX,
                            (uint16_t)(data_length + name_length),
                            (uint32_t)WHD_IOCTL_PACKET_TIMEOUT) == WHD_SUCCESS)
    {
        uint8_t *data = whd_buffer_get_current_piece_data_pointer(whd_driver, *buffer);
        CHECK_PACKET_NULL(data, NULL);
        whd_mem_memset(data, 0, (name_length + data_length));
        whd_mem_memcpy(data, name, name_length);
        return (data + name_length);
    }
    else
    {
        WPRINT_WHD_ERROR( ("Error - failed to allocate a packet buffer for IOVAR\n") );
        return NULL;
    }
}

/** A helper function to easily acquire and initialise a buffer destined for use as an ioctl
 *
 * @param  buffer      : A pointer to a whd_buffer_t object where the created buffer will be stored
 * @param  data_length : The length of space reserved for user data
 *
 * @return A pointer to the start of user data with data_length space available
 */
static void *whd_msgbuf_get_ioctl_buffer(whd_driver_t whd_driver,
                                         whd_buffer_t *buffer,
                                         uint16_t data_length)
{

    if (whd_host_buffer_get(whd_driver, buffer, WHD_NETWORK_TX, (uint16_t)(data_length),
                            (uint32_t)WHD_IOCTL_PACKET_TIMEOUT) == WHD_SUCCESS)
    {
        return (whd_buffer_get_current_piece_data_pointer(whd_driver, *buffer) );
    }
    else
    {
        WPRINT_WHD_ERROR( ("Error - failed to allocate a packet buffer for IOCTL\n") );
        return NULL;
    }

}

whd_result_t whd_msgbuf_send_mbdata(struct whd_driver *drvr, uint32_t mbdata)
{
    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)drvr->msgbuf;
    struct whd_commonring *commonring;
    struct msgbuf_h2d_mbdata *h2d_mbdata;
    void *ret_ptr;
    int err;

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_CONTROL_SUBMIT];
    whd_commonring_lock(commonring);

    ret_ptr = whd_commonring_reserve_for_write(commonring);
    if (!ret_ptr)
    {
        WPRINT_WHD_ERROR( ("Memory Allocation Space at Common Ring Failed \n") );
        whd_commonring_unlock(commonring);
        return WHD_UNFINISHED;
    }

    h2d_mbdata = (struct msgbuf_h2d_mbdata *)ret_ptr;
    whd_mem_memset(h2d_mbdata, 0, sizeof(*h2d_mbdata) );

    h2d_mbdata->msg.msgtype = MSGBUF_TYPE_H2D_MAILBOX_DATA;
    h2d_mbdata->mbdata = htod32(mbdata);

    err = whd_commonring_write_complete(commonring);
    whd_commonring_unlock(commonring);

    return err;
}

whd_result_t whd_msgbuf_ioctl_dequeue(struct whd_driver *whd_driver)
{
    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)whd_driver->msgbuf;
    struct whd_commonring *commonring;
    struct msgbuf_ioctl_req_hdr *request;
    uint32_t buf_len = 0, data_length = 0;
    void *ret_ptr;
    int retval;
    uint8_t *ioctl_queue_buf = NULL;

    /* Get the data length */
    data_length = (uint32_t)(whd_buffer_get_current_piece_size(whd_driver, msgbuf->ioctl_queue) );

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_CONTROL_SUBMIT];
    whd_commonring_lock(commonring);
    ret_ptr = whd_commonring_reserve_for_write(commonring);

    if (!ret_ptr)
    {
        WPRINT_WHD_ERROR( ("Failed to reserve space in commonring\n") );
        whd_commonring_unlock(commonring);
        CHECK_RETURN(whd_buffer_release(whd_driver, msgbuf->ioctl_queue, WHD_NETWORK_TX) );
        msgbuf->ioctl_queue = NULL;
        return WHD_UNFINISHED;
    }

    msgbuf->reqid++;
    WPRINT_WHD_DEBUG( ("Interface Index is %d ret_ptr is 0x%lx cmd: %d \n", msgbuf->ifidx, (uint32_t)ret_ptr,
                       (unsigned int)msgbuf->ioctl_cmd) );
    request = (struct msgbuf_ioctl_req_hdr *)ret_ptr;
    request->msg.msgtype = MSGBUF_TYPE_IOCTLPTR_REQ;
    request->msg.ifidx = (uint8_t)msgbuf->ifidx;
    request->msg.flags = 0;
    request->msg.request_ptr = htod32(WHD_IOCTL_REQ_PKTID);
    request->cmd = htod32(msgbuf->ioctl_cmd);
    request->output_buf_len = htod16(data_length);
    request->trans_id = htod16(msgbuf->reqid);
    buf_len = MIN(data_length, WHD_MSGBUF_IOCTL_MAX_TX_SIZE);
    request->input_buf_len = htod16(buf_len);
    request->req_buf_addr.high_addr = htod32(msgbuf->ioctbuf_phys_hi);
    request->req_buf_addr.low_addr = htod32(msgbuf->ioctbuf_phys_lo);

    if (msgbuf->ioctl_queue)
    {
        ioctl_queue_buf = whd_buffer_get_current_piece_data_pointer(whd_driver, (whd_buffer_t)msgbuf->ioctl_queue);
        whd_mem_memcpy(msgbuf->ioctbuf, ioctl_queue_buf, buf_len);
        CHECK_RETURN(whd_buffer_release(whd_driver, msgbuf->ioctl_queue, WHD_NETWORK_TX) );
        msgbuf->ioctl_queue = NULL;
    }
    else
    {
        whd_mem_memset(msgbuf->ioctbuf, 0, buf_len);
    }

    retval = whd_commonring_write_complete(commonring);

    whd_commonring_unlock(commonring);

    return retval;
}

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
static bool ioctl_lock_taken = WHD_FALSE;

static void whd_msgbuf_ioctl_entry(void)
{
    if (ioctl_lock_taken == WHD_FALSE)
    {
        cyhal_syspm_lock_deepsleep();
        ioctl_lock_taken = WHD_TRUE;
    }
}
static void whd_msgbuf_ioctl_exit(void)
{
    if (ioctl_lock_taken == WHD_TRUE)
    {
        cyhal_syspm_unlock_deepsleep();
        ioctl_lock_taken = WHD_FALSE;
    }
}
#else
static void whd_msgbuf_ioctl_entry(void)
{
    return;
}
static void whd_msgbuf_ioctl_exit(void)
{
    return;
}
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

static int whd_msgbuf_send_ioctl(whd_interface_t ifp, uint32_t cmd, whd_buffer_t send_buffer_hnd,
                                 whd_buffer_t *response_buffer_hnd)
{

    whd_msgbuf_ioctl_entry();

    whd_driver_t whd_driver = ifp->whd_driver;
    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)whd_driver->msgbuf;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    int retval;
    uint32_t retry = 0;

    /* Set send IOCTL buffer to ioctl_queue.
       Because we have ioctl_mutex, only one IOCTL buffer at one time. */
    msgbuf->ioctl_queue = send_buffer_hnd;
    msgbuf->ifidx = ifp->ifidx;
    msgbuf->ioctl_cmd = cmd;

    /* Make sure FW has atleast one IOCTL RX buffer to post the response */
    if (msgbuf->cur_ioctlrespbuf == 0)
    {
        whd_msgbuf_rxbuf_ioctlresp_post(msgbuf);
    }

    do
    {
        whd_thread_notify(whd_driver);

        /* Wait till response has been received  */
        retval = cy_rtos_get_semaphore(&msgbuf_info->ioctl_sleep, (uint32_t)WHD_IOCTL_TIMEOUT_MS, WHD_FALSE);
        if (retval != WHD_SUCCESS)
        {
            retry++;
            /* This is to force read the commonring for any index update, in case, interrupt got missed */
            /* WHD is doing the retry beacuse of interrupt fix observation - CYW55900A0-1020 */
            (void)whd_hw_generateBt2WlDbInterruptApi(0, 0x01);
            whd_driver->force_rx_read = WHD_TRUE;
        }
        else
        {
            whd_driver->force_rx_read = WHD_FALSE;
            break;
        }
    } while(retry < WHD_IOCTL_NO_OF_RETRIES);

    if ((retry == WHD_IOCTL_NO_OF_RETRIES) && (retval != WHD_SUCCESS))
    {
        whd_driver->force_rx_read = WHD_FALSE;
        whd_msgbuf_ioctl_exit();
        return retval;
    }
    msgbuf_info->ioctl_response =
        whd_msgbuf_get_pktid(whd_driver, msgbuf->rx_pktids, msgbuf_info->ioctl_response_pktid);

    if (msgbuf_info->ioctl_response_length != 0)
    {
        if (!msgbuf_info->ioctl_response)
        {
            whd_msgbuf_ioctl_exit();
            return WHD_BADARG;
        }
    }

    /* Check if the caller wants the response */
    if (response_buffer_hnd != NULL)
    {
        *response_buffer_hnd = msgbuf_info->ioctl_response;
    }
    else
    {
        (void)whd_buffer_release(whd_driver, msgbuf_info->ioctl_response, WHD_NETWORK_RX);
    }

    msgbuf_info->ioctl_response = NULL;

    /* Check whether the IOCTL response indicates it failed. */
    if (msgbuf_info->ioctl_response_status != WHD_SUCCESS)
    {
        if (response_buffer_hnd != NULL)
        {
            (void)whd_buffer_release(whd_driver, *response_buffer_hnd, WHD_NETWORK_RX);
            *response_buffer_hnd = NULL;
        }
        whd_msgbuf_ioctl_exit();
        return WHD_RESULT_CREATE( (WLAN_ENUM_OFFSET - msgbuf_info->ioctl_response_status) );
    }

    whd_msgbuf_ioctl_exit();
    return WHD_SUCCESS;
}

static whd_result_t whd_msgbuf_set_ioctl(whd_interface_t ifp, uint32_t command,
                                         whd_buffer_t send_buffer_hnd,
                                         whd_buffer_t *response_buffer_hnd)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    whd_result_t retval = WHD_SUCCESS;

    /* Acquire mutex which prevents multiple simultaneous IOCTLs */
    retval = cy_rtos_get_semaphore(&msgbuf_info->ioctl_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    if (retval != WHD_SUCCESS)
    {
        CHECK_RETURN(whd_buffer_release(whd_driver, send_buffer_hnd, WHD_NETWORK_TX) );
        return retval;
    }

    retval = whd_msgbuf_send_ioctl(ifp, command, send_buffer_hnd, response_buffer_hnd);

    /* Release the mutex since ioctl response will no longer be referenced. */
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE);

    return retval;
}

static whd_result_t whd_msgbuf_get_ioctl(whd_interface_t ifp, uint32_t command,
                                         whd_buffer_t send_buffer_hnd,
                                         whd_buffer_t *response_buffer_hnd)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    whd_result_t retval = WHD_SUCCESS;

    /* Acquire mutex which prevents multiple simultaneous IOCTLs */
    retval = cy_rtos_get_semaphore(&msgbuf_info->ioctl_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    if (retval != WHD_SUCCESS)
    {
        CHECK_RETURN(whd_buffer_release(whd_driver, send_buffer_hnd, WHD_NETWORK_TX) );
        return retval;
    }
    retval = whd_msgbuf_send_ioctl(ifp, command, send_buffer_hnd, response_buffer_hnd);

    /* Release the mutex since ioctl response will no longer be referenced. */
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE);

    return retval;
}

static whd_result_t whd_msgbuf_set_iovar(whd_interface_t ifp,
                                         whd_buffer_t send_buffer_hnd,
                                         whd_buffer_t *response_buffer_hnd)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    whd_result_t retval = WHD_SUCCESS;

    /* Acquire mutex which prevents multiple simultaneous IOCTLs */
    retval = cy_rtos_get_semaphore(&msgbuf_info->ioctl_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    if (retval != WHD_SUCCESS)
    {
        CHECK_RETURN(whd_buffer_release(whd_driver, send_buffer_hnd, WHD_NETWORK_TX) );
        return retval;
    }
    retval = whd_msgbuf_send_ioctl(ifp, (uint32_t)WLC_SET_VAR, send_buffer_hnd, response_buffer_hnd);

    /* Release the mutex since ioctl response will no longer be referenced. */
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE);

    return retval;
}

static whd_result_t whd_msgbuf_get_iovar(whd_interface_t ifp,
                                         whd_buffer_t send_buffer_hnd,
                                         whd_buffer_t *response_buffer_hnd)
{
    whd_driver_t whd_driver = ifp->whd_driver;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    whd_result_t retval = WHD_SUCCESS;

    /* Acquire mutex which prevents multiple simultaneous IOCTLs */
    retval = cy_rtos_get_semaphore(&msgbuf_info->ioctl_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    if (retval != WHD_SUCCESS)
    {
        CHECK_RETURN(whd_buffer_release(whd_driver, send_buffer_hnd, WHD_NETWORK_TX) );
        return retval;
    }
    retval = whd_msgbuf_send_ioctl(ifp, (uint32_t)WLC_GET_VAR, send_buffer_hnd, response_buffer_hnd);

    /* Release the mutex since ioctl response will no longer be referenced. */
    /* Ignore return - not much can be done about failure */
    (void)cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE);

    return retval;
}

static whd_result_t
whd_msgbuf_remove_flowring(struct whd_msgbuf *msgbuf, uint16_t flowid)
{

    WPRINT_WHD_DEBUG( ("Removing flowring %d\n", flowid) );
    /* TODO: Buffer release is not needed as it is from whd dmapool */
    //CHECK_RETURN(whd_buffer_release(msgbuf->drvr, (whd_buffer_t)msgbuf->flowring_handle[flowid], WHD_NETWORK_TX));

    whd_flowring_delete(msgbuf->flow, flowid);

    return WHD_SUCCESS;
}

static void whd_msgbuf_process_gen_status(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_gen_status *gen_status = buf;
    int err;

    err = dtoh16(gen_status->compl_hdr.status);
    if (err)
        WPRINT_WHD_ERROR( ("Firmware reported general error: %d\n", err) );
}

static void whd_msgbuf_process_ring_status(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_ring_status *ring_status = buf;
    int err;

    err = dtoh16(ring_status->compl_hdr.status);
    if (err)
    {
        WPRINT_WHD_ERROR( ("Firmware reported ring %d error: %d\n", dtoh16(ring_status->compl_hdr.flow_ring_id), err) );
    }
}

static void
whd_msgbuf_process_flow_ring_create_response(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_flowring_create_resp *flowring_create_resp;
    uint16_t status;
    uint16_t flowid;

    flowring_create_resp = (struct msgbuf_flowring_create_resp *)buf;

    flowid = dtoh16(flowring_create_resp->compl_hdr.flow_ring_id);
    flowid -= WHD_H2D_MSGRING_FLOWRING_IDSTART;
    status =  dtoh16(flowring_create_resp->compl_hdr.status);

    if (status)
    {
        WPRINT_WHD_ERROR( ("Flowring creation failed, code %d\n", status) );
        whd_msgbuf_remove_flowring(msgbuf, flowid);
        return;
    }
    WPRINT_WHD_DEBUG( ("Flowring %d Create response status %d\n", flowid, status) );

    whd_flowring_open(msgbuf->flow, flowid);
    msgbuf->current_flowring_count++;

    whd_msgbuf_schedule_txdata(msgbuf, flowid, WHD_TRUE);
}

static void
whd_msgbuf_process_flow_ring_delete_response(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_flowring_delete_resp *flowring_delete_resp;
    uint16_t status;
    uint16_t flowid;

    flowring_delete_resp = (struct msgbuf_flowring_delete_resp *)buf;

    flowid = dtoh16(flowring_delete_resp->compl_hdr.flow_ring_id);
    flowid -= WHD_H2D_MSGRING_FLOWRING_IDSTART;
    status =  dtoh16(flowring_delete_resp->compl_hdr.status);

    if (status)
    {
        WPRINT_WHD_ERROR( ("Flowring deletion failed, code %d\n", status) );
        whd_flowring_delete(msgbuf->flow, flowid);
        return;
    }
    WPRINT_WHD_DEBUG( ("Flowring %d Delete response status %d\n", flowid, status) );

    whd_msgbuf_remove_flowring(msgbuf, flowid);
    msgbuf->current_flowring_count--;

}

static void
whd_msgbuf_process_ioctl_complete(whd_driver_t whd_driver, struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_ioctl_resp_hdr *ioctl_resp;
    whd_result_t result;
    whd_result_t ioctl_mutex_res;
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;

    CHECK_PACKET_WITH_NULL_RETURN(buf);

    ioctl_resp = (struct msgbuf_ioctl_resp_hdr *)buf;

    /* Validate request ioctl ID and check if whd_bus_send_ioctl is still waiting for response*/
    if ( ( (ioctl_mutex_res = cy_rtos_get_semaphore(&msgbuf_info->ioctl_mutex, 0, WHD_FALSE) ) != WHD_SUCCESS ) )
    {
        /* Save the response packet in a variable */
        msgbuf_info->ioctl_response_pktid = dtoh32(ioctl_resp->msg.request_ptr);
        msgbuf_info->ioctl_response_length = dtoh16(ioctl_resp->resp_len);
        msgbuf_info->ioctl_response_status = dtoh16(ioctl_resp->compl_hdr.status);

        WPRINT_WHD_DATA_LOG( ("Wcd:< Procd pkt 0x%08lX: IOCTL Response\n",
                              (unsigned long)ioctl_resp) );

        if(msgbuf->reqid == ioctl_resp->trans_id)
        {
            /* Wake the thread which sent the IOCTL/IOVAR so that it will resume */
            result = cy_rtos_set_semaphore(&msgbuf_info->ioctl_sleep, WHD_FALSE);
            if (result != WHD_SUCCESS)
                 WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        }
        else
        {
            /* If we got the response for different IOCTL sent earlier,
               free the buffer to avoid buffer pool memory leak */
            msgbuf_info->ioctl_response =
                    whd_msgbuf_get_pktid(whd_driver, msgbuf->rx_pktids, msgbuf_info->ioctl_response_pktid);

            if(msgbuf_info->ioctl_response != NULL)
            {
                   result = whd_buffer_release(whd_driver, msgbuf_info->ioctl_response, WHD_NETWORK_RX);
                   if (result != WHD_SUCCESS)
                   {
                         WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
                         return;
                   }
            }
        }
    }
    else
    {
        if (ioctl_mutex_res == WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("whd_bus_send_ioctl is already timed out, drop the buffer\n") );
            result = cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE);
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
            }
        }
        else
        {
            WPRINT_WHD_ERROR( ("Received a response for a different IOCTL - retry\n") );
        }

        /* Save the response packet in a variable */
        msgbuf_info->ioctl_response_pktid = dtoh32(ioctl_resp->msg.request_ptr);
        msgbuf_info->ioctl_response_length = dtoh16(ioctl_resp->resp_len);
        msgbuf_info->ioctl_response_status = dtoh16(ioctl_resp->compl_hdr.status);
        msgbuf_info->ioctl_response = whd_msgbuf_get_pktid(whd_driver, msgbuf->rx_pktids, msgbuf_info->ioctl_response_pktid);

        if(msgbuf_info->ioctl_response != NULL)
        {
            result = whd_buffer_release(whd_driver, msgbuf_info->ioctl_response, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
                return;
            }
        }
    }

    if (msgbuf->cur_ioctlrespbuf)
        msgbuf->cur_ioctlrespbuf--;

    whd_msgbuf_rxbuf_ioctlresp_post(msgbuf);
}

static void whd_msgbuf_process_event_buffer(whd_driver_t whd_driver, whd_buffer_t buffer, uint16_t size)
{
    uint16_t ether_type;
    whd_event_header_t *whd_event;
    whd_event_t *event, *aligned_event = (whd_event_t *)whd_driver->aligned_addr;
    struct whd_msgbuf_info *msgbuf_info = whd_driver->proto->pd;
    whd_result_t result;
    uint16_t i;
    uint16_t j;
    uint32_t datalen, addr;


    if ( ( (uint32_t)whd_buffer_get_current_piece_size(whd_driver,
                                                       buffer) + WHD_ETHERNET_SIZE ) < sizeof(whd_event_t) )
        return;

    event = (whd_event_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, buffer);
    CHECK_PACKET_WITH_NULL_RETURN(event);

    ether_type = ntoh16(event->eth.ethertype);

    /* If frame is truly an event, it should have EtherType equal to the Broadcom type. */
    if (ether_type != (uint16_t)ETHER_TYPE_BRCM)
    {
        WPRINT_WHD_ERROR( ("Error - received a channel 1 packet which was not BRCM ethertype\n") );
        return;
    }

    /* If ethertype is correct, the contents of the ethernet packet
     * are a structure of type bcm_event_t
     */

    /* Check that the OUI matches the Broadcom OUI */
    if (0 != memcmp(BRCM_OUI, &event->eth_evt_hdr.oui[0], (size_t)DOT11_OUI_LEN) )
    {
        WPRINT_WHD_ERROR( ("Event OUI mismatch\n") );
        return;
    }

    whd_event = &event->whd_event;

    /* Search for the event type in the list of event handler functions
     * event data is stored in network endianness
     */
    whd_event->flags      =                        ntoh16(whd_event->flags);
    whd_event->event_type = (whd_event_num_t)ntoh32(whd_event->event_type);
    whd_event->status     = (whd_event_status_t)ntoh32(whd_event->status);
    whd_event->reason     = (whd_event_reason_t)ntoh32(whd_event->reason);
    whd_event->auth_type  =                        ntoh32(whd_event->auth_type);
    whd_event->datalen    =                        ntoh32(whd_event->datalen);

    /* Ensure data length is correct */
    if (whd_event->datalen + (sizeof(whd_event_t) ) >= (uint32_t)(size) )
    {
        WPRINT_WHD_ERROR( (
                              "Error - (data length received [%d], sizeof(whd_event_t)[%u]. Bus header packet size = [%d]. Ignoring the packet\n",
                              (int)whd_event->datalen, sizeof(whd_event_t), size) );
        return;
    }

    if (whd_event->bsscfgidx >= WHD_INTERFACE_MAX)
    {
        WPRINT_WHD_ERROR( ("bsscfgidx is out of range\n") );
        return;
    }

    /* This is necessary because people who defined event statuses and reasons overlapped values. */
    if (whd_event->event_type == WLC_E_PSK_SUP)
    {
        whd_event->status = (whd_event_status_t)( (int)whd_event->status + WLC_SUP_STATUS_OFFSET );
        whd_event->reason = (whd_event_reason_t)( (int)whd_event->reason + WLC_E_SUP_REASON_OFFSET );
    }
    else if (whd_event->event_type == WLC_E_PRUNE)
    {
        whd_event->reason = (whd_event_reason_t)( (int)whd_event->reason + WLC_E_PRUNE_REASON_OFFSET );
    }
    else if ( (whd_event->event_type == WLC_E_DISASSOC) || (whd_event->event_type == WLC_E_DEAUTH) )
    {
        whd_event->status = (whd_event_status_t)( (int)whd_event->status + WLC_DOT11_SC_STATUS_OFFSET );
        whd_event->reason = (whd_event_reason_t)( (int)whd_event->reason + WLC_E_DOT11_RC_REASON_OFFSET );
    }

    /* do any needed debug logging of event */
    WHD_IOCTL_LOG_ADD_EVENT(whd_driver, whd_event->event_type, whd_event->status,
                            whd_event->reason);

    if (cy_rtos_get_semaphore(&msgbuf_info->event_list_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Failed to obtain mutex for event list access!\n") );
        return;
    }

    datalen = whd_event->datalen;
    /* use whd_mem_memcpy to get aligned event message */
    addr = (uint32_t )DATA_AFTER_HEADER(event);
    if (aligned_event && (addr & ALIGNED_ADDRESS) )
    {
        whd_mem_memcpy(aligned_event, (whd_event_t *)addr, datalen);
    }
    else
    {
        aligned_event = (whd_event_t *)addr;
    }
    for (i = 0; i < (uint16_t)WHD_EVENT_HANDLER_LIST_SIZE; i++)
    {
        if (msgbuf_info->whd_event_list[i].event_set)
        {
            for (j = 0; msgbuf_info->whd_event_list[i].events[j] != WLC_E_NONE; ++j)
            {
                if ( (msgbuf_info->whd_event_list[i].events[j] == whd_event->event_type) &&
                     (msgbuf_info->whd_event_list[i].ifidx == whd_event->ifidx) )
                {
                    /* Correct event type has been found - call the handler function and exit loop */
                    msgbuf_info->whd_event_list[i].handler_user_data =
                        msgbuf_info->whd_event_list[i].handler(whd_driver->iflist[whd_event->bsscfgidx],
                                                               whd_event,
                                                               (uint8_t *)aligned_event,
                                                               msgbuf_info->whd_event_list[i].handler_user_data);
                    break;
                }
            }
        }
    }

    result = cy_rtos_set_semaphore(&msgbuf_info->event_list_mutex, WHD_FALSE);
    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );

    WPRINT_WHD_DATA_LOG( ("Wcd:< Procd pkt 0x%08lX: Evnt %d (%d bytes)\n", (unsigned long)buffer,
                          (int)whd_event->event_type, size) );
}

static void
whd_msgbuf_process_event(struct whd_msgbuf *msgbuf, void *buf)
{
    struct whd_driver *drvr = msgbuf->drvr;
    struct msgbuf_rx_event *event;
    uint32_t idx;
    uint16_t buflen;
    whd_buffer_t skb = NULL;
    whd_interface_t ifp;
    whd_result_t result;

    event = (struct msgbuf_rx_event *)buf;
    idx = dtoh32(event->msg.request_ptr);
    buflen = dtoh16(event->event_data_len);

    skb = whd_msgbuf_get_pktid(drvr, msgbuf->rx_pktids, idx);
    if (!skb)
        return;

    if (msgbuf->rx_dataoffset)
        (void)whd_buffer_add_remove_at_front(drvr, &skb, msgbuf->rx_dataoffset);

    (void)whd_buffer_set_size(drvr, skb, buflen);

    ifp = whd_get_interface(msgbuf->drvr, event->msg.ifidx);
    if (!ifp)
    {
        WPRINT_WHD_ERROR( ("Received pkt for invalid ifidx %d\n",
                           event->msg.ifidx) );
        goto exit;
    }

    whd_msgbuf_process_event_buffer(drvr, skb, buflen);

exit:
    /* Release the event packet buffer */
    result = whd_buffer_release(drvr, skb, WHD_NETWORK_RX);
    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );

    if (msgbuf->cur_eventbuf)
        msgbuf->cur_eventbuf--;
    whd_msgbuf_rxbuf_event_post(msgbuf);

}

static void
whd_msgbuf_process_txstatus(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_tx_status *tx_status;
    uint16_t flowid;
    uint32_t idx;
    whd_buffer_t skb = NULL;
    struct whd_driver *drvr = msgbuf->drvr;
    whd_result_t result;

    tx_status = (struct msgbuf_tx_status *)buf;
    flowid = dtoh16(tx_status->compl_hdr.flow_ring_id);
    flowid -= WHD_H2D_MSGRING_FLOWRING_IDSTART;
    idx = dtoh32(tx_status->msg.request_ptr) - 1;
    skb = whd_msgbuf_get_pktid(drvr, msgbuf->tx_pktids, idx);
    WPRINT_WHD_DEBUG( ("%s - tx_status - %d for FlowID is %d, skb:0x%lu\n", __func__, tx_status->compl_hdr.status,
                       flowid, (uint32_t)skb) );

    result = whd_buffer_release(drvr, skb, WHD_NETWORK_TX);
    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );

    return;
}

static void
whd_msgbuf_process_rx_complete(struct whd_msgbuf *msgbuf, void *buf)
{
    WPRINT_WHD_DEBUG( ("%s: RECEIVED RX PACKETS \n", __func__) );

    struct whd_driver *drvr = msgbuf->drvr;
    struct msgbuf_rx_complete *rx_complete;
    whd_interface_t ifp;
    whd_result_t result;
    whd_buffer_t skb = NULL;
    uint16_t data_offset;
    uint16_t buflen;
    uint16_t flags;
    uint32_t idx;

    WPRINT_WHD_DEBUG( ("%s : buf is 0x%lx \n", __func__, (uint32_t)buf) );

    if (buf != NULL)
    {
        rx_complete = (struct msgbuf_rx_complete *)buf;
        data_offset = dtoh16(rx_complete->data_offset);
        buflen = dtoh16(rx_complete->data_len);
        idx = dtoh32(rx_complete->msg.request_ptr);
        flags = dtoh16(rx_complete->flags);

        skb = whd_msgbuf_get_pktid(drvr, msgbuf->rx_pktids, idx);
        if (!skb)
            return;

        if (data_offset)
            (void)whd_buffer_add_remove_at_front(drvr, &skb, data_offset);
        else if (msgbuf->rx_dataoffset)
            (void)whd_buffer_add_remove_at_front(drvr, &skb, msgbuf->rx_dataoffset);

        (void)whd_buffer_set_size(drvr, skb, buflen);

        WPRINT_WHD_DEBUG( ("%s : buflen is %d , skb is 0x%lx\n", __func__, buflen, (uint32_t)skb) );

        if ( (flags & WHD_MSGBUF_PKT_FLAGS_FRAME_MASK) ==
             WHD_MSGBUF_PKT_FLAGS_FRAME_802_11 )
        {
            result = whd_buffer_release(drvr, skb, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
        }

        ifp = (whd_interface_t)whd_get_interface(drvr, rx_complete->msg.ifidx);
        if (!ifp)
        {
            WPRINT_WHD_ERROR( ("Received pkt for invalid ifidx %d\n",
                               rx_complete->msg.ifidx) );
            result = whd_buffer_release(drvr, skb, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
            return;
        }

        /* Send packet to bottom of network stack */
        result = whd_network_process_ethernet_data(ifp, skb);
        if (result != WHD_SUCCESS)
            WPRINT_WHD_ERROR( ("%s failed at %d \n", __func__, __LINE__) );
    }
    else
        WPRINT_WHD_DEBUG( ("%s : RECEIVED BUFFER IS NULL \n", __func__) );

    whd_msgbuf_update_rxbufpost_count(msgbuf, 1);

    return;

}

static void
whd_msgbuf_process_d2h_mbdata(struct whd_msgbuf *msgbuf, void *buf)
{
    struct msgbuf_d2h_mailbox_data *d2h_mbdata;

    d2h_mbdata = (struct msgbuf_d2h_mailbox_data *)buf;

    if (!d2h_mbdata)
    {
        WPRINT_WHD_ERROR( ("d2h_mbdata is null\n") );
        return;
    }

    whd_bus_handle_mb_data(msgbuf->drvr, d2h_mbdata->mbdata);
}

static void whd_msgbuf_process_msgtype(struct whd_msgbuf *msgbuf, void *buf)
{
    struct whd_driver *drvr = msgbuf->drvr;
    struct msgbuf_common_hdr *msg;

    msg = (struct msgbuf_common_hdr *)buf;

    WPRINT_WHD_DEBUG( ("Handling RX msgtype - %d \n", msg->msgtype) );

    switch (msg->msgtype)
    {
        case MSGBUF_TYPE_GEN_STATUS:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_GEN_STATUS\n") );
            whd_msgbuf_process_gen_status(msgbuf, buf);
            break;
        case MSGBUF_TYPE_RING_STATUS:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_RING_STATUS\n") );
            whd_msgbuf_process_ring_status(msgbuf, buf);
            break;
        case MSGBUF_TYPE_FLOW_RING_CREATE_CMPLT:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_FLOW_RING_CREATE_CMPLT\n") );
            whd_msgbuf_process_flow_ring_create_response(msgbuf, buf);
            break;
        case MSGBUF_TYPE_FLOW_RING_DELETE_CMPLT:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_FLOW_RING_DELETE_CMPLT\n") );
            whd_msgbuf_process_flow_ring_delete_response(msgbuf, buf);
            break;
        case MSGBUF_TYPE_IOCTLPTR_REQ_ACK:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_IOCTLPTR_REQ_ACK\n") );
            break;
        case MSGBUF_TYPE_IOCTL_CMPLT:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_IOCTL_CMPLT\n") );
            whd_msgbuf_process_ioctl_complete(drvr, msgbuf, buf);
            break;
        case MSGBUF_TYPE_WL_EVENT:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_WL_EVENT\n") );
            whd_msgbuf_process_event(msgbuf, buf);
            break;
        case MSGBUF_TYPE_TX_STATUS:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_TX_STATUS\n") );
            whd_msgbuf_process_txstatus(msgbuf, buf);
            break;
        case MSGBUF_TYPE_RX_CMPLT:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_RX_CMPLT\n") );
            whd_msgbuf_process_rx_complete(msgbuf, buf);
            break;
        case MSGBUF_TYPE_D2H_MAILBOX_DATA:
            WPRINT_WHD_DEBUG( ("MSGBUF_TYPE_D2H_MAILBOX_DATA\n") );
            whd_msgbuf_process_d2h_mbdata(msgbuf, buf);
            break;

        default:
            WPRINT_WHD_DEBUG( ("Unsupported msgtype %d\n", msg->msgtype) );
            break;
    }
}

static uint32_t whd_msgbuf_process_rx_buffer(struct whd_msgbuf *msgbuf,
                                         struct whd_commonring *commonring)
{
    void *buf;
    uint16_t count;
    uint16_t processed;

again:
    buf = whd_commonring_get_read_ptr(commonring, &count);

    if (buf == NULL)
        return 0;
    else
        WPRINT_WHD_DEBUG( ("<== %s: Received read pointer is NOT NULL \n", __func__) );

    processed = 0;
    while (count)
    {
        whd_msgbuf_process_msgtype(msgbuf, (uint8_t *)buf + msgbuf->rx_dataoffset);
        buf = (uint8_t *)buf + whd_commonring_len_item(commonring);
        processed++;
        if (processed == WHD_MSGBUF_UPDATE_RX_PTR_THRS)
        {
            whd_commonring_read_complete(commonring, processed);
            processed = 0;
        }
        count--;
    }
    if (processed)
        whd_commonring_read_complete(commonring, processed);

    if (commonring->r_ptr == 0)
        goto again;

    DELAYED_BUS_RELEASE_SCHEDULE(msgbuf->drvr, WHD_TRUE);

    return 1;
}

uint32_t whd_msgbuf_process_rx_packet(struct whd_driver *dev)
{
    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)dev->msgbuf;
    void *buf;
    uint32_t result;

    buf = msgbuf->commonrings[WHD_D2H_MSGRING_RX_COMPLETE];
    result = whd_msgbuf_process_rx_buffer(msgbuf, buf);
    buf = msgbuf->commonrings[WHD_D2H_MSGRING_TX_COMPLETE];
    result = whd_msgbuf_process_rx_buffer(msgbuf, buf);
    buf = msgbuf->commonrings[WHD_D2H_MSGRING_CONTROL_COMPLETE];
    result = whd_msgbuf_process_rx_buffer(msgbuf, buf);

    return result;
}

void whd_msgbuf_delete_flowring(struct whd_driver *drvr, uint16_t flowid)
{
    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)drvr->msgbuf;
    struct msgbuf_tx_flowring_delete_req *delete;
    struct whd_commonring *commonring;
    struct whd_flowring *flow = msgbuf->flow;
    void *ret_ptr;
    uint8_t ifidx;
    int err;

    /* make sure it is not in txflow , TBD */
    flow->rings[flowid]->status = RING_CLOSING;

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_CONTROL_SUBMIT];
    whd_commonring_lock(commonring);
    ret_ptr = whd_commonring_reserve_for_write(commonring);
    if (!ret_ptr)
    {
        WPRINT_WHD_ERROR( ("FW unaware, flowring will be removed !!\n") );
        whd_commonring_unlock(commonring);
        whd_msgbuf_remove_flowring(msgbuf, flowid);
        return;
    }

    delete = (struct msgbuf_tx_flowring_delete_req *)ret_ptr;

    ifidx = whd_flowring_ifidx_get(msgbuf->flow, flowid);

    delete->msg.msgtype = MSGBUF_TYPE_FLOW_RING_DELETE;
    delete->msg.ifidx = ifidx;
    delete->msg.request_ptr = 0;

    delete->flow_ring_id = htod16(flowid +
                                  WHD_H2D_MSGRING_FLOWRING_IDSTART);
    delete->reason = 0;

    WPRINT_WHD_DEBUG( ("Send Flow Delete Req flow ID %d, ifindex %d\n",
                       flowid, ifidx) );

    err = whd_commonring_write_complete(commonring);
    whd_commonring_unlock(commonring);
    if (err)
    {
        WPRINT_WHD_ERROR( ("Failed to submit RING_DELETE, flowring will be removed\n") );
        whd_msgbuf_remove_flowring(msgbuf, flowid);
    }
}

static whd_result_t whd_msgbuf_txflow_reinsert(struct whd_flowring *flow, uint16_t flowid,
                                           whd_buffer_t skb)
{
    struct whd_flowring_ring *ring = flow->rings[flowid];
    struct whd_driver *drvr = flow->dev;
    whd_msgbuftx_info_t *msgtx_info = &ring->txflow_queue;
    whd_result_t result;

    if (cy_rtos_get_semaphore(&msgtx_info->send_queue_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE) != WHD_SUCCESS)
    {
        /* Could not obtain mutex */
        /* Fatal error */
        result = whd_buffer_release(drvr, skb, WHD_NETWORK_TX);
        if (result != WHD_SUCCESS)
            WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    if ( (msgtx_info->send_queue_head == NULL) && (msgtx_info->send_queue_tail == NULL) )
    {
        whd_msgbuf_set_next_buffer_in_queue(drvr, NULL, skb);
        msgtx_info->send_queue_head = msgtx_info->send_queue_tail = skb;
    }
    else if (msgtx_info->send_queue_head != NULL)
    {
        whd_msgbuf_set_next_buffer_in_queue(drvr, msgtx_info->send_queue_head, skb);
        msgtx_info->send_queue_head = skb;
    }
    else
    {
        WPRINT_WHD_ERROR( ("Error here %s at %d, head: 0x%x, tail: 0x%x\n", __func__, __LINE__,
                           (unsigned int)msgtx_info->send_queue_head, (unsigned int)msgtx_info->send_queue_tail) );
    }
    msgtx_info->npkt_in_q++;

    result = cy_rtos_set_semaphore(&msgtx_info->send_queue_mutex, WHD_FALSE);

    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );

    return WHD_SUCCESS;
}

whd_result_t whd_get_high_priority_flowring(whd_driver_t whd_driver, uint32_t num_flowring, uint16_t *prio_ring_id)
{
    struct whd_msgbuf *msgbuf = whd_driver->msgbuf;
    struct whd_flowring *flow = msgbuf->flow;
    uint32_t compare = 0, i = 0;
    bool fr_act_avl = WHD_FALSE;

    for (i = 0; i < num_flowring; i++) {
        if(isset(whd_driver->msgbuf->flow_map, i))
        {
            struct whd_flowring_ring *ring = flow->rings[i];

            if (compare <= ring->ac_prio)
            {
                compare = ring->ac_prio;
                *prio_ring_id = i;
            }
            fr_act_avl = WHD_TRUE;
        }
    }

    if(fr_act_avl == WHD_TRUE)
    {
        return WHD_SUCCESS;
    }
    else
    {
        return WHD_BADARG;
    }
}

whd_result_t whd_msgbuf_txflow(struct whd_driver *drvr, uint16_t flowid)
{
    struct whd_msgbuf *msgbuf = drvr->msgbuf;
    struct whd_commonring *commonring;
    void *ret_ptr;
    struct msgbuf_tx_msghdr *tx_msghdr;
    uint32_t address;
    uint32_t count;
    uint32_t physaddr;
    uint32_t pktid;
    whd_buffer_t skb;
    whd_result_t result;
    struct whd_flowring *flow = msgbuf->flow;
    struct whd_flowring_ring *ring = flow->rings[flowid];

    WPRINT_WHD_DEBUG( (" %s : Created Flow Id is %d \n", __func__, flowid) );

    commonring = msgbuf->flowrings[flowid];
    if (!whd_commonring_write_available(commonring) )
        return WHD_BUS_MEM_RESERVE_FAIL;

    //whd_commonring_lock(commonring);	//to be fixed

    count = WHD_MSGBUF_TX_FLUSH_CNT2 - WHD_MSGBUF_TX_FLUSH_CNT1;
    while (whd_flowring_qlen(flow, flowid) )
    {
        result = whd_msgbuf_txflow_dequeue(drvr, &skb, flowid);
        if ( (result != WHD_SUCCESS) || (skb == NULL) )
        {
            WPRINT_WHD_ERROR( ("No SKB, but qlen %u\n", (unsigned int)whd_flowring_qlen(flow, flowid) ) );
            break;
        }
        CHECK_RETURN(whd_buffer_add_remove_at_front(drvr, &skb, (int32_t)(sizeof(whd_buffer_header_t)) ) );

        if (whd_msgbuf_alloc_pktid(drvr, msgbuf->tx_pktids, skb, WHD_ETHERNET_SIZE,
                                   &physaddr, &pktid) )
        {
            whd_msgbuf_txflow_reinsert(flow, flowid, skb);
            WPRINT_WHD_ERROR( ("No PKTID available !!\n") );
            result = WHD_NO_PKT_ID_AVAILABLE;
            break;
        }

        ret_ptr = whd_commonring_reserve_for_write(commonring);
        WPRINT_WHD_DEBUG( ("TX flowring Reserve ptr is 0x%lx \n", (uint32_t)ret_ptr) );
        if (!ret_ptr)
        {
            whd_msgbuf_get_pktid(drvr, msgbuf->tx_pktids, pktid);
            whd_msgbuf_txflow_reinsert(flow, flowid, skb);
            WPRINT_WHD_ERROR( ("%s: ERROR in Reserving  for Write \n", __func__) );
            result = WHD_BUS_MEM_RESERVE_FAIL;
            break;
        }
        WPRINT_WHD_DATA_LOG( ("Wcd:> Sending pkt 0x%08lX\n", (unsigned long)skb) );
        WHD_STATS_INCREMENT_VARIABLE(drvr, tx_total);
        count++;

        tx_msghdr = (struct msgbuf_tx_msghdr *)ret_ptr;

        tx_msghdr->msg.msgtype = MSGBUF_TYPE_TX_POST;
        tx_msghdr->msg.request_ptr = htod32(pktid + 1);
        tx_msghdr->msg.ifidx = flow->hash[ring->hash_id].ifidx;
        tx_msghdr->flags = WHD_MSGBUF_PKT_FLAGS_FRAME_802_3;
        tx_msghdr->flags |= ((msgbuf->priority) & 0x07) << WHD_MSGBUF_PKT_FLAGS_PRIO_SHIFT;
        tx_msghdr->seg_cnt = 1;
        whd_mem_memcpy(tx_msghdr->txhdr, whd_buffer_get_current_piece_data_pointer(drvr, skb), WHD_ETHERNET_SIZE);
        tx_msghdr->data_len = (whd_buffer_get_current_piece_size(drvr, skb) - htod16(WHD_ETHERNET_SIZE) );
        address = (uint32_t)physaddr;
        tx_msghdr->data_buf_addr.high_addr = 0x0;
        tx_msghdr->data_buf_addr.low_addr = htod32(address & 0xffffffff);
        tx_msghdr->metadata_buf_len = 0;
        tx_msghdr->metadata_buf_addr.high_addr = 0;
        tx_msghdr->metadata_buf_addr.low_addr = 0;
        if (count >= WHD_MSGBUF_TX_FLUSH_CNT2)
        {
            whd_commonring_write_complete(commonring);
            count = 0;
        }
    }

    if (count)
        whd_commonring_write_complete(commonring);
    //whd_commonring_unlock(commonring);	//to be fixed

    return result;
}

whd_result_t whd_msgbuf_txflow_init(whd_msgbuftx_info_t *msgtx_info)
{
    /* Create the msgbuf tx packet queue semaphore */
    if (cy_rtos_init_semaphore(&msgtx_info->send_queue_mutex, 1, 0) != WHD_SUCCESS)
    {
        return WHD_SEMAPHORE_ERROR;
    }
    if (cy_rtos_set_semaphore(&msgtx_info->send_queue_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    msgtx_info->send_queue_head = (whd_buffer_t)NULL;
    msgtx_info->send_queue_tail = (whd_buffer_t)NULL;
    msgtx_info->npkt_in_q = 0;

    return WHD_SUCCESS;
}

whd_result_t whd_msgbuf_txflow_deinit(whd_msgbuftx_info_t *msgtx_info)
{
    msgtx_info->send_queue_head = (whd_buffer_t)NULL;
    msgtx_info->send_queue_tail = (whd_buffer_t)NULL;
    msgtx_info->npkt_in_q = 0;

    /* Delete the msgbuf tx packet queue semaphore */
    if (cy_rtos_deinit_semaphore(&msgtx_info->send_queue_mutex) != WHD_SUCCESS)
    {
        return WHD_SEMAPHORE_ERROR;
    }

    return WHD_SUCCESS;
}

static void whd_msgbuf_set_next_buffer_in_queue(whd_driver_t whd_driver, whd_buffer_t buffer, whd_buffer_t prev_buffer)
{
    whd_buffer_header_t *packet =
        (whd_buffer_header_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, prev_buffer);
    packet->queue_next = buffer;
}

static whd_buffer_t whd_msgbuf_get_next_buffer_in_queue(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_buffer_header_t *packet = (whd_buffer_header_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, buffer);
    return packet->queue_next;
}

static int whd_msgbuf_schedule_txdata(struct whd_msgbuf *msgbuf, uint32_t flowid,
                                      whd_bool_t force)
{
    whd_driver_t whd_driver = msgbuf->drvr;
    setbit(msgbuf->flow_map, flowid);

    if (force == WHD_TRUE)
    {
        whd_thread_notify(whd_driver);
    }
    return 0;
}

whd_result_t whd_msgbuf_txflow_dequeue(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t flowid)
{
    struct whd_flowring_ring *ring;
    struct whd_msgbuf *msgbuf = whd_driver->msgbuf;
    struct whd_flowring *flow = msgbuf->flow;
    whd_result_t result;

    ring = flow->rings[flowid];
    whd_msgbuftx_info_t *msgtx_info = &ring->txflow_queue;
    *buffer = NULL;

    if (ring->status != RING_OPEN)
    {
        WPRINT_WHD_ERROR( ("Flow Ring is not Opened, %s failed at %d\n", __func__, __LINE__) );
        return WHD_NO_REGISTER_FUNCTION_POINTER;
    }

    if (msgtx_info->npkt_in_q == 0)
    {
        return WHD_NO_PACKET_TO_SEND;
    }

    /* There is a packet waiting to be sent - send it then fix up queue and release packet */
    if (cy_rtos_get_semaphore(&msgtx_info->send_queue_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE) != WHD_SUCCESS)
    {
        /* Could not obtain mutex, push back the flow control semaphore */
        WPRINT_WHD_ERROR( ("Error manipulating a semaphore, %s failed at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    /* Pop the head off and set the new send_queue head */
    WPRINT_WHD_DEBUG( ("Dequeuing --- \n") );
    *buffer = msgtx_info->send_queue_head;
    msgtx_info->send_queue_head = whd_msgbuf_get_next_buffer_in_queue(whd_driver, *buffer);

    if (msgtx_info->send_queue_head == NULL)
    {
        msgtx_info->send_queue_tail = NULL;
    }

    WPRINT_WHD_DEBUG(("Dequeue --> send_queue_head - %p\n", *buffer));
    msgtx_info->npkt_in_q--;

    result = cy_rtos_set_semaphore(&msgtx_info->send_queue_mutex, WHD_FALSE);

    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_msgbuf_txflow_enqueue(whd_driver_t whd_driver, whd_buffer_t buffer, uint8_t prio,
                                              uint8_t flowid)
{
    uint8_t *data = NULL;
    whd_result_t result;
    struct whd_msgbuf *msgbuf = whd_driver->msgbuf;
    struct whd_flowring *flow = msgbuf->flow;
    struct whd_flowring_ring *ring;
    ring = flow->rings[flowid];
    whd_msgbuftx_info_t *msgtx_info = &ring->txflow_queue;

    CHECK_PACKET_NULL(buffer, WHD_NO_REGISTER_FUNCTION_POINTER);

    data = whd_buffer_get_current_piece_data_pointer(whd_driver, buffer);
    CHECK_PACKET_NULL(data, WHD_NO_REGISTER_FUNCTION_POINTER);

    if (cy_rtos_get_semaphore(&msgtx_info->send_queue_mutex, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE) != WHD_SUCCESS)
    {
        /* Could not obtain mutex */
        /* Fatal error */
        result = whd_buffer_release(whd_driver, buffer, WHD_NETWORK_TX);
        if (result != WHD_SUCCESS)
            WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    msgbuf->priority = prio;
    /* Set the ac priority for flowring to queue the packet based on prioritization */
    ring->ac_prio = whd_flowring_prio2fifo[msgbuf->priority];

    WPRINT_WHD_DEBUG(("Enqueuing +++ \n"));
    CHECK_RETURN(whd_buffer_add_remove_at_front(whd_driver, &buffer, -(int)(sizeof(whd_buffer_header_t)) ) );
    whd_msgbuf_set_next_buffer_in_queue(whd_driver, NULL, buffer);
    if (msgtx_info->send_queue_tail != NULL)
    {
        whd_msgbuf_set_next_buffer_in_queue(whd_driver, buffer, msgtx_info->send_queue_tail);
    }

    msgtx_info->send_queue_tail = buffer;
    if (msgtx_info->send_queue_head == NULL)
    {
        msgtx_info->send_queue_head = buffer;
    }

    WPRINT_WHD_DEBUG(("Enqueue <-- send_queue_head - %p\n", msgtx_info->send_queue_head));
    msgtx_info->npkt_in_q++;

    result = cy_rtos_set_semaphore(&msgtx_info->send_queue_mutex, WHD_FALSE);

    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );

    return WHD_SUCCESS;
}

static uint32_t
whd_msgbuf_flowring_create_worker(struct whd_msgbuf *msgbuf, struct whd_msgbuf_work_item *work)
{
    struct msgbuf_tx_flowring_create_req *create;
    struct whd_commonring *commonring;
    void *ret_ptr;
    uint32_t flowid;
    uint32_t flow_sz;
    uint32_t address;
    int err;

    flowid = work->flowid;
    flow_sz = WHD_H2D_TXFLOWRING_MAX_ITEM * WHD_H2D_TXFLOWRING_ITEMSIZE;

    /* memory allocation for WLAN DMA is permanent, no need to do
     * allocation for same flowid again and again */
    if (msgbuf->flowring_handle[flowid] == (uint32_t)NULL)
    {
        msgbuf->flowring_handle[flowid] = (uint32_t)whd_dmapool_alloc(flow_sz);
    }

    if (!(msgbuf->flowring_handle[flowid]) )
    {
        WPRINT_WHD_ERROR( ("DMA Alloc for FlowRingfailed\n") );
        whd_flowring_delete(msgbuf->flow, flowid);
        return WHD_FLOWRING_INVALID_ID;
    }

    whd_commonring_config(msgbuf->flowrings[flowid],
                          WHD_H2D_TXFLOWRING_MAX_ITEM,
                          WHD_H2D_TXFLOWRING_ITEMSIZE, (void *)msgbuf->flowring_handle[flowid]);

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_CONTROL_SUBMIT];
    whd_commonring_lock(commonring);
    ret_ptr = whd_commonring_reserve_for_write(commonring);

    if (!ret_ptr)
    {
        WPRINT_WHD_DEBUG( ("Failed to reserve space in commonring\n") );
        whd_commonring_unlock(commonring);
        whd_msgbuf_remove_flowring(msgbuf, flowid);
        return WHD_FLOWRING_INVALID_ID;
    }

    create = (struct msgbuf_tx_flowring_create_req *)ret_ptr;
    create->msg.msgtype = MSGBUF_TYPE_FLOW_RING_CREATE;
    create->msg.ifidx = work->ifidx;
    create->msg.request_ptr = 0;
    create->tid = whd_flowring_tid(msgbuf->flow, flowid);
    create->flow_ring_id = htod16(flowid + WHD_H2D_MSGRING_FLOWRING_IDSTART);
    whd_mem_memcpy(create->sa, work->sa, ETHER_ADDR_LEN);
    whd_mem_memcpy(create->da, work->da, ETHER_ADDR_LEN);
    address = (uint32_t)msgbuf->flowring_handle[flowid];
    create->flow_ring_addr.high_addr = 0x0;
    create->flow_ring_addr.low_addr = htod32(address & 0xffffffff);
    create->max_items = htod16(WHD_H2D_TXFLOWRING_MAX_ITEM);
    create->len_item = htod16(WHD_H2D_TXFLOWRING_ITEMSIZE);

    WPRINT_WHD_DEBUG( ("Send Flow Create Req flow ID %lu for peer(%x:%x:%x:%x:%x:%x) prio %u ifindex %d\n",
                       flowid, work->da[0], work->da[1], work->da[2], work->da[3], work->da[4], work->da[5],
                       create->tid, work->ifidx) );

    err = whd_commonring_write_complete(commonring);
    whd_commonring_unlock(commonring);
    if (err)
    {
        WPRINT_WHD_DEBUG( ("Failed to write commonring\n") );
        whd_msgbuf_remove_flowring(msgbuf, flowid);
        return WHD_FLOWRING_INVALID_ID;
    }

    return flowid;
}

static uint32_t whd_msgbuf_flowring_create(struct whd_msgbuf *msgbuf, int ifidx, void *skb, uint32_t prio)
{
    struct whd_msgbuf_work_item *create;
    ether_header_t *eh = (ether_header_t *)whd_buffer_get_current_piece_data_pointer(msgbuf->drvr, skb);
    uint32_t flowid;

    create = whd_mem_malloc(sizeof(*create) );
    if (create == NULL)
        return WHD_FLOWRING_INVALID_ID;

    whd_mem_memset(create, 0, sizeof(*create) );

    flowid = whd_flowring_create(msgbuf->flow, eh->destination_address, prio, ifidx);

    if (flowid == WHD_FLOWRING_INVALID_ID)
    {
        whd_mem_free(create);
        return flowid;
    }

    create->flowid = flowid;
    create->ifidx = ifidx;
    whd_mem_memcpy(create->sa, eh->source_address, ETHER_ADDR_LEN);
    whd_mem_memcpy(create->da, eh->destination_address, ETHER_ADDR_LEN);

    if (whd_msgbuf_flowring_create_worker(msgbuf, create) == WHD_FLOWRING_INVALID_ID)
    {
        flowid = WHD_FLOWRING_INVALID_ID;
    }
    whd_mem_free(create);

    return flowid;
}

whd_result_t whd_msgbuf_tx_queue_data(whd_interface_t ifp, whd_buffer_t buffer)
{
    uint8_t *dscp = NULL;
    uint8_t priority = 0;
    uint32_t flowid = -1;
    whd_driver_t whd_driver = ifp->whd_driver;
    uint16_t ether_type;
    whd_result_t result = WHD_SUCCESS;

    struct whd_msgbuf *msgbuf = (struct whd_msgbuf *)whd_driver->msgbuf;
    struct whd_flowring *flow = msgbuf->flow;

    ether_header_t *ethernet_header = (ether_header_t *)whd_buffer_get_current_piece_data_pointer(
        whd_driver, buffer);

    CHECK_PACKET_NULL(ethernet_header, WHD_NO_REGISTER_FUNCTION_POINTER);
    ether_type = ntoh16(ethernet_header->ethertype);

    if ( ( (ether_type == WHD_ETHERTYPE_IPv4) || (ether_type == WHD_ETHERTYPE_DOT1AS) ) )
    {
        dscp = (uint8_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, buffer) + IPV4_DSCP_OFFSET;

        if (dscp != NULL)
        {
            if (*dscp != 0) /* If it's equal 0 then it's best effort traffic and nothing needs to be done */
            {
                priority = whd_map_dscp_to_priority(whd_driver, *dscp);
            }
        }

    }

    flowid = whd_flowring_lookup(flow, ethernet_header->destination_address, priority, ifp->ifidx);

    if (flowid == WHD_FLOWRING_INVALID_ID)
    {
        flowid = whd_msgbuf_flowring_create(msgbuf, ifp->ifidx, buffer, priority);

        WPRINT_WHD_DEBUG( (" %s : Created Flow Id is %lu \n", __func__, flowid) );

        if (flowid == WHD_FLOWRING_INVALID_ID)
        {
            WPRINT_WHD_ERROR( ("Created Flow Id Failed\n") );
            result = whd_buffer_release(whd_driver, buffer, WHD_NETWORK_TX);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
            return WHD_BADARG;
        }
        else
        {
            whd_msgbuf_txflow_enqueue(whd_driver, buffer, priority, flowid);
            return WHD_SUCCESS;
        }
    }

    whd_msgbuf_txflow_enqueue(whd_driver, buffer, priority, flowid);

    whd_msgbuf_schedule_txdata(msgbuf, flowid, WHD_TRUE);
    return WHD_SUCCESS;
}

static whd_result_t whd_msgbuf_rxbuf_data_post(struct whd_msgbuf *msgbuf, uint32_t count)
{
    struct whd_driver *drvr = msgbuf->drvr;
    struct whd_commonring *commonring;
    void *ret_ptr;
    whd_buffer_t rx_databuf = NULL;
    uint16_t alloced = 0;
    uint32_t pktlen = 0;
    struct msgbuf_rx_bufpost *rx_bufpost;
    uint32_t physaddr;
    uint32_t address;
    uint32_t pktid;
    uint8_t i = 0;
    uint32_t result = 0;

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_RXPOST_SUBMIT];

    ret_ptr = whd_commonring_reserve_for_write_multiple(commonring, count, &alloced);

    WPRINT_WHD_DEBUG( ("%s : Allocated is %d , count is %ld \n", __func__, alloced, count) );

    if (!ret_ptr)
    {
        WPRINT_WHD_DEBUG( ("Failed to reserve space in commonring\n") );
        return WHD_SUCCESS;
    }

    for (i = 0; i < alloced; i++)
    {
        rx_bufpost = (struct msgbuf_rx_bufpost *)ret_ptr;
        whd_mem_memset(rx_bufpost, 0, sizeof(*rx_bufpost) );

        result = whd_host_buffer_get(drvr, &rx_databuf, WHD_NETWORK_RX,
                                     (uint16_t)(WHD_MSGBUF_DATA_MAX_RX_SIZE + sizeof(whd_buffer_header_t)), WHD_RX_BUF_TIMEOUT);

        if (result != WHD_SUCCESS)
        {
            whd_commonring_write_cancel(commonring, alloced - i);

            if(msgbuf->rxbufpost <= WHD_MSGBUF_RXBUFPOST_THRESHOLD)
            {
                WPRINT_WHD_ERROR(("Allocation Failed error - %lu, need to alloced %d, available - %ld\n", result, alloced, msgbuf->rxbufpost));
            }
            break;
        }
        /* Since the buffer to be given to WLAN DMA, Adding 2 bytes for DMA Alignment */
        CHECK_RETURN(whd_buffer_add_remove_at_front(drvr, &rx_databuf, (int)(sizeof(whd_buffer_header_t) + 2)));

        pktlen = whd_buffer_get_current_piece_size(drvr, rx_databuf);

        WPRINT_WHD_DEBUG( ("RX Buf Data Address is 0x%lx \n", (uint32_t)rx_databuf) );

        if (whd_msgbuf_alloc_pktid(drvr, msgbuf->rx_pktids, rx_databuf, 0, &physaddr, &pktid) )
        {
            result = whd_buffer_release(drvr, rx_databuf, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
            WPRINT_WHD_ERROR( ("No PKTID available !!\n") );
            whd_commonring_write_cancel(commonring, alloced - i);
            break;
        }

        if (msgbuf->rx_metadata_offset)
        {
            address = (uint32_t)rx_databuf;
            rx_bufpost->metadata_buf_len = htod16(msgbuf->rx_metadata_offset);
            rx_bufpost->metadata_buf_addr.high_addr = htod32(0);
            rx_bufpost->metadata_buf_addr.low_addr = htod32(address & 0xffffffff);
            pktlen = WHD_MSGBUF_DATA_MAX_RX_SIZE +  msgbuf->rx_metadata_offset;
            rx_databuf = (uint8_t *)rx_databuf + msgbuf->rx_metadata_offset;
        }
        rx_bufpost->msg.msgtype = MSGBUF_TYPE_RXBUF_POST;
        rx_bufpost->msg.request_ptr = htod32(pktid);

        address = (uint32_t)physaddr;
        rx_bufpost->data_buf_len = htod16(pktlen);
        rx_bufpost->data_buf_addr.high_addr = htod32(0);
        rx_bufpost->data_buf_addr.low_addr = htod32(address & 0xffffffff);

        ret_ptr = (uint8_t *)ret_ptr + whd_commonring_len_item(commonring);
    }

    if (i)
        whd_commonring_write_complete(commonring);

    return i;
}

static void
whd_msgbuf_rxbuf_data_fill(struct whd_msgbuf *msgbuf)
{
    uint32_t fillbufs;
    uint32_t retcount;

    fillbufs = msgbuf->max_rxbufpost - msgbuf->rxbufpost;

    WPRINT_WHD_DEBUG( ("%s : fillbufs - %ld \n", __func__, fillbufs) );

    while (fillbufs)
    {
        retcount = whd_msgbuf_rxbuf_data_post(msgbuf, fillbufs);
        WPRINT_WHD_DEBUG( ("%s : retcount is %ld \n", __func__, retcount) );

        if (!retcount)
            break;

        msgbuf->rxbufpost += retcount;
        fillbufs -= retcount;
    }

    if (msgbuf->rxbufpost == WHD_MSGBUF_RXBUFPOST_THRESHOLD)
    {
        bool is_rxbuf_timer_running = WHD_FALSE;
        cy_rtos_is_running_timer(&msgbuf->drvr->rxbuf_update_timer, &is_rxbuf_timer_running);

        if (is_rxbuf_timer_running == WHD_FALSE)
        {
            whd_wifi_rxbuf_fill_timer_start(msgbuf->drvr);
        }
    }

}

static void whd_msgbuf_update_rxbufpost_count(struct whd_msgbuf *msgbuf, uint16_t rxcnt)
{
    msgbuf->rxbufpost -= rxcnt;
    if (msgbuf->rxbufpost <= (msgbuf->max_rxbufpost - WHD_MSGBUF_RXBUFPOST_THRESHOLD) )
        whd_msgbuf_rxbuf_data_fill(msgbuf);
}

static uint32_t
whd_msgbuf_rxbuf_ctrl_post(struct whd_msgbuf *msgbuf, uint8_t event_buf,
                           uint32_t count)
{
    struct whd_driver *drvr = msgbuf->drvr;
    struct whd_commonring *commonring;
    void *ret_ptr = NULL;
    whd_buffer_t rx_ctlbuf = NULL;
    uint16_t allocated;
    uint32_t pktlen = 0;
    struct msgbuf_rx_ioctl_resp_or_event *rx_bufpost = NULL;
    uint32_t physaddr;
    uint32_t address;
    uint32_t pktid;
    uint8_t i = 0, result = -1;

    commonring = msgbuf->commonrings[WHD_H2D_MSGRING_CONTROL_SUBMIT];
    whd_commonring_lock(commonring);
    ret_ptr = whd_commonring_reserve_for_write_multiple(commonring, count, &allocated);

    if (!ret_ptr)
    {
        WPRINT_WHD_ERROR( ("Failed to reserve space in commonring\n") );
        whd_commonring_unlock(commonring);
        return WHD_BUS_MEM_RESERVE_FAIL;
    }

    WPRINT_WHD_DEBUG( ("%s - allocated is %d \n", __func__, allocated) );

    for (i = 0; i < allocated; i++)
    {
        rx_bufpost = (struct msgbuf_rx_ioctl_resp_or_event *)ret_ptr;
        whd_mem_memset(rx_bufpost, 0, sizeof(*rx_bufpost) );

        result = whd_host_buffer_get(drvr, &rx_ctlbuf, WHD_NETWORK_RX,
                                     (uint16_t)((event_buf) ? WHD_MSGBUF_EVENT_MAX_RX_SIZE : WHD_MSGBUF_IOCTL_MAX_RX_SIZE),
                                    WHD_RX_BUF_TIMEOUT);

        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s : Allocation Failed \n", __func__) );
            whd_commonring_write_cancel(commonring, allocated - i);
            break;
        }
        pktlen = whd_buffer_get_current_piece_size(drvr, rx_ctlbuf);

        WPRINT_WHD_DEBUG( ("RX Buf CTL Address is 0x%lx \n", (uint32_t)rx_ctlbuf) );

        if (whd_msgbuf_alloc_pktid(drvr, msgbuf->rx_pktids, rx_ctlbuf, 0, &physaddr, &pktid) )
        {
            result = whd_buffer_release(drvr, rx_ctlbuf, WHD_NETWORK_RX);
            if (result != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
            WPRINT_WHD_ERROR( ("No PKTID available !!\n") );
            whd_commonring_write_cancel(commonring, allocated - i);
            break;
        }

        if (event_buf)
            rx_bufpost->msg.msgtype = MSGBUF_TYPE_EVENT_BUF_POST;
        else
            rx_bufpost->msg.msgtype = MSGBUF_TYPE_IOCTLRESP_BUF_POST;

        rx_bufpost->msg.request_ptr = htod32(pktid);

        address = (uint32_t)physaddr;
        rx_bufpost->host_buf_len = htod16( (uint16_t)pktlen );
        rx_bufpost->host_buf_addr.high_addr = htod32(0);
        rx_bufpost->host_buf_addr.low_addr = htod32(address & 0xffffffff);

        ret_ptr = (uint8_t *)ret_ptr + whd_commonring_len_item(commonring);
    }

    if (i)
    {
        whd_commonring_write_complete(commonring);
    }
    whd_commonring_unlock(commonring);

    return i;
}

static void whd_msgbuf_rxbuf_ioctlresp_post(struct whd_msgbuf *msgbuf)
{
    uint32_t count;

    count = msgbuf->max_ioctlrespbuf - msgbuf->cur_ioctlrespbuf;
    count = whd_msgbuf_rxbuf_ctrl_post(msgbuf, 0, count);
    msgbuf->cur_ioctlrespbuf += count;
}

static void whd_msgbuf_rxbuf_event_post(struct whd_msgbuf *msgbuf)
{
    uint32_t count;

    count = msgbuf->max_eventbuf - msgbuf->cur_eventbuf;
    count = whd_msgbuf_rxbuf_ctrl_post(msgbuf, 1, count);
    msgbuf->cur_eventbuf += count;
}

static struct whd_msgbuf_pktids *
whd_msgbuf_init_pktids(uint32_t nr_array_entries)
{
    struct whd_msgbuf_pktid *array;
    struct whd_msgbuf_pktids *pktids;
    uint32_t i = 0;

    array = (struct whd_msgbuf_pktid *)whd_mem_malloc(nr_array_entries * sizeof(*array) );
    if (array == NULL)
    {
        WPRINT_WHD_DEBUG( ("array allocation failed \n") );
        return NULL;
    }
    whd_mem_memset(array, 0, sizeof(struct whd_msgbuf_pktid) );

    for(i = 0; i < nr_array_entries; i++)
    {
        array[i].allocated = 0;
    }

    pktids = (struct whd_msgbuf_pktids *)whd_mem_malloc(sizeof(*pktids) );
    if (pktids == NULL)
    {
        WPRINT_WHD_DEBUG( ("pktids allocation failed \n") );
        whd_mem_free(array);
        return NULL;
    }
    whd_mem_memset(pktids, 0, sizeof(struct whd_msgbuf_pktids) );

    pktids->array = array;
    pktids->array_size = nr_array_entries;

    return pktids;
}

static void whd_msgbuf_detach(struct whd_driver *whd_driver)
{
    struct whd_msgbuf *msgbuf;

    if (whd_driver->msgbuf)
    {
        msgbuf = (struct whd_msgbuf *)whd_driver->msgbuf;

        whd_flowring_detach(msgbuf->flow);
        whd_mem_free(msgbuf->flow_map);
        whd_buffer_release(whd_driver, msgbuf->ioctl_buffer, WHD_NETWORK_TX);
        msgbuf->ioctbuf = NULL;
        whd_mem_free(msgbuf->flowring_handle);
        whd_mem_free(msgbuf->flowrings);
        whd_msgbuf_release_pktids(whd_driver, msgbuf);
        whd_mem_free(msgbuf);
        whd_wifi_rxbuf_fill_timer_deinit(whd_driver);
    }
}

static whd_result_t whd_msgbuf_attach(struct whd_driver *whd_driver)
{
    struct whd_msgbuf *msgbuf;
    struct whd_commonring **flowrings = NULL;
    uint32_t address = 0, result = 0;
    uint32_t i = 0, count = 0;

    WPRINT_WHD_DEBUG( ("Msgbuf Attach Start \n") );

    msgbuf = (struct whd_msgbuf *)whd_mem_malloc(sizeof(*msgbuf) );
    if (msgbuf == NULL)
    {
        WPRINT_WHD_DEBUG( ("msgbuf allocation failed \n") );
        return WHD_MALLOC_FAILURE;
    }
    whd_mem_memset(msgbuf, 0, sizeof(struct whd_msgbuf) );

    count = CEIL(whd_driver->ram_shared->max_flowrings, NBBY);
    count = count * sizeof(uint8_t);
    msgbuf->flow_map = whd_mem_malloc(count);
    if (!msgbuf->flow_map)
    {
        WPRINT_WHD_ERROR( ("flow_map allocation failed \n") );
        goto fail;
    }
    whd_mem_memset(msgbuf->flow_map, 0, count);

    msgbuf->drvr = whd_driver;

    result = whd_host_buffer_get(whd_driver, (whd_buffer_t )&(msgbuf->ioctl_buffer), WHD_NETWORK_TX,
                                     (uint16_t)WHD_MSGBUF_IOCTL_MAX_TX_SIZE, WHD_RX_BUF_TIMEOUT);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("%s : Allocation Failed \n", __func__) );
        return result;
    }

    msgbuf->ioctbuf = whd_buffer_get_current_piece_data_pointer(whd_driver, (whd_buffer_t)msgbuf->ioctl_buffer);

    if (msgbuf->ioctbuf == NULL)
    {
        WPRINT_WHD_ERROR( ("Ioctbuf allocation failed \n") );
        goto fail;
    }

    address = (uint32_t)msgbuf->ioctbuf;
    msgbuf->ioctbuf_phys_hi = 0;
    msgbuf->ioctbuf_phys_lo = address & 0xffffffff;

    /* hook the commonrings in the main msgbuf structure. */
    for (i = 0; i < WHD_NROF_COMMON_MSGRINGS; i++)
        msgbuf->commonrings[i] = &whd_driver->ram_shared->commonrings[i]->commonring;

    WPRINT_WHD_DEBUG( ("msgbuf commonring pointer is 0x%lx \n", (uint32_t)msgbuf->commonrings) );

    msgbuf->flowring_handle = whd_mem_malloc(sizeof(*msgbuf->flowring_handle) * whd_driver->ram_shared->max_flowrings);
    if (msgbuf->flowring_handle == NULL)
    {
        WPRINT_WHD_ERROR( ("Flowring_handle allocation failed \n") );
        goto fail;
    }
    whd_mem_memset(msgbuf->flowring_handle, 0, sizeof(*msgbuf->flowring_handle) * whd_driver->ram_shared->max_flowrings);

    flowrings = whd_mem_malloc(sizeof(*flowrings) * whd_driver->ram_shared->max_flowrings);
    if (flowrings == NULL)
    {
        WPRINT_WHD_ERROR( ("Flowring allocation failed \n") );
        goto fail;
    }

    for (i = 0; i < whd_driver->ram_shared->max_flowrings; i++)
        flowrings[i] = &whd_driver->ram_shared->flowrings[i].commonring;

    msgbuf->flowrings = flowrings;
    msgbuf->rx_dataoffset = whd_driver->ram_shared->rx_dataoffset;
    msgbuf->max_rxbufpost = whd_driver->ram_shared->max_rxbufpost;
    msgbuf->max_flowrings = whd_driver->ram_shared->max_flowrings;

    msgbuf->max_ioctlrespbuf = WHD_MSGBUF_MAX_IOCTLRESPBUF_POST;
    msgbuf->max_eventbuf = WHD_MSGBUF_MAX_EVENTBUF_POST;

    msgbuf->tx_pktids = whd_msgbuf_init_pktids(NR_TX_PKTIDS);
    if (!msgbuf->tx_pktids)
        goto fail;
    /* Create the mutex protecting the packet send queue */
    if (cy_rtos_init_semaphore(&msgbuf->tx_pktids->pktid_mutex, 1, 0) != WHD_SUCCESS)
        goto fail;
    if (cy_rtos_set_semaphore(&msgbuf->tx_pktids->pktid_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    msgbuf->rx_pktids = whd_msgbuf_init_pktids(NR_RX_PKTIDS);
    if (!msgbuf->rx_pktids)
        goto fail;
    /* Create the mutex protecting the packet send queue */
    if (cy_rtos_init_semaphore(&msgbuf->rx_pktids->pktid_mutex, 1, 0) != WHD_SUCCESS)
        goto fail;
    if (cy_rtos_set_semaphore(&msgbuf->rx_pktids->pktid_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    msgbuf->flow = whd_flowring_attach(whd_driver, whd_driver->ram_shared->max_flowrings);
    if (!msgbuf->flow)
        goto fail;

    whd_driver->msgbuf = msgbuf;

    whd_msgbuf_rxbuf_ioctlresp_post(msgbuf);
    whd_msgbuf_rxbuf_event_post(msgbuf);


    do
    {
        whd_msgbuf_rxbuf_data_fill(msgbuf);
        cy_rtos_delay_milliseconds(1);
    } while (msgbuf->max_rxbufpost != msgbuf->rxbufpost);

    /* This timer is required when buffer availability becomes 0, where WHD and FW communication breaks */
    whd_wifi_rxbuf_fill_timer_init(whd_driver);

    WPRINT_WHD_DEBUG( ("Msgbuf Attach End \n") );
    return WHD_SUCCESS;

fail:
    if (msgbuf)
    {
        if (msgbuf->flow_map)
            whd_mem_free(msgbuf->flow_map);
        msgbuf->flow_map = NULL;
        if (msgbuf->ioctbuf)
            whd_mem_free(msgbuf->ioctbuf);
        msgbuf->ioctbuf = NULL;
        if (msgbuf->flowring_handle)
            whd_mem_free(msgbuf->flowring_handle);
        msgbuf->flowring_handle = NULL;
        if (flowrings)
            whd_mem_free(flowrings);
        msgbuf->flowrings = NULL;
        whd_msgbuf_release_pktids(whd_driver, msgbuf);
        whd_mem_free(msgbuf);
        whd_driver->msgbuf = NULL;
    }
    return WHD_MALLOC_FAILURE;
}

void whd_msgbuf_info_deinit(whd_driver_t whd_driver)
{
    whd_msgbuf_info_t *msgbuf_info = whd_driver->proto->pd;
    whd_error_info_t *error_info = &whd_driver->error_info;

    /* Delete the sleep mutex */
    (void)cy_rtos_deinit_semaphore(&msgbuf_info->ioctl_sleep);

    /* Delete the queue mutex.  */
    (void)cy_rtos_deinit_semaphore(&msgbuf_info->ioctl_mutex);

    /* Delete the event list management mutex */
    (void)cy_rtos_deinit_semaphore(&msgbuf_info->event_list_mutex);

    whd_msgbuf_detach(whd_driver);

    whd_driver->proto->pd = NULL;
    whd_mem_free(msgbuf_info);

    /* Delete the error list management mutex */
    (void)cy_rtos_deinit_semaphore(&error_info->event_list_mutex);
}

whd_result_t whd_msgbuf_info_init(whd_driver_t whd_driver)
{
    whd_msgbuf_info_t *msgbuf_info;
    whd_error_info_t *error_info = &whd_driver->error_info;

    msgbuf_info = (whd_msgbuf_info_t *)whd_mem_malloc(sizeof(whd_msgbuf_info_t) );
    if (!msgbuf_info)
    {
        return WHD_MALLOC_FAILURE;
    }

    /* Create the mutex protecting the packet send queue */
    if (cy_rtos_init_semaphore(&msgbuf_info->ioctl_mutex, 1, 0) != WHD_SUCCESS)
    {
        return WHD_SEMAPHORE_ERROR;
    }
    if (cy_rtos_set_semaphore(&msgbuf_info->ioctl_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    /* Create the event flag which signals the whd thread needs to wake up */
    if (cy_rtos_init_semaphore(&msgbuf_info->ioctl_sleep, 1, 0) != WHD_SUCCESS)
    {
        cy_rtos_deinit_semaphore(&msgbuf_info->ioctl_sleep);
        return WHD_SEMAPHORE_ERROR;
    }

    /* Create semaphore to protect event list management */
    if (cy_rtos_init_semaphore(&msgbuf_info->event_list_mutex, 1, 0) != WHD_SUCCESS)
    {
        cy_rtos_deinit_semaphore(&msgbuf_info->ioctl_sleep);
        cy_rtos_deinit_semaphore(&msgbuf_info->ioctl_mutex);
        return WHD_SEMAPHORE_ERROR;
    }
    if (cy_rtos_set_semaphore(&msgbuf_info->event_list_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    /* Initialise the list of event handler functions */
    whd_mem_memset(msgbuf_info->whd_event_list, 0, sizeof(msgbuf_info->whd_event_list) );

    /* Create semaphore to protect event list management */
    if (cy_rtos_init_semaphore(&error_info->event_list_mutex, 1, 0) != WHD_SUCCESS)
    {
        return WHD_SEMAPHORE_ERROR;
    }

    if (cy_rtos_set_semaphore(&error_info->event_list_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    /* Initialise the list of error handler functions */
    whd_mem_memset(error_info->whd_event_list, 0, sizeof(error_info->whd_event_list) );

    whd_driver->proto->get_ioctl_buffer = whd_msgbuf_get_ioctl_buffer;
    whd_driver->proto->get_iovar_buffer = whd_msgbuf_get_iovar_buffer;
    whd_driver->proto->set_ioctl = whd_msgbuf_set_ioctl;
    whd_driver->proto->get_ioctl = whd_msgbuf_get_ioctl;
    whd_driver->proto->set_iovar = whd_msgbuf_set_iovar;
    whd_driver->proto->get_iovar = whd_msgbuf_get_iovar;
    whd_driver->proto->tx_queue_data = whd_msgbuf_tx_queue_data;
    whd_driver->proto->pd = msgbuf_info;

    CHECK_RETURN(whd_msgbuf_attach(whd_driver) );

    return WHD_SUCCESS;
}

void whd_msgbuf_rxbuf_fill_all(struct whd_msgbuf *msgbuf)
{
    uint32_t retry_count = 0;
    cy_network_packet_pool_info_t rx_pool_avl;

    msgbuf->drvr->update_buffs = 0;

    do
    {
        cy_network_get_packet_pool_info(CY_NETWORK_PACKET_RX, &rx_pool_avl);

        if(rx_pool_avl.free_packets >= (msgbuf->max_rxbufpost - WHD_MSGBUF_RXBUFPOST_THRESHOLD))
        {
            whd_msgbuf_rxbuf_data_fill(msgbuf);
        }
        else
        {
            retry_count++;
            cy_rtos_delay_milliseconds(2000);
        }
    } while( (msgbuf->rxbufpost < (msgbuf->max_rxbufpost - WHD_MSGBUF_RXBUFPOST_THRESHOLD))
                                               && (retry_count <= WHD_MSGBUF_RXBUFPOST_RETRY_COUNT));

    if(msgbuf->cur_eventbuf == 0)
        whd_msgbuf_rxbuf_event_post(msgbuf);

}

void whd_msgbuf_indicate_to_fill_buffers(cy_timer_callback_arg_t arg)
{
     whd_driver_t whd_driver = (whd_driver_t)arg;

     whd_driver->update_buffs = 1;
     whd_thread_notify(whd_driver);

     whd_wifi_rxbuf_fill_timer_stop(whd_driver->msgbuf->drvr);
}

void whd_wifi_rxbuf_fill_timer_init(whd_driver_t whd_driver)
{
    cy_rtos_timer_init(&whd_driver->rxbuf_update_timer, CY_TIMER_TYPE_ONCE,
                                        whd_msgbuf_indicate_to_fill_buffers, (cy_timer_callback_arg_t ) whd_driver);
}

void whd_wifi_rxbuf_fill_timer_start(whd_driver_t whd_driver)
{
    /* Currently setting 60sec timed-out value to read the buffer availability.*/
    WPRINT_WHD_ERROR(("Out of Buffers - Needs sometime to recover \n"));
    cy_rtos_timer_start(&whd_driver->rxbuf_update_timer, WHD_MSGBUF_RXBUFPOST_TIMER_DELAY);
}

void whd_wifi_rxbuf_fill_timer_deinit(whd_driver_t whd_driver)
{
    cy_rtos_timer_deinit(&whd_driver->rxbuf_update_timer);
}

void whd_wifi_rxbuf_fill_timer_stop(whd_driver_t whd_driver)
{
    cy_rtos_timer_stop(&whd_driver->rxbuf_update_timer);
}

#endif /* PROTO_MSGBUF */
