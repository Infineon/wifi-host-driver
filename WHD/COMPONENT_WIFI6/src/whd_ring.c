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
 *  Provides generic Ring functionality that chip specific files use
 */
#ifdef PROTO_MSGBUF

#include "whd_utils.h"
#include "whd_int.h"
#include "whd_ring.h"
#include "whd_chip_constants.h"
#include "bus_protocols/whd_bus_protocol_interface.h"
#include "whd_oci.h"
#include "whd_buffer_api.h"

#ifdef GCI_SECURE_ACCESS
#include "whd_hw.h"
#endif

#define HT_AVAIL_WAIT_MS            (1)
#define WLAN_BUS_UP_ATTEMPTS        (1000)
#define WHD_HOST_TRIGGER_SUSPEND_TIMEOUT (WHD_MBDATA_TIMEOUT + 2 * 1000)
#define HOST_TRIGGER_SUSPEND_COMPLETE  (1UL << 0)

static int whd_ring_mb_ring_bell(void *ctx)
{
    WPRINT_WHD_DEBUG( ("RINGING !!!\n") );

    /* Any arbitrary value will do, lets use 1 */
#ifndef GCI_SECURE_ACCESS
    struct whd_ringbuf *ring = (struct whd_ringbuf *)ctx;
    whd_driver_t whd_driver = ring->whd_drv;
    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)GCI_BT2WL_DB0_REG, 4, 0x01) );
#else
    CHECK_RETURN(whd_hw_generateBt2WlDbInterruptApi(0, 0x01));
#endif

    return WHD_SUCCESS;
}

static int whd_ring_mb_update_rptr(void *ctx)
{
    struct whd_ringbuf *ring = (struct whd_ringbuf *)ctx;
    whd_driver_t whd_driver = ring->whd_drv;
    struct whd_commonring *commonring = &ring->commonring;

    commonring->r_ptr = whd_driver->read_ptr(whd_driver, ring->r_idx_addr);

    WPRINT_WHD_DEBUG( ("<== R : r_ptr %d (%d), ring_id %d\n", commonring->r_ptr,
                       commonring->w_ptr, ring->id) );

    return WHD_SUCCESS;
}

static int whd_ring_mb_update_wptr(void *ctx)
{
    struct whd_ringbuf *ring = (struct whd_ringbuf *)ctx;
    whd_driver_t whd_driver = ring->whd_drv;
    struct whd_commonring *commonring = &ring->commonring;

    commonring->w_ptr = whd_driver->read_ptr(whd_driver, ring->w_idx_addr);

    WPRINT_WHD_DEBUG( ("<== R : w_ptr %d (%d), ring_id %d\n", commonring->w_ptr,
                       commonring->r_ptr, ring->id) );

    return WHD_SUCCESS;
}

static int whd_ring_mb_write_rptr(void *ctx)
{
    struct whd_ringbuf *ring = (struct whd_ringbuf *)ctx;
    whd_driver_t whd_driver = ring->whd_drv;
    struct whd_commonring *commonring = &ring->commonring;

    WPRINT_WHD_DEBUG( ("==> W: r_ptr %d (%d), ring_id %d, r_idx_addr 0x%lx \n", commonring->r_ptr,
                       commonring->w_ptr, ring->id, ring->r_idx_addr) );

    whd_driver->write_ptr(whd_driver, ring->r_idx_addr, commonring->r_ptr);

    return WHD_SUCCESS;
}

static int whd_ring_mb_write_wptr(void *ctx)
{
    struct whd_ringbuf *ring = (struct whd_ringbuf *)ctx;
    whd_driver_t whd_driver = ring->whd_drv;
    struct whd_commonring *commonring = &ring->commonring;

    WPRINT_WHD_DEBUG( ("==> W: w_ptr %d (%d), ring_id %d, w_idx_addr 0x%lx \n", commonring->w_ptr,
                       commonring->r_ptr, ring->id, ring->w_idx_addr) );

    whd_driver->write_ptr(whd_driver, ring->w_idx_addr, commonring->w_ptr);

    return WHD_SUCCESS;
}

static uint16_t whd_read_tcm16(whd_driver_t whd_driver, uint32_t mem_offset)
{
    uint32_t address = mem_offset;

    return REG16(TRANS_ADDR(address) );
}

static void whd_write_tcm16(whd_driver_t whd_driver, uint32_t mem_offset, uint16_t value)
{
    uint32_t address = mem_offset;

    REG16(TRANS_ADDR(address) ) = value;
}

static void whd_write_tcm32(whd_driver_t whd_driver, uint32_t mem_offset, uint32_t value)
{
    uint32_t address = mem_offset;

    REG32(TRANS_ADDR(address) ) = value;
}

void whd_bus_handle_mb_data(whd_driver_t whd_driver, uint32_t d2h_mb_data)
{

    WPRINT_WHD_DEBUG( ("D2H_MB_DATA: 0x%04x\n", (unsigned int)d2h_mb_data) );

    if (d2h_mb_data & WHD_D2H_DEV_DS_ENTER_REQ)
    {
        WPRINT_WHD_DEBUG( ("D2H_MB_DATA: DEEP SLEEP REQ\n") );
    }

    if (d2h_mb_data & WHD_D2H_DEV_DS_EXIT_NOTE)
        WPRINT_WHD_DEBUG( ("D2H_MB_DATA: DEEP SLEEP EXIT\n") );

    if (d2h_mb_data & WHD_D2H_DEV_D3_ACK)
    {
        WPRINT_WHD_DEBUG( ("D2H_MB_DATA: D3 ACK\n") );
        whd_driver->ack_d2h_suspend = 1;
    }

    if (d2h_mb_data & WHD_D2H_DEV_FWHALT)
    {
        WPRINT_WHD_DEBUG( ("D2H_MB_DATA: FW HALT\n") );
        //whd_fw_crashed(&devinfo->pdev->dev);
    }

}

whd_result_t whd_bus_suspend(whd_driver_t whd_driver)
{

    if (whd_bus_is_up(whd_driver) == WHD_FALSE)
    {
        WPRINT_WHD_DEBUG( ("Bus is already in SUSPEND state.\n") );
        return WHD_SUCCESS;
    }

    whd_bus_set_state(whd_driver, WHD_FALSE);

#ifndef GCI_SECURE_ACCESS
    /* Disable WLAN force HT clock, if running */
    CHECK_RETURN_IGNORE(whd_bus_write_backplane_value(whd_driver, (uint32_t)GCI_BT2WL_CLOCK_CSR, 4, 0) );
#endif

    return WHD_SUCCESS;
}

whd_result_t whd_bus_resume(whd_driver_t whd_driver)
{
    whd_result_t result = WHD_SUCCESS;

    /* Check register “BT2WL Clock Request and Status Register (Offset 0x6A4)”
     * if ALP or HT not available on WLAN backplane then set ALPAvailRequest (AQ) before accessing the TCM.
     * Make sure ALPClockAvailable (AA) is available before accessing the TCM. */
#ifndef GCI_SECURE_ACCESS
    uint32_t csr = 0;
    uint32_t attempts = 0;

    while ( ( (result = whd_bus_read_backplane_value(whd_driver, GCI_BT2WL_CLOCK_CSR,
                                                     (uint8_t)sizeof(csr), (uint8_t *)&csr) ) == WHD_SUCCESS ) &&
            ( (csr & (GCI_ALP_AVAIL | GCI_HT_AVAIL) ) == 0 ) &&
            (attempts < (uint32_t)WLAN_BUS_UP_ATTEMPTS) )
    {
        CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)GCI_BT2WL_CLOCK_CSR,
                                                   (uint8_t)4, (uint32_t)GCI_ALP_AVAIL_REQ) );
        (void)cy_rtos_delay_milliseconds( (uint32_t)HT_AVAIL_WAIT_MS );   /* Ignore return - nothing can be done if it fails */
        attempts++;
    }
#endif

    /* Host trigger suspend or not  */
    if (whd_driver->ack_d2h_suspend == WHD_TRUE)
    {
        whd_driver->ack_d2h_suspend = WHD_FALSE;
        /* If Resume from Host, send H1D DB1 to "WLAN FW" */
        WPRINT_WHD_INFO( ("Notify Firmware about HOST READY!!! \n") );
#ifndef GCI_SECURE_ACCESS
        result = whd_bus_write_backplane_value(whd_driver, (uint32_t)GCI_BT2WL_DB1_REG, 4, WHD_H2D_INFORM_HOSTRDY);
#else
        result = whd_hw_generateBt2WlDbInterruptApi(1, WHD_H2D_INFORM_HOSTRDY);
#endif
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("whd_bus_write_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        }
    }

    return WHD_SUCCESS;
}

static struct whd_ringbuf *whd_allocate_ring_and_handle(whd_driver_t whd_driver, uint32_t ring_id,
                                                        uint32_t tcm_ring_phys_addr)
{
    uint32_t* ring_handle = NULL;
    struct whd_ringbuf *ring;
    uint32_t size;
    uint32_t addr;
    uint32_t ring_addr;
    const uint32_t *ring_itemsize_array;

    if (whd_driver->ram_shared->version_new < WHD_PCIE_SHARED_VERSION_7)
        ring_itemsize_array = whd_ring_itemsize_pre_v7;
    else
        ring_itemsize_array = whd_ring_itemsize;

    size = whd_ring_max_item[ring_id] * ring_itemsize_array[ring_id];

    WPRINT_WHD_DEBUG( ("Allocate Ring Handle: %s\n", __func__) );
    ring_handle = whd_dmapool_alloc(size);

    if(ring_handle == NULL)
        return NULL;

    ring_addr = (uint32_t)ring_handle;

    WPRINT_WHD_DEBUG( ("tcm_ring_phys_addr is 0x%lx, ring_addr is 0x%lx, ring_handle is 0x%lx \n", tcm_ring_phys_addr,
                       ring_addr, (uint32_t)ring_handle) );

    whd_write_tcm32(whd_driver, tcm_ring_phys_addr + WHD_RING_MEM_BASE_ADDR_OFFSET, dtoh32(ring_addr & 0xffffffff) );
    whd_write_tcm32(whd_driver, tcm_ring_phys_addr + WHD_RING_MEM_BASE_ADDR_OFFSET + 4, 0);

    WPRINT_WHD_DEBUG( ("%s : Ring Handle Address For ring id[%lu] is 0x%lx\n", __func__, ring_id,
                       (uint32_t)ring_handle) );

    WPRINT_WHD_DEBUG( ("Read the value at ringmem ptr[0x%lx] at TCM - 0x%lx \n",
                       (tcm_ring_phys_addr + WHD_RING_MEM_BASE_ADDR_OFFSET),
                       REG32(TRANS_ADDR(tcm_ring_phys_addr + WHD_RING_MEM_BASE_ADDR_OFFSET) ) ) );

    addr = tcm_ring_phys_addr + WHD_RING_MAX_ITEM_OFFSET;
    whd_write_tcm16(whd_driver, addr, whd_ring_max_item[ring_id]);
    addr = tcm_ring_phys_addr + WHD_RING_LEN_ITEMS_OFFSET;
    whd_write_tcm16(whd_driver, addr, ring_itemsize_array[ring_id]);

    ring = whd_mem_malloc(sizeof(*ring) );

    if (!ring)
    {
        return NULL;
    }
    whd_mem_memset(ring, 0, sizeof(*ring) );
    whd_commonring_config(&ring->commonring, whd_ring_max_item[ring_id],
                          ring_itemsize_array[ring_id], (void*)ring_handle);

    ring->ring_handle = ring_handle;
    ring->whd_drv = whd_driver;

    whd_commonring_register_cb(&ring->commonring,
                               whd_ring_mb_ring_bell,
                               whd_ring_mb_update_rptr,
                               whd_ring_mb_update_wptr,
                               whd_ring_mb_write_rptr,
                               whd_ring_mb_write_wptr, ring);

    return (ring);
}

static whd_result_t whd_release_ringbuffer(whd_driver_t whd_driver,
                                   struct whd_ringbuf *ring)
{
    void *dma_buf;

    if (!ring)
        return WHD_BUFFER_ALLOC_FAIL;

    dma_buf = ring->commonring.buf_addr;
    if (dma_buf)
    {
        whd_mem_free(ring->ring_handle);
    }
    whd_mem_free(ring);

    return WHD_SUCCESS;
}

static void whd_release_ringbuffers(whd_driver_t whd_driver)
{
    uint32_t i;

    for (i = 0; i < WHD_NROF_COMMON_MSGRINGS; i++)
    {
        whd_release_ringbuffer(whd_driver, whd_driver->ram_shared->commonrings[i]);
        whd_driver->ram_shared->commonrings[i] = NULL;
    }
    whd_mem_free(whd_driver->ram_shared->flowrings);
    whd_driver->ram_shared->flowrings = NULL;
}

uint32_t whd_bus_m2m_ring_init(whd_driver_t whd_driver)
{
    struct whd_dhi_ringinfo ringinfo;
    whd_result_t result = WHD_SUCCESS;
    struct whd_ringbuf *ring;
    struct whd_ringbuf *rings;
    uint32_t d2h_w_idx_ptr = 0;
    uint32_t d2h_r_idx_ptr = 0;
    uint32_t h2d_w_idx_ptr = 0;
    uint32_t h2d_r_idx_ptr = 0;
    uint32_t ring_mem_ptr = 0;
    uint32_t i;
    uint8_t idx_offset;
    uint16_t max_flowrings;
    uint16_t max_submissionrings;
    uint16_t max_completionrings;

    result = whd_bus_mem_bytes(whd_driver, BUS_READ, TRANS_ADDR(whd_driver->ram_shared->ring_info_addr),
                               sizeof(ringinfo), (uint8_t *)&ringinfo);
    if (result != 0)
    {
        WPRINT_WHD_ERROR( ("%s: Read Ring Info failed\n", __func__) );
        return result;
    }

    if (whd_driver->ram_shared->version_new >= 6)
    {
        max_submissionrings = dtoh16(ringinfo.max_submissionrings);
        max_flowrings = dtoh16(ringinfo.max_flowrings);
        max_completionrings = dtoh16(ringinfo.max_completionrings);
    }
    else
    {
        max_submissionrings = dtoh16(ringinfo.max_flowrings);
        max_flowrings = max_submissionrings - WHD_NROF_H2D_COMMON_MSGRINGS;
        max_completionrings = WHD_NROF_D2H_COMMON_MSGRINGS;
    }

    if (max_flowrings > 256)
    {
        WPRINT_WHD_ERROR( ("Invalid max_flowrings(%d)\n", max_flowrings) );
        return WHD_WLAN_BADARG;
    }

    if (whd_driver->dma_index_sz == 0)
    {
        d2h_w_idx_ptr = dtoh32(ringinfo.d2h_w_idx_ptr);
        d2h_r_idx_ptr = dtoh32(ringinfo.d2h_r_idx_ptr);
        h2d_w_idx_ptr = dtoh32(ringinfo.h2d_w_idx_ptr);
        h2d_r_idx_ptr = dtoh32(ringinfo.h2d_r_idx_ptr);
        idx_offset = sizeof(uint32_t);
        whd_driver->write_ptr = whd_write_tcm16;
        whd_driver->read_ptr = whd_read_tcm16;
        WPRINT_WHD_DEBUG( ("Using TCM indices\n") );
        WPRINT_WHD_DEBUG( ("d2h_w_idx_ptr - 0x%lx, d2h_r_idx_ptr - 0x%lx \n", d2h_w_idx_ptr, d2h_r_idx_ptr) );
        WPRINT_WHD_DEBUG( ("h2d_w_idx_ptr - 0x%lx, h2d_r_idx_ptr - 0x%lx \n", h2d_w_idx_ptr, h2d_r_idx_ptr) );
    }
    else
    {
        WPRINT_WHD_ERROR( ("Host Indices Support is NOT IMPLEMENTED!!! \n") );
    }

    ring_mem_ptr = dtoh32(ringinfo.ringmem);

    for (i = 0; i < WHD_NROF_H2D_COMMON_MSGRINGS; i++)
    {
        ring = whd_allocate_ring_and_handle(whd_driver, i, ring_mem_ptr);
        if (!ring)
            goto fail;
        ring->w_idx_addr = h2d_w_idx_ptr;
        ring->r_idx_addr = h2d_r_idx_ptr;
        ring->id = i;
        whd_driver->ram_shared->commonrings[i] = ring;

        h2d_w_idx_ptr += idx_offset;
        h2d_r_idx_ptr += idx_offset;
        ring_mem_ptr += WHD_RING_MEM_SZ;
    }

    for (i = WHD_NROF_H2D_COMMON_MSGRINGS; i < WHD_NROF_COMMON_MSGRINGS; i++)
    {
        ring = whd_allocate_ring_and_handle(whd_driver, i, ring_mem_ptr);
        if (!ring)
            goto fail;
        ring->w_idx_addr = d2h_w_idx_ptr;
        ring->r_idx_addr = d2h_r_idx_ptr;
        ring->id = i;
        whd_driver->ram_shared->commonrings[i] = ring;

        d2h_w_idx_ptr += idx_offset;
        d2h_r_idx_ptr += idx_offset;
        ring_mem_ptr += WHD_RING_MEM_SZ;
    }

#if 0    /* Currently WLAN FW is returning the max flowring req count as 40, which needs to be fixed */
    whd_driver->ram_shared->max_flowrings = max_flowrings;
#else
    whd_driver->ram_shared->max_flowrings = 5;    /* This will be increased for concurrent mode later */
#endif

    whd_driver->ram_shared->max_submissionrings = max_submissionrings;
    whd_driver->ram_shared->max_completionrings = max_completionrings;
    rings = whd_mem_malloc(max_flowrings * sizeof(*ring) );

    if (!rings)
        goto fail;

    WPRINT_WHD_DEBUG( ("Number of flowrings is %d\n", max_flowrings) );

    for (i = 0; i < max_flowrings; i++)
    {
        ring = &rings[i];
        ring->whd_drv = whd_driver;
        ring->id = i + WHD_H2D_MSGRING_FLOWRING_IDSTART;
        whd_commonring_register_cb(&ring->commonring,
                                   whd_ring_mb_ring_bell,
                                   whd_ring_mb_update_rptr,
                                   whd_ring_mb_update_wptr,
                                   whd_ring_mb_write_rptr,
                                   whd_ring_mb_write_wptr,
                                   ring);

        ring->w_idx_addr = h2d_w_idx_ptr;
        ring->r_idx_addr = h2d_r_idx_ptr;
        h2d_w_idx_ptr += idx_offset;
        h2d_r_idx_ptr += idx_offset;
    }
    whd_driver->ram_shared->flowrings = rings;

    WPRINT_WHD_DEBUG( ("CommonRings Init Done \n") );

    WPRINT_WHD_DEBUG( ("Notify Firmware about HOST READY!!! \n") );
#ifndef GCI_SECURE_ACCESS
    result = whd_bus_write_backplane_value(whd_driver, (uint32_t)GCI_BT2WL_DB1_REG, 4, WHD_H2D_INFORM_HOSTRDY);
#else
    result = whd_hw_generateBt2WlDbInterruptApi(1, WHD_H2D_INFORM_HOSTRDY);
#endif
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_write_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    return result;

fail:
    WPRINT_WHD_ERROR( ("Allocating ring buffers failed\n") );
    whd_release_ringbuffers(whd_driver);
    return WHD_WLAN_ERROR;

}

whd_result_t whd_bus_m2m_sharedmem_init(whd_driver_t whd_driver)
{
    uint32_t wlan_shared_address = 0;
    whd_result_t result = WHD_SUCCESS;
    uint32_t shared_addr = 0;
    uint32_t addr = 0;
    uint32_t host_capability = 0;
    whd_internal_info_t internal_info;

    struct whd_ram_shared_info *ram_shared_info;

    ram_shared_info = (struct whd_ram_shared_info *)whd_mem_malloc(sizeof(struct whd_ram_shared_info) );
    if (!ram_shared_info)
    {
        return WHD_MALLOC_FAILURE;
    }
    whd_mem_memset(ram_shared_info, 0, sizeof(struct whd_ram_shared_info));

    whd_driver->ram_shared = ram_shared_info;

    wlan_shared_address = GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS) + GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4;

    WPRINT_WHD_DEBUG( ("%s: WLAN Shared Area Space is 0x%lx\n", __func__, wlan_shared_address) );

    WPRINT_WHD_DEBUG( ("Waiting for FW to update Shared Area\n") );

    while ( (shared_addr == 0) || (shared_addr <= GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS) ) ||
            (shared_addr >= (GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS) + GET_C_VAR(whd_driver, CHIP_RAM_SIZE) ) ) )
    {
        result = whd_bus_read_backplane_value(whd_driver, wlan_shared_address, 4, (uint8_t *)&shared_addr);
    }

    WPRINT_WHD_DEBUG( ("%s: WLAN Shared Address is 0x%lx\n", __func__,  shared_addr) );

    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_DEBUG( ("%s: Shared Address Read Failed\n", __func__) );
        goto fail;
    }

    result = whd_bus_mem_bytes(whd_driver, BUS_READ, TRANS_ADDR(
                                   shared_addr), sizeof(internal_info.sh), (uint8_t *)&internal_info.sh);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("%s: Read Internal Info failed\n", __func__) );
        goto fail;
    }

    internal_info.sh.flags = dtoh32(internal_info.sh.flags);
    internal_info.sh.trap_addr = dtoh32(internal_info.sh.trap_addr);
    internal_info.sh.assert_exp_addr = dtoh32(internal_info.sh.assert_exp_addr);
    internal_info.sh.assert_file_addr = dtoh32(internal_info.sh.assert_file_addr);
    internal_info.sh.assert_line = dtoh32(internal_info.sh.assert_line);
    internal_info.sh.console_addr = dtoh32(internal_info.sh.console_addr);
    internal_info.sh.msgtrace_addr = dtoh32(internal_info.sh.msgtrace_addr);
    internal_info.sh.ring_info_ptr = dtoh32(internal_info.sh.ring_info_ptr);
    internal_info.sh.fwid = dtoh32(internal_info.sh.fwid);

    host_capability = (dtoh32(internal_info.sh.flags) & WHD_PCIE_SHARED_VERSION_MASK);

    if ( (internal_info.sh.flags & WLAN_M2M_SHARED_VERSION_MASK) > WLAN_M2M_SHARED_VERSION )
    {
        WPRINT_WHD_ERROR( ("ReadShared: WLAN shared version is not valid sh.flags %x\n\r", internal_info.sh.flags) );
        goto fail;
    }

    WPRINT_WHD_DEBUG( ("FW Supported Flag Value is 0x%x \n", internal_info.sh.flags) );

    /* Disable oob interrupt base for Hatchet-1 CP */
    host_capability = host_capability | WHD_SHARED_HOST_CAP_NO_OOB;

    if ( (internal_info.sh.flags & WHD_PCIE_SHARED_HOSTRDY_DB1) )
    {
        WPRINT_WHD_DEBUG( ("HOST READY supported by dongle\n") );
        host_capability = host_capability | WHD_H2D_ENABLE_HOSTRDY;
    }

    if ( (internal_info.sh.flags & WHD_PCIE_SHARED_USE_MAILBOX) )
    {
        WPRINT_WHD_DEBUG( ("Shared Mailbox supported by dongle\n") );
        host_capability = host_capability | WHD_PCIE_SHARED_USE_MAILBOX;
    }

    /* check firmware support dma indicies */
    if (internal_info.sh.flags & WHD_PCIE_SHARED_DMA_INDEX)
    {
        if (internal_info.sh.flags & WHD_PCIE_SHARED_DMA_2B_IDX)
            whd_driver->dma_index_sz = sizeof(uint16_t);
        else
            whd_driver->dma_index_sz = sizeof(uint32_t);
    }

    whd_driver->ram_shared->tcm_base_address = shared_addr;
    whd_driver->ram_shared->version_new = internal_info.sh.flags & WLAN_M2M_SHARED_VERSION_MASK;

#if 0	/* To be verified after TO for alignment issue */
    addr = shared_addr + WHD_SHARED_MAX_RXBUFPOST_OFFSET;
    result = whd_bus_read_backplane_value(whd_driver, addr, 4,
                                          (uint8_t *)&whd_driver->ram_shared->max_rxbufpost);

    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_read_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }
#endif

    if (whd_driver->ram_shared->max_rxbufpost == 0)
        whd_driver->ram_shared->max_rxbufpost = WHD_DEF_MAX_RXBUFPOST;

    addr = shared_addr + WHD_SHARED_RX_DATAOFFSET_OFFSET;
    result = whd_bus_read_backplane_value(whd_driver, addr, 4,
                                          (uint8_t *)&whd_driver->ram_shared->rx_dataoffset);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_read_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

#if 0	/* To be verified after TO for alignment issue */
    addr = shared_addr + WHD_SHARED_HTOD_MB_DATA_ADDR_OFFSET;
    result = whd_bus_read_backplane_value(whd_driver, TRANS_ADDR(addr), 4,
                                          (uint8_t *)&whd_driver->ram_shared->htod_mb_data_addr);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_read_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    addr = shared_addr + WHD_SHARED_DTOH_MB_DATA_ADDR_OFFSET;
    result = whd_bus_read_backplane_value(whd_driver, addr, 4,
                                          (uint8_t *)&whd_driver->ram_shared->dtoh_mb_data_addr);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_read_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }
#endif

    addr = shared_addr + WHD_SHARED_RING_INFO_ADDR_OFFSET;
    result = whd_bus_read_backplane_value(whd_driver, addr, 4,
                                          (uint8_t *)&whd_driver->ram_shared->ring_info_addr);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_read_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    addr = shared_addr + WHD_SHARED_HOST_CAP_OFFSET;
    result = whd_bus_write_backplane_value(whd_driver, addr, 4, host_capability);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_write_backplane_value failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    WPRINT_WHD_DEBUG( ("Firmware ID is 0x%x\n", internal_info.sh.fwid) );
    WPRINT_WHD_DEBUG( ("RingInfo Pointer is 0x%lx \n", whd_driver->ram_shared->ring_info_addr) );

    result = whd_bus_m2m_ring_init(whd_driver);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("whd_bus_m2m_ring_init failed in %s at %d \n", __func__, __LINE__) );
        goto fail;
    }

    /* Create the mutex protecting host suspend */
    if (cy_rtos_init_semaphore(&whd_driver->host_suspend_mutex, 1, 0) != WHD_SUCCESS)
    {
        return WHD_SEMAPHORE_ERROR;
    }
    if (cy_rtos_set_semaphore(&whd_driver->host_suspend_mutex, WHD_FALSE) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
        return WHD_SEMAPHORE_ERROR;
    }

    return result;

fail:
    WPRINT_WHD_DEBUG( ("%s, Initial whd_bus_m2m_sharedmem_init failed\n", __FUNCTION__) );
    whd_mem_free(ram_shared_info);
    whd_driver->ram_shared = NULL;
    return result;
}

#endif /* PROTO_MSGBUF */
