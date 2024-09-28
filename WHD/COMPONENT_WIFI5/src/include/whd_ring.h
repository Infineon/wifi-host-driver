/*
 * Copyright 2023, Cypress Semiconductor Corporation (an Infineon company)
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

/**
 * @file WHD ring
 *
 * Utilities to help do specialized (not general purpose) WHD specific things
 */
#ifndef INCLUDED_WHD_RING_H
#define INCLUDED_WHD_RING_H

#include "whd.h"
#include "whd_commonring.h"
#include "whd_msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Translated Address for CM33 to access CR4 memory */
#define TRANS_ADDR(addr)                    ( (0x8F620000 - 0x003A0000) + addr )

#define WHD_RING_MEM_BASE_ADDR_OFFSET       8
#define WHD_RING_MAX_ITEM_OFFSET            4
#define WHD_RING_LEN_ITEMS_OFFSET           6
#define WHD_RING_MEM_SZ                     16
#define WHD_RING_STATE_SZ                   8

#define WHD_PCIE_SHARED_VERSION_6           6
#define WHD_PCIE_SHARED_VERSION_7           7
#define WHD_PCIE_MIN_SHARED_VERSION         5
#define WHD_PCIE_MAX_SHARED_VERSION         WHD_PCIE_SHARED_VERSION_7
#define WHD_PCIE_SHARED_VERSION_MASK        0x00FF
#define WHD_PCIE_SHARED_DMA_INDEX           0x10000
#define WHD_PCIE_SHARED_DMA_2B_IDX          0x100000
#define WHD_PCIE_SHARED_USE_MAILBOX         0x2000000
#define WHD_PCIE_SHARED_HOSTRDY_DB1         0x10000000

//Ram Shared Region Offsets
#define WHD_SHARED_MAX_RXBUFPOST_OFFSET     34
#define WHD_SHARED_RING_BASE_OFFSET         52
#define WHD_SHARED_RX_DATAOFFSET_OFFSET     36
#define WHD_SHARED_CONSOLE_ADDR_OFFSET      20
#define WHD_SHARED_HTOD_MB_DATA_ADDR_OFFSET 40
#define WHD_SHARED_DTOH_MB_DATA_ADDR_OFFSET 44
#define WHD_SHARED_RING_INFO_ADDR_OFFSET    48
#define WHD_SHARED_DMA_SCRATCH_LEN_OFFSET   52
#define WHD_SHARED_DMA_SCRATCH_ADDR_OFFSET  56
#define WHD_SHARED_DMA_RINGUPD_LEN_OFFSET   64
#define WHD_SHARED_DMA_RINGUPD_ADDR_OFFSET  68
#define WHD_SHARED_HOST_CAP_OFFSET          84

#define WHD_H2D_ENABLE_HOSTRDY              0x400

#define WHD_DEF_MAX_RXBUFPOST               8

#define WLAN_M2M_SHARED_VERSION_MASK    (0x00ff)
#define WLAN_M2M_SHARED_VERSION         (0x0007)

#define WHD_D2H_DEV_D3_ACK                0x00000001
#define WHD_D2H_DEV_DS_ENTER_REQ          0x00000002
#define WHD_D2H_DEV_DS_EXIT_NOTE          0x00000004
#define WHD_D2H_DEV_FWHALT                0x10000000

#define WHD_H2D_HOST_D3_INFORM            0x00000001
#define WHD_H2D_HOST_DS_ACK               0x00000002
#define WHD_H2D_HOST_D0_INFORM_IN_USE     0x00000008
#define WHD_H2D_HOST_D0_INFORM            0x00000010

#define WHD_MBDATA_TIMEOUT                (2000)

#define WHD_H2D_MSGRING_CONTROL_SUBMIT_MAX_ITEM     32
#define WHD_H2D_MSGRING_RXPOST_SUBMIT_MAX_ITEM      128
#define WHD_D2H_MSGRING_CONTROL_COMPLETE_MAX_ITEM   32
#define WHD_D2H_MSGRING_TX_COMPLETE_MAX_ITEM        128
#define WHD_D2H_MSGRING_RX_COMPLETE_MAX_ITEM        128
#define WHD_H2D_TXFLOWRING_MAX_ITEM                 128

/* Submit Itemsize for rings should match with FW item sizes */
#define WHD_H2D_MSGRING_CONTROL_SUBMIT_ITEMSIZE     40
#define WHD_H2D_MSGRING_RXPOST_SUBMIT_ITEMSIZE      32
#define WHD_D2H_MSGRING_CONTROL_COMPLETE_ITEMSIZE   24
#define WHD_D2H_MSGRING_TX_COMPLETE_ITEMSIZE_PRE_V7 16
#define WHD_D2H_MSGRING_TX_COMPLETE_ITEMSIZE        24
#define WHD_D2H_MSGRING_RX_COMPLETE_ITEMSIZE_PRE_V7 32
#define WHD_D2H_MSGRING_RX_COMPLETE_ITEMSIZE        40
#define WHD_H2D_TXFLOWRING_ITEMSIZE                 48

/* IDs of the 6 default common rings of msgbuf protocol */
#define WHD_H2D_MSGRING_CONTROL_SUBMIT    0
#define WHD_H2D_MSGRING_RXPOST_SUBMIT     1
#define WHD_H2D_MSGRING_FLOWRING_IDSTART  2
#define WHD_D2H_MSGRING_CONTROL_COMPLETE  2
#define WHD_D2H_MSGRING_TX_COMPLETE       3
#define WHD_D2H_MSGRING_RX_COMPLETE       4

static const uint32_t whd_ring_max_item[WHD_NROF_COMMON_MSGRINGS] = {
    WHD_H2D_MSGRING_CONTROL_SUBMIT_MAX_ITEM,
    WHD_H2D_MSGRING_RXPOST_SUBMIT_MAX_ITEM,
    WHD_D2H_MSGRING_CONTROL_COMPLETE_MAX_ITEM,
    WHD_D2H_MSGRING_TX_COMPLETE_MAX_ITEM,
    WHD_D2H_MSGRING_RX_COMPLETE_MAX_ITEM
};

static const uint32_t whd_ring_itemsize_pre_v7[WHD_NROF_COMMON_MSGRINGS] = {
    WHD_H2D_MSGRING_CONTROL_SUBMIT_ITEMSIZE,
    WHD_H2D_MSGRING_RXPOST_SUBMIT_ITEMSIZE,
    WHD_D2H_MSGRING_CONTROL_COMPLETE_ITEMSIZE,
    WHD_D2H_MSGRING_TX_COMPLETE_ITEMSIZE_PRE_V7,
    WHD_D2H_MSGRING_RX_COMPLETE_ITEMSIZE_PRE_V7
};

static const uint32_t whd_ring_itemsize[WHD_NROF_COMMON_MSGRINGS] = {
    WHD_H2D_MSGRING_CONTROL_SUBMIT_ITEMSIZE,
    WHD_H2D_MSGRING_RXPOST_SUBMIT_ITEMSIZE,
    WHD_D2H_MSGRING_CONTROL_COMPLETE_ITEMSIZE,
    WHD_D2H_MSGRING_TX_COMPLETE_ITEMSIZE,
    WHD_D2H_MSGRING_RX_COMPLETE_ITEMSIZE
};

struct whd_ringbuf
{
    struct whd_commonring commonring;
    void *ring_handle; //ram_todo dma_handle
    uint32_t w_idx_addr;
    uint32_t r_idx_addr;
    whd_driver_t whd_drv;
    uint8_t id;
};

struct whd_dhi_ringinfo
{
    uint32_t ringmem;
    uint32_t h2d_w_idx_ptr;
    uint32_t h2d_r_idx_ptr;
    uint32_t d2h_w_idx_ptr;
    uint32_t d2h_r_idx_ptr;
    struct msg_buf_addr h2d_w_idx_hostaddr;
    struct msg_buf_addr h2d_r_idx_hostaddr;
    struct msg_buf_addr d2h_w_idx_hostaddr;
    struct msg_buf_addr d2h_r_idx_hostaddr;
    uint32_t max_flowrings;
    uint16_t max_submissionrings;
    uint16_t max_completionrings;
};

struct whd_ram_shared_info
{
    uint32_t tcm_base_address;
    uint32_t flags;
    struct whd_ringbuf *commonrings[WHD_NROF_COMMON_MSGRINGS];
    struct whd_ringbuf *flowrings;
    uint16_t max_rxbufpost;
    uint16_t max_flowrings;
    uint16_t max_submissionrings;
    uint16_t max_completionrings;
    uint32_t rx_dataoffset;
    uint32_t htod_mb_data_addr;
    uint32_t dtoh_mb_data_addr;
    uint32_t ring_info_addr;
    //struct brcmf_pcie_console console;
    void *scratch;
    //ram_check dma_addr_t scratch_dmahandle;
    void *ringupd;
    //ram_check dma_addr_t ringupd_dmahandle;
    uint32_t version_new;
};

extern void whd_bus_handle_mb_data(whd_driver_t whd_driver, uint32_t d2h_mb_data);
extern whd_result_t whd_host_trigger_suspend(whd_driver_t whd_driver);
extern whd_result_t whd_host_trigger_resume(whd_driver_t whd_driver);
extern whd_result_t whd_bus_suspend(whd_driver_t whd_driver);
extern whd_result_t whd_bus_resume(whd_driver_t whd_driver);
extern whd_result_t whd_bus_m2m_sharedmem_init(whd_driver_t whd_driver);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INCLUDED_WHD_RING_H */
