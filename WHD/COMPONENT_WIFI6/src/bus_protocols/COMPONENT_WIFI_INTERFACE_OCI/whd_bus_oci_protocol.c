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

/** @file
 *  Broadcom WLAN OCI(On Chip Interconnect) Protocol interface
 *
 *  Implements the WHD Bus Protocol Interface for CUSTOM
 *  Provides functions for initialising, de-intitialising 802.11 device,
 *  sending/receiving raw packets etc
 */

#include "cybsp.h"
#if defined(COMPONENT_WIFI_INTERFACE_OCI)
#include "whd_oci.h"
#include "bus_protocols/whd_bus.h"

#include "whd_chip_constants.h"
#include "whd_buffer_api.h"
#include "whd_types_int.h"
#include "whd_thread_internal.h"
#include "whd_resource_if.h"
#include "whd_wlioctl.h"
#include "whd_proto.h"
#include "whd_ring.h"

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
#include "cyhal_syspm.h"
#include "cy_wcm.h"
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

#ifdef GCI_SECURE_ACCESS
#include "whd_hw.h"
#endif

/******************************************************
*             Constants
******************************************************/

#define WHD_BUS_OCI_BACKPLANE_READ_PADD_SIZE    	(0)
#define WHD_BUS_OCI_MAX_BACKPLANE_TRANSFER_SIZE 	(WHD_PAYLOAD_MTU)

#define ARMCR4_SW_INT0                      (0x1 << 0)

#if !defined (__IAR_SYSTEMS_ICC__)
/* assume registers are Device memory, so have implicit CPU memory barriers */
#define MEMORY_BARRIER_AGAINST_COMPILER_REORDERING()  __asm__ __volatile__ ("" : : : "memory")
#define REGISTER_WRITE_WITH_BARRIER(type, address, value) \
    do {*(volatile type *)(address) = (type)(value); \
        MEMORY_BARRIER_AGAINST_COMPILER_REORDERING();} while (0)
#define REGISTER_READ(type, address) \
    (*(volatile type *)(address) )
#endif

#define WHD_THREAD_POLL_TIMEOUT      (CY_RTOS_NEVER_TIMEOUT)

/******************************************************
*             Structures
******************************************************/
struct whd_bus_priv
{
    whd_oci_config_t oci_config;
};

/******************************************************
*             Variables
******************************************************/
static struct whd_bus_info whd_bus_oci_info;
static struct whd_bus_priv whd_bus_priv;

/******************************************************
*             Function declarations
******************************************************/
#ifdef GCI_SECURE_ACCESS
static BOOL32 whd_bus_oci_gci_interrupt_handler(void * callback_arg, uint32_t db_status_mask, uint32_t *db_values, uint8_t len);
#endif
static whd_result_t whd_bus_oci_init(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_deinit(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus);
static whd_result_t whd_bus_oci_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer);

static whd_bool_t whd_bus_oci_wake_interrupt_present(whd_driver_t whd_driver);
static uint32_t whd_bus_oci_packet_available_to_read(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer);

static whd_result_t whd_bus_oci_write_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                      uint8_t register_length, uint32_t value);
static whd_result_t whd_bus_oci_read_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                                     uint8_t *value);
static whd_result_t whd_bus_oci_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                     uint32_t address, uint8_t value_length, uint32_t value);
static whd_result_t whd_bus_oci_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                    uint32_t address, uint8_t value_length, uint8_t *value);
static whd_result_t whd_bus_oci_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                               whd_bus_function_t function, uint32_t address, uint16_t size,
                                               whd_transfer_bytes_packet_t *data);

static whd_result_t whd_bus_oci_poke_wlan(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_wakeup(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_sleep(whd_driver_t whd_driver);
static uint8_t whd_bus_oci_backplane_read_padd_size(whd_driver_t whd_driver);
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
static whd_result_t whd_bus_oci_sleep_allow_decider(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore, uint32_t timeout_ms);
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
static whd_result_t whd_bus_oci_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore);
static whd_bool_t whd_bus_oci_use_status_report_scheme(whd_driver_t whd_driver);
static uint32_t whd_bus_oci_get_max_transfer_size(whd_driver_t whd_driver);
static void whd_bus_oci_init_stats(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print);
static whd_result_t whd_bus_oci_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware);
static whd_result_t whd_bus_oci_irq_register(whd_driver_t whd_driver);
static whd_result_t whd_bus_oci_irq_enable(whd_driver_t whd_driver, whd_bool_t enable);
#ifdef BLHS_SUPPORT
static whd_result_t whd_bus_oci_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage);
#endif
static whd_result_t whd_bus_oci_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address, uint32_t image_size);
static whd_result_t whd_bus_oci_write_wifi_nvram_image(whd_driver_t whd_driver);

whd_bool_t whd_ensure_wlan_is_up(whd_driver_t whd_driver);
whd_result_t whd_oci_bus_write_wifi_firmware_image(whd_driver_t whd_driver);

/******************************************************
*             Function definitions
******************************************************/

// Functions for whd_driver->bus_if function list
whd_result_t whd_bus_oci_attach(whd_driver_t whd_driver, whd_oci_config_t *whd_oci_config )
{
    WPRINT_WHD_INFO( ("oci_attach\n") );

    whd_driver->bus_if   = &whd_bus_oci_info;
    whd_driver->bus_priv = &whd_bus_priv;

    whd_mem_memset(whd_driver->bus_if, 0, sizeof(whd_bus_info_t) );
    whd_mem_memset(whd_driver->bus_priv, 0, sizeof(struct whd_bus_priv) );

    whd_driver->bus_priv->oci_config = *whd_oci_config;

#ifndef PROTO_MSGBUF
    whd_driver->proto_type = WHD_PROTO_BCDC;
#else
    whd_driver->proto_type = WHD_PROTO_MSGBUF;
#endif /* PROTO_MSGBUF */

    whd_driver->bus_if->whd_bus_init_fptr = whd_bus_oci_init;
    whd_driver->bus_if->whd_bus_deinit_fptr = whd_bus_oci_deinit;

    whd_driver->bus_if->whd_bus_ack_interrupt_fptr = whd_bus_oci_ack_interrupt;
    whd_driver->bus_if->whd_bus_send_buffer_fptr = whd_bus_oci_send_buffer;

    whd_driver->bus_if->whd_bus_wake_interrupt_present_fptr = whd_bus_oci_wake_interrupt_present;
    whd_driver->bus_if->whd_bus_packet_available_to_read_fptr = whd_bus_oci_packet_available_to_read;
    whd_driver->bus_if->whd_bus_read_frame_fptr = whd_bus_oci_read_frame;


    whd_driver->bus_if->whd_bus_write_backplane_value_fptr = whd_bus_oci_write_backplane_value;
    whd_driver->bus_if->whd_bus_read_backplane_value_fptr = whd_bus_oci_read_backplane_value;

    whd_driver->bus_if->whd_bus_write_register_value_fptr = whd_bus_oci_write_register_value;
    whd_driver->bus_if->whd_bus_read_register_value_fptr = whd_bus_oci_read_register_value;

    whd_driver->bus_if->whd_bus_transfer_bytes_fptr = whd_bus_oci_transfer_bytes;

    whd_driver->bus_if->whd_bus_poke_wlan_fptr = whd_bus_oci_poke_wlan;

    whd_driver->bus_if->whd_bus_wakeup_fptr = whd_bus_oci_wakeup;
    whd_driver->bus_if->whd_bus_sleep_fptr = whd_bus_oci_sleep;

    whd_driver->bus_if->whd_bus_backplane_read_padd_size_fptr = whd_bus_oci_backplane_read_padd_size;

    whd_driver->bus_if->whd_bus_wait_for_wlan_event_fptr = whd_bus_oci_wait_for_wlan_event;
    whd_driver->bus_if->whd_bus_use_status_report_scheme_fptr = whd_bus_oci_use_status_report_scheme;

    whd_driver->bus_if->whd_bus_get_max_transfer_size_fptr = whd_bus_oci_get_max_transfer_size;

    whd_driver->bus_if->whd_bus_init_stats_fptr = whd_bus_oci_init_stats;
    whd_driver->bus_if->whd_bus_print_stats_fptr = whd_bus_oci_print_stats;
    whd_driver->bus_if->whd_bus_reinit_stats_fptr = whd_bus_oci_reinit_stats;
    whd_driver->bus_if->whd_bus_irq_register_fptr = whd_bus_oci_irq_register;
    whd_driver->bus_if->whd_bus_irq_enable_fptr = whd_bus_oci_irq_enable;
    whd_driver->bus_if->whd_bus_download_resource_fptr = whd_bus_oci_download_resource;

#ifdef BLHS_SUPPORT
    whd_driver->bus_if->whd_bus_blhs_fptr = whd_bus_oci_blhs;
#endif /* BLHS_SUPPORT */
    return WHD_SUCCESS;
}

void whd_bus_oci_detach(whd_driver_t whd_driver)
{
    whd_driver->bus_if = NULL;
    whd_driver->bus_priv = NULL;
}

#ifdef GCI_SECURE_ACCESS
static BOOL32 whd_bus_oci_gci_interrupt_handler(void * callback_arg, uint32_t db_status_mask, uint32_t *db_values, uint8_t len)
{
    whd_driver_t whd_driver = (whd_driver_t)callback_arg;

    if(db_status_mask & (GCI_H2D_SET_BIT_DB1 | GCI_H2D_SET_BIT_DB0))
    {
        /* call thread notify to wake up WHD thread */
        whd_thread_notify_irq(whd_driver);
    }
    else
    {
        //WPRINT_WHD_DEBUG(("Unhandled DB Interrupt - %d\n", (uint32_t)db_status_mask));
        return 1;
    }

    return 0;
}
#endif

static whd_result_t whd_bus_oci_init(whd_driver_t whd_driver)
{
    whd_result_t result = WHD_SUCCESS;

#ifdef GCI_SECURE_ACCESS
    uint16_t Chip_ID;

    /* WLAN Out of Reset sequence for H1-CP */
    whd_hw_initApi();
    whd_hw_wlanAssertResetApi();
    whd_hw_wlanResetControlOverrideApi(1);
    cyhal_system_delay_ms(500);
    whd_hw_wlanDeassertResetApi();
    cyhal_system_delay_ms(500);

    Chip_ID = whd_hw_readGciChipIdRegisterApi();
    whd_chip_set_chip_id(whd_driver, Chip_ID);
#endif

    /* Download wlan firmware */
    result = whd_oci_bus_write_wifi_firmware_image(whd_driver);
    if ( (result == WHD_UNFINISHED) || (result != WHD_SUCCESS) )
    {
        /* for user abort, then put wifi module into known good state */
        return result;
    }

    result = whd_bus_oci_write_wifi_nvram_image(whd_driver);
    if ( (result == WHD_UNFINISHED) || (result != WHD_SUCCESS) )
    {
        /* for user abort, then put wifi module into known good state */
        return result;
    }

#ifdef GCI_SECURE_ACCESS
    /* Enable WL2BT Interrupts Mask */
    whd_hw_wl2BtDbEnableInterruptsApi(GCI_H2D_SET_ALL_DB);
    /* Register Interrupt Callback */
    whd_hw_registerWl2BtInterruptCallbackApi(whd_bus_oci_gci_interrupt_handler, whd_driver);
#endif

#ifdef PROTO_MSGBUF
    CHECK_RETURN(whd_bus_m2m_sharedmem_init(whd_driver) );
#endif

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_deinit(whd_driver_t whd_driver)
{
    //TBD

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus)
{
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_result_t result = WHD_SUCCESS;

    //TBD

    return result;
}

static whd_bool_t whd_bus_oci_wake_interrupt_present(whd_driver_t whd_driver)
{
    /* functionality is only currently needed and present on SDIO */
    return WHD_FALSE;
}

static uint32_t whd_bus_oci_packet_available_to_read(whd_driver_t whd_driver)
{
    return 1;
}

static whd_result_t whd_bus_oci_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer)
{
    whd_result_t result = WHD_SUCCESS;

    //TBD

    return result;
}

static whd_result_t whd_bus_oci_write_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                      uint8_t register_length, uint32_t value)
{
    if (register_length == 4)
    {
#ifdef PROTO_MSGBUF
       REG32(TRANS_ADDR(address)) = ((uint32_t)value);
#else
       REG32(address) = ((uint32_t)value);
#endif
    }
    else if (register_length == 2)
    {
#ifdef PROTO_MSGBUF
        REG16(TRANS_ADDR(address)) = ((uint16_t)value);
#else
        REG16(address) = ((uint16_t)value);
#endif
    }
    else if (register_length == 1)
    {
#ifdef PROTO_MSGBUF
        REG8(TRANS_ADDR(address)) = ((uint8_t)value);
#else
        REG8(address) = ((uint8_t)value);
#endif
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_read_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                     uint8_t register_length, /*@out@*/ uint8_t *value)
{
    if (register_length == 4)
    {
#ifdef PROTO_MSGBUF
        *((uint32_t*)value) = REG32(TRANS_ADDR(address));
#else
        *((uint32_t*)value) = REG32(address);
#endif
    }
    else if (register_length == 2)
    {
#ifdef PROTO_MSGBUF
        *((uint16_t*)value) = REG16(TRANS_ADDR(address));
#else
        *((uint16_t*)value) = REG16(address);
#endif
    }
    else if (register_length == 1)
    {
#ifdef PROTO_MSGBUF
        *value = REG8(TRANS_ADDR(address));
#else
        *((uint8_t*)value) = REG8(address);
#endif
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                     uint32_t address, uint8_t value_length, uint32_t value)
{
    // Not used by OCI bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                    uint32_t address, uint8_t value_length, uint8_t *value)
{
    // Not used by OCI bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                               whd_bus_function_t function, uint32_t address, uint16_t size,
                                               whd_transfer_bytes_packet_t *data)
{
    if (function != BACKPLANE_FUNCTION)
    {
        return WHD_DOES_NOT_EXIST;
    }

    if (direction == BUS_WRITE)
    {
        whd_mem_memcpy((void *)address, (const void *)data, size);
    }
    else
    {
        whd_mem_memcpy((void *)data, (const void *)address, size);
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_poke_wlan(whd_driver_t whd_driver)
{
    // Not used by OCI bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_wakeup(whd_driver_t whd_driver)
{
    WPRINT_WHD_INFO( ("bus_oci_wakeup\n") );
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_sleep(whd_driver_t whd_driver)
{
    WPRINT_WHD_INFO( ("bus_oci_sleep\n") );
    return WHD_SUCCESS;
}

static uint8_t whd_bus_oci_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return WHD_BUS_OCI_BACKPLANE_READ_PADD_SIZE;
}

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
static whd_result_t whd_bus_oci_sleep_allow_decider(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore, uint32_t timeout_ms)
{
    whd_result_t result = 0;

    if (whd_driver->ack_d2h_suspend == WHD_TRUE)
    {
        CHECK_RETURN(whd_bus_suspend(whd_driver));
        whd_driver->pds_sleep_allow = WHD_TRUE;
        WPRINT_WHD_DEBUG(("***SLEEP ALLOW*** \n"));
        whd_pds_unlock_sleep(whd_driver);
        /* If it comes here means, timeout aleady happened and WLAN is in D3 state,
           so return CY_RTOS_TIMEOUT to wait forever for any activity */
        return CY_RTOS_TIMEOUT;
    }
    else
    {
        whd_driver->pds_sleep_allow = WHD_FALSE;
        whd_pds_lock_sleep(whd_driver);
    }

    result = cy_rtos_get_semaphore(transceive_semaphore, timeout_ms, WHD_FALSE);

    if (result == CY_RTOS_TIMEOUT)
    {
        CHECK_RETURN(whd_msgbuf_send_mbdata(whd_driver, WHD_H2D_HOST_D3_INFORM));
    }

    return result;
}
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

static whd_result_t whd_bus_oci_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t timeout_ms = 0;

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
    timeout_ms = WHD_MSGBUF_SLP_DETECT_TIME;

    result = whd_bus_oci_sleep_allow_decider(whd_driver, transceive_semaphore, timeout_ms);
    if (result == CY_RTOS_TIMEOUT)
    {
        /* Here the timeout indiactes, no activity detected for this time(WHD_MSGBUF_SLP_DETECT_TIME),
           so D3 suspend is done and now wait on infinite timeout for any interrupt reception/activity */
        result = cy_rtos_get_semaphore(transceive_semaphore, CY_RTOS_NEVER_TIMEOUT, WHD_FALSE);
    }
#else
    uint32_t delayed_release_timeout_ms = 0;
    delayed_release_timeout_ms = whd_bus_handle_delayed_release(whd_driver);
    if (delayed_release_timeout_ms != 0)
    {
        timeout_ms = delayed_release_timeout_ms;
    }
    else
    {
        timeout_ms = CY_RTOS_NEVER_TIMEOUT;
    }

    result = cy_rtos_get_semaphore(transceive_semaphore, (uint32_t)MIN_OF(timeout_ms,
                                                                          WHD_THREAD_POLL_TIMEOUT), WHD_FALSE);
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

    return result;
}

static whd_bool_t whd_bus_oci_use_status_report_scheme(whd_driver_t whd_driver)
{
    /* !OCI_RX_POLL_MODE */
    return WHD_FALSE;
}

static uint32_t whd_bus_oci_get_max_transfer_size(whd_driver_t whd_driver)
{
    return WHD_BUS_OCI_MAX_BACKPLANE_TRANSFER_SIZE;
}

static void whd_bus_oci_init_stats(whd_driver_t whd_driver)
{
    /* To be implemented */
}

static whd_result_t whd_bus_oci_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    /* To be implemented */
    UNUSED_VARIABLE(reset_after_print);
    WPRINT_MACRO( ("Bus stats not available\n") );
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_oci_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    UNUSED_PARAMETER(wake_from_firmware);
    /* functionality is only currently needed and present on SDIO */
    return WHD_UNSUPPORTED;
}

static whd_result_t whd_bus_oci_irq_register(whd_driver_t whd_driver)
{
    return WHD_TRUE;
}

static whd_result_t whd_bus_oci_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
    return WHD_TRUE;
}

whd_result_t whd_oci_bus_write_wifi_firmware_image(whd_driver_t whd_driver)
{
    return whd_bus_write_wifi_firmware_image(whd_driver);
}

#ifdef BLHS_SUPPORT
uint8_t whd_bus_oci_blhs_read_h2d(whd_driver_t whd_driver, uint32_t *val )
{
#ifndef GCI_SECURE_ACCESS
    return whd_bus_oci_read_backplane_value(whd_driver, (uint32_t)OCI_REG_DAR_H2D_MSG_0, 1, (uint8_t *)val);
#else
    *val = whd_hw_readBTSwScratchpad2Api();
    return WHD_SUCCESS;
#endif
}

uint8_t whd_bus_oci_blhs_write_h2d(whd_driver_t whd_driver, uint32_t val )
{
#ifndef GCI_SECURE_ACCESS
    return whd_bus_oci_write_backplane_value(whd_driver, (uint32_t)OCI_REG_DAR_H2D_MSG_0, 1, val);
#else
    whd_hw_writeBTSwScratchpad2Api(val);
    return WHD_SUCCESS;
#endif
}

uint8_t whd_bus_oci_blhs_wait_d2h(whd_driver_t whd_driver, uint8_t state)
{
    uint32_t byte_data;
#ifndef GCI_SECURE_ACCESS
    uint32_t loop_count = 0;
#endif

    /* while ( ( ( whd_bus_read_backplane_value(whd_driver, OCI_REG_DAR_SC0_MSG_0, 1, &byte_data) ) == 0 ) &&
            ( (byte_data & OCI_BLHS_WLRDY_BIT) == 0 ) )
    {
        vt_printf("%d ", byte_data);
    } */

    byte_data = 0;

    WPRINT_WHD_DEBUG(("Wait for D2H - %d \n", state));

#ifndef GCI_SECURE_ACCESS
    while ( ( ( whd_bus_oci_read_backplane_value(whd_driver, OCI_REG_DAR_D2H_MSG_0, 1, (uint8_t*)&byte_data) ) == 0 ) &&
            ( (byte_data & state) == 0 )  &&
            (loop_count < 30000)  )
    {
        loop_count += 10;
    }

    if (loop_count >= 30000)
    {
        WPRINT_WHD_ERROR(("%s: D2H Wait TimeOut! \n", __FUNCTION__));
        return -1;
    }
#else
    while ( ( (byte_data & state) == 0 ) /* && (loop_count < 100000) */)	//todo
    {
        //byte_data = thread_ap_whd_hw_readWLSwScratchpad2();
        byte_data = whd_hw_readWLSwScratchpad2Api();
        //loop_count += 10;
    }

    //if (loop_count >= 100000)
    //{
        //WPRINT_WHD_ERROR(("%s: D2H Wait TimeOut! \n", __FUNCTION__));
        //return -1;
    //}
#endif

    return 0;
}

static whd_result_t whd_bus_oci_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage)
{
    uint32_t val;

    switch (stage)
    {
        case PREP_FW_DOWNLOAD:
            CHECK_RETURN(whd_bus_oci_blhs_write_h2d(whd_driver, OCI_BLHS_H2D_BL_INIT) );
            CHECK_RETURN(whd_bus_oci_blhs_wait_d2h(whd_driver, OCI_BLHS_D2H_READY) );
            CHECK_RETURN(whd_bus_oci_blhs_write_h2d(whd_driver, OCI_BLHS_H2D_DL_FW_START) );
            break;
        case POST_FW_DOWNLOAD:
            CHECK_RETURN(whd_bus_oci_blhs_write_h2d(whd_driver, OCI_BLHS_H2D_DL_FW_DONE) );
            if (whd_bus_oci_blhs_wait_d2h(whd_driver, OCI_BLHS_D2H_TRXHDR_PARSE_DONE) != 0)
            {
                whd_bus_oci_blhs_read_h2d(whd_driver, &val);
                whd_bus_oci_blhs_write_h2d(whd_driver, (val | OCI_BLHS_H2D_BL_RESET_ON_ERROR) );
                return 1;
            }
            break;
        case CHK_FW_VALIDATION:
            if ( (whd_bus_oci_blhs_wait_d2h(whd_driver, OCI_BLHS_D2H_VALDN_DONE) != 0) ||
                 (whd_bus_oci_blhs_wait_d2h(whd_driver, OCI_BLHS_D2H_VALDN_RESULT) != 0) )
            {
                whd_bus_oci_blhs_read_h2d(whd_driver, &val);
                whd_bus_oci_blhs_write_h2d(whd_driver, (val | OCI_BLHS_H2D_BL_RESET_ON_ERROR) );
                return 1;
            }
            break;
        case POST_NVRAM_DOWNLOAD:
            CHECK_RETURN(whd_bus_oci_blhs_read_h2d(whd_driver, &val) );
            CHECK_RETURN(whd_bus_oci_blhs_write_h2d(whd_driver, (val | OCI_BLHS_H2D_DL_NVRAM_DONE) ));
            break;
        case POST_WATCHDOG_RESET:
            CHECK_RETURN(whd_bus_oci_blhs_write_h2d(whd_driver, OCI_BLHS_H2D_BL_INIT) );
            CHECK_RETURN(whd_bus_oci_blhs_wait_d2h(whd_driver, OCI_BLHS_D2H_READY) );
        default:
            return 1;
    }

    return 0;
}

#endif /* BLHS_SUPPORT */

static whd_result_t whd_bus_oci_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address,
                                                  uint32_t image_size)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t size_out;

#ifdef PROTO_MSGBUF
    uint8_t *image;
    uint32_t block_count = 0;
    uint32_t i = 0;

    CHECK_RETURN(whd_get_resource_no_of_blocks(whd_driver, resource, &block_count));

    for (i = 0; i < block_count; i++)
    {

        CHECK_RETURN(whd_get_resource_block(whd_driver, resource, i, (const uint8_t **)&image,
                                        &size_out) );

        if (resource == WHD_RESOURCE_WLAN_FIRMWARE)
        {
            if(i == 0)
            {
                trx_header_t *trx = (trx_header_t *)&image[0];

                if (trx->magic == TRX_MAGIC)
                {
                    image_size = trx->len;
                    address -= sizeof(*trx);
                }
                else
                {
                    result = WHD_BADARG;
                    WPRINT_WHD_ERROR( ("%s: TRX header mismatch\n", __FUNCTION__) );
                    return result;
                }
            }
        }

        whd_mem_memcpy( (void *)TRANS_ADDR(address), (void*)image, size_out );

        address += size_out;
    }

#else
    uint32_t reset_instr = 0;

    CHECK_RETURN(whd_resource_read(whd_driver, resource, 0,
                                   GET_C_VAR(whd_driver, CHIP_RAM_SIZE), &size_out,
                                   (uint8_t *)(address) ) );

    CHECK_RETURN(whd_resource_read(whd_driver, resource, 0, 4, &size_out, &reset_instr) );

    /* CR4_FF_ROM_SHADOW_INDEX_REGISTER */
    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, GET_C_VAR(whd_driver, PMU_BASE_ADDRESS) + 0x080,
                                               (uint8_t)1, 0) );

    /* CR4_FF_ROM_SHADOW_DATA_REGISTER */
    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, GET_C_VAR(whd_driver, PMU_BASE_ADDRESS) + 0x084,
                                               (uint8_t)4, reset_instr) );
#endif /* PROTO_MSGBUF */

    return result;
}

static whd_result_t whd_bus_oci_write_wifi_nvram_image(whd_driver_t whd_driver)
{
    uint32_t img_base;
    uint32_t img_end;
    uint32_t image_size;

    /* Get the size of the variable image */
    CHECK_RETURN(whd_resource_size(whd_driver, WHD_RESOURCE_WLAN_NVRAM, &image_size) );

    /* Round up the size of the image */
    image_size = ROUND_UP(image_size, 4);

    /* Write image */
    img_end = GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4;


    img_base = (img_end - image_size);
    img_base += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    CHECK_RETURN(whd_bus_oci_download_resource(whd_driver, WHD_RESOURCE_WLAN_NVRAM, WHD_FALSE, img_base, image_size) );

    /* Write the variable image size at the end */
    image_size = (~(image_size / 4) << 16) | (image_size / 4);

    img_end += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)img_end, 4, image_size) );

#ifdef BLHS_SUPPORT
    CHECK_RETURN(whd_bus_common_blhs(whd_driver, POST_NVRAM_DOWNLOAD) );
#endif /* BLHS_SUPPORT */

    return WHD_SUCCESS;
}

whd_bool_t whd_ensure_wlan_is_up(whd_driver_t whd_driver)
{
    if ( (whd_driver != NULL) && (whd_driver->internal_info.whd_wlan_status.state == WLAN_UP) )
        return WHD_TRUE;
    else
        return WHD_FALSE;
}

#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
void whd_pds_lock_sleep(whd_driver_t whd_driver)
{
    cy_rtos_get_mutex(&whd_driver->sleep_mutex, CY_RTOS_NEVER_TIMEOUT);
    if(whd_driver->lock_sleep == 0)
    {
         whd_driver->lock_sleep++;
         cyhal_syspm_lock_deepsleep();
    }
    cy_rtos_set_mutex(&whd_driver->sleep_mutex);
}

void whd_pds_unlock_sleep(whd_driver_t whd_driver)
{
    cy_rtos_get_mutex(&whd_driver->sleep_mutex, CY_RTOS_NEVER_TIMEOUT);
    if(whd_driver->lock_sleep == 1)
    {
         whd_driver->lock_sleep--;
         cyhal_syspm_unlock_deepsleep();
    }
    cy_rtos_set_mutex(&whd_driver->sleep_mutex);
}
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

#endif /* COMPONENT_WIFI_INTERFACE_OCI */
