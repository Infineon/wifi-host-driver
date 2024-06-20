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
 *  Broadcom WLAN M2M Protocol interface
 *
 *  Implements the WHD Bus Protocol Interface for M2M
 *  Provides functions for initialising, de-intitialising 802.11 device,
 *  sending/receiving raw packets etc
 */

#include "cybsp.h"
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_M2M_INTERFACE)

#include "cyhal_dma.h"
#include "cyhal_hw_types.h"
#include "whd_bus_m2m_protocol.h"

#include "whd_bus.h"
#include "whd_bus_common.h"
#include "whd_chip_reg.h"
#include "whd_chip_constants.h"
#include "whd_buffer_api.h"
#include "whd_types_int.h"
#include "whd_types.h"
#include "whd_thread_internal.h"
#include "whd_resource_if.h"
#include "whd_wlioctl.h"
#include "whd_m2m.h"
#include "whd_proto.h"
#ifdef PROTO_MSGBUF
#include "whd_ring.h"
#endif /* PROTO_MSGBUF */
/******************************************************
*             Constants
******************************************************/

#define WHD_BUS_M2M_BACKPLANE_READ_PADD_SIZE    (0)
#define WHD_BUS_M2M_MAX_BACKPLANE_TRANSFER_SIZE (WHD_PAYLOAD_MTU)
#define BOOT_WLAN_WAIT_TIME                     (5)     /* 5ms wait time */
#define M2M_DMA_RX_BUFFER_SIZE                  (WHD_PHYSICAL_HEADER + WLC_IOCTL_MEDLEN)

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
    whd_m2m_config_t m2m_config;
    cyhal_m2m_t *m2m_obj;
};

/******************************************************
*             Variables
******************************************************/
static whd_bus_info_t whd_bus_m2m_info;
static struct whd_bus_priv whd_bus_priv;

/******************************************************
*             Function declarations
******************************************************/
static whd_result_t whd_bus_m2m_init(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_deinit(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus);
static whd_result_t whd_bus_m2m_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer);

static whd_bool_t whd_bus_m2m_wake_interrupt_present(whd_driver_t whd_driver);
static uint32_t whd_bus_m2m_packet_available_to_read(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer);

static whd_result_t whd_bus_m2m_write_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                      uint8_t register_length, uint32_t value);
static whd_result_t whd_bus_m2m_read_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                                     uint8_t *value);
static whd_result_t whd_bus_m2m_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                     uint32_t address, uint8_t value_length, uint32_t value);
static whd_result_t whd_bus_m2m_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                    uint32_t address, uint8_t value_length, uint8_t *value);
static whd_result_t whd_bus_m2m_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                               whd_bus_function_t function, uint32_t address, uint16_t size,
                                               whd_transfer_bytes_packet_t *data);

static whd_result_t whd_bus_m2m_poke_wlan(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_wakeup(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_sleep(whd_driver_t whd_driver);
static uint8_t whd_bus_m2m_backplane_read_padd_size(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore);
static whd_bool_t whd_bus_m2m_use_status_report_scheme(whd_driver_t whd_driver);
static uint32_t whd_bus_m2m_get_max_transfer_size(whd_driver_t whd_driver);
static void whd_bus_m2m_init_stats(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print);
static whd_result_t whd_bus_m2m_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware);
static whd_result_t whd_bus_m2m_irq_register(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_irq_enable(whd_driver_t whd_driver, whd_bool_t enable);
#ifdef BLHS_SUPPORT
static whd_result_t whd_bus_m2m_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage);
#endif /* BLHS_SUPPORT */
static whd_result_t whd_bus_m2m_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address, uint32_t image_size);
static whd_result_t whd_bus_m2m_write_wifi_nvram_image(whd_driver_t whd_driver);
static whd_result_t whd_bus_m2m_set_backplane_window(whd_driver_t whd_driver, uint32_t addr, uint32_t *curaddr);

static whd_result_t boot_wlan(whd_driver_t whd_driver);
whd_bool_t whd_ensure_wlan_is_up(whd_driver_t whd_driver);
whd_result_t m2m_bus_write_wifi_firmware_image(whd_driver_t whd_driver);
void whd_bus_m2m_irq_handler(void *callback_arg, cyhal_m2m_event_t events);


/******************************************************
*             Function definitions
******************************************************/

// Functions for whd_driver->bus_if function list
whd_result_t whd_bus_m2m_attach(whd_driver_t whd_driver, whd_m2m_config_t *whd_m2m_config, cyhal_m2m_t *m2m_obj)
{
    WPRINT_WHD_INFO( ("m2m_attach\n") );

    whd_driver->bus_if   = &whd_bus_m2m_info;
    whd_driver->bus_priv = &whd_bus_priv;

    memset(whd_driver->bus_if, 0, sizeof(whd_bus_info_t) );
    memset(whd_driver->bus_priv, 0, sizeof(struct whd_bus_priv) );

    whd_driver->bus_priv->m2m_obj = m2m_obj;
    whd_driver->bus_priv->m2m_config = *whd_m2m_config;

#ifndef PROTO_MSGBUF
    whd_driver->proto_type = WHD_PROTO_BCDC;
#else
    whd_driver->proto_type = WHD_PROTO_MSGBUF;
#endif /* PROTO_MSGBUF */

    whd_driver->bus_if->whd_bus_init_fptr = whd_bus_m2m_init;
    whd_driver->bus_if->whd_bus_deinit_fptr = whd_bus_m2m_deinit;

    whd_driver->bus_if->whd_bus_ack_interrupt_fptr = whd_bus_m2m_ack_interrupt;
    whd_driver->bus_if->whd_bus_send_buffer_fptr = whd_bus_m2m_send_buffer;

    whd_driver->bus_if->whd_bus_wake_interrupt_present_fptr = whd_bus_m2m_wake_interrupt_present;
    whd_driver->bus_if->whd_bus_packet_available_to_read_fptr = whd_bus_m2m_packet_available_to_read;
    whd_driver->bus_if->whd_bus_read_frame_fptr = whd_bus_m2m_read_frame;


    whd_driver->bus_if->whd_bus_write_backplane_value_fptr = whd_bus_m2m_write_backplane_value;
    whd_driver->bus_if->whd_bus_read_backplane_value_fptr = whd_bus_m2m_read_backplane_value;

    whd_driver->bus_if->whd_bus_write_register_value_fptr = whd_bus_m2m_write_register_value;
    whd_driver->bus_if->whd_bus_read_register_value_fptr = whd_bus_m2m_read_register_value;

    whd_driver->bus_if->whd_bus_transfer_bytes_fptr = whd_bus_m2m_transfer_bytes;

    whd_driver->bus_if->whd_bus_poke_wlan_fptr = whd_bus_m2m_poke_wlan;

    whd_driver->bus_if->whd_bus_wakeup_fptr = whd_bus_m2m_wakeup;
    whd_driver->bus_if->whd_bus_sleep_fptr = whd_bus_m2m_sleep;

    whd_driver->bus_if->whd_bus_backplane_read_padd_size_fptr = whd_bus_m2m_backplane_read_padd_size;

    whd_driver->bus_if->whd_bus_wait_for_wlan_event_fptr = whd_bus_m2m_wait_for_wlan_event;
    whd_driver->bus_if->whd_bus_use_status_report_scheme_fptr = whd_bus_m2m_use_status_report_scheme;

    whd_driver->bus_if->whd_bus_get_max_transfer_size_fptr = whd_bus_m2m_get_max_transfer_size;

    whd_driver->bus_if->whd_bus_init_stats_fptr = whd_bus_m2m_init_stats;
    whd_driver->bus_if->whd_bus_print_stats_fptr = whd_bus_m2m_print_stats;
    whd_driver->bus_if->whd_bus_reinit_stats_fptr = whd_bus_m2m_reinit_stats;
    whd_driver->bus_if->whd_bus_irq_register_fptr = whd_bus_m2m_irq_register;
    whd_driver->bus_if->whd_bus_irq_enable_fptr = whd_bus_m2m_irq_enable;
    whd_driver->bus_if->whd_bus_download_resource_fptr = whd_bus_m2m_download_resource;
    whd_driver->bus_if->whd_bus_set_backplane_window_fptr = whd_bus_m2m_set_backplane_window;

#ifdef BLHS_SUPPORT
    whd_driver->bus_if->whd_bus_blhs_fptr = whd_bus_m2m_blhs;
#endif /* BLHS_SUPPORT */
    return WHD_SUCCESS;
}

void whd_bus_m2m_detach(whd_driver_t whd_driver)
{
    whd_driver->bus_if = NULL;
    whd_driver->bus_priv = NULL;
}

void whd_bus_m2m_irq_handler(void *callback_arg, cyhal_m2m_event_t events)
{
    whd_driver_t whd_driver = (whd_driver_t)callback_arg;

    whd_bus_m2m_irq_enable(whd_driver, WHD_FALSE);
    whd_thread_notify_irq(whd_driver);
}

static whd_result_t whd_bus_m2m_init(whd_driver_t whd_driver)
{
    whd_result_t result = WHD_SUCCESS;

#if !defined (__IAR_SYSTEMS_ICC__)
    /* Get chip id */
    whd_chip_set_chip_id(whd_driver, (uint16_t)REGISTER_READ(uint32_t, CHIPCOMMON_BASE_ADDRESS) );
#endif

    result = boot_wlan(whd_driver);

#ifdef PROTO_MSGBUF
    CHECK_RETURN(whd_bus_m2m_sharedmem_init(whd_driver) );
#else
    if (result == WHD_SUCCESS)
    {
        cyhal_m2m_init(whd_driver->bus_priv->m2m_obj, M2M_DMA_RX_BUFFER_SIZE);
        cyhal_m2m_register_callback(whd_driver->bus_priv->m2m_obj, whd_bus_m2m_irq_handler, whd_driver);
    }
#endif /* PROTO_MSGBUF */

    return result;
}

static whd_result_t whd_bus_m2m_deinit(whd_driver_t whd_driver)
{
    //PLATFORM_WLAN_POWERSAVE_RES_UP();

    /* Down M2M and WLAN */
    CHECK_RETURN(whd_disable_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_CPU_HALT) );
    cyhal_m2m_free(whd_driver->bus_priv->m2m_obj);

    /* Put WLAN to reset. */
    //host_platform_reset_wifi( WICED_TRUE );

    //PLATFORM_WLAN_POWERSAVE_RES_DOWN( NULL, WICED_FALSE );

    /* Force resource down even if resource up/down is unbalanced */
    //PLATFORM_WLAN_POWERSAVE_RES_DOWN( NULL, WICED_TRUE );

    CHECK_RETURN(whd_allow_wlan_bus_to_sleep(whd_driver) );

    whd_bus_set_resource_download_halt(whd_driver, WHD_FALSE);

    DELAYED_BUS_RELEASE_SCHEDULE(whd_driver, WHD_FALSE);

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus)
{
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_result_t result = WHD_SUCCESS;

    if (cyhal_m2m_tx_send(whd_driver->bus_priv->m2m_obj, buffer) != CY_RSLT_SUCCESS)
    {
        result = WHD_WLAN_ERROR;
    }

    return result;
}

static whd_bool_t whd_bus_m2m_wake_interrupt_present(whd_driver_t whd_driver)
{
    /* functionality is only currently needed and present on SDIO */
    return WHD_FALSE;
}

static uint32_t whd_bus_m2m_packet_available_to_read(whd_driver_t whd_driver)
{
    return 1;
}

static whd_result_t whd_bus_m2m_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer)
{
    whd_result_t result = WHD_SUCCESS;
    cyhal_m2m_event_t m2m_event;
    uint16_t *hwtag;
    void *packet = NULL;
    bool signal_txdone;

    cyhal_m2m_rx_prepare(whd_driver->bus_priv->m2m_obj);

    m2m_event = cyhal_m2m_intr_status(whd_driver->bus_priv->m2m_obj, &signal_txdone);
    if (signal_txdone)
    {
        /* Signal WLAN core there is a TX done by setting wlancr4 SW interrupt 0 */
        CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, GET_C_VAR(whd_driver, PMU_BASE_ADDRESS) + 0x1c,
                                                   (uint8_t)4, ARMCR4_SW_INT0) );
    }

    /* Handle DMA interrupts */
    if ( (m2m_event & CYHAL_M2M_TX_CHANNEL_INTERRUPT) != 0 )
    {
        cyhal_m2m_tx_release(whd_driver->bus_priv->m2m_obj);
    }

    /* Handle DMA receive interrupt */
    cyhal_m2m_rx_receive(whd_driver->bus_priv->m2m_obj, &packet, &hwtag);
    if (packet == NULL)
    {
        result = WHD_NO_PACKET_TO_RECEIVE;
    }
    else
    {
        *buffer = packet;

        /* move the data pointer 12 bytes(sizeof(wwd_buffer_header_t))
         * back to the start of the pakcet
         */
        whd_buffer_add_remove_at_front(whd_driver, buffer, -(int)sizeof(whd_buffer_header_t) );
#ifndef PROTO_MSGBUF
        whd_sdpcm_update_credit(whd_driver, (uint8_t *)hwtag);
#endif /* PROTO_MSGBUF */
    }

    cyhal_m2m_rx_prepare(whd_driver->bus_priv->m2m_obj);

    return result;
}

static whd_result_t whd_bus_m2m_write_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                      uint8_t register_length, uint32_t value)
{
#if !defined (__IAR_SYSTEMS_ICC__)
    MEMORY_BARRIER_AGAINST_COMPILER_REORDERING();

    if (register_length == 4)
    {
        REGISTER_WRITE_WITH_BARRIER(uint32_t, address, value);
    }
    else if (register_length == 2)
    {
        REGISTER_WRITE_WITH_BARRIER(uint16_t, address, value);
    }
    else if (register_length == 1)
    {
        REGISTER_WRITE_WITH_BARRIER(uint8_t, address, value);
    }
    else
    {
        return WHD_WLAN_ERROR;
    }
#endif
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_read_backplane_value(whd_driver_t whd_driver, uint32_t address,
                                                     uint8_t register_length, /*@out@*/ uint8_t *value)
{
#if !defined (__IAR_SYSTEMS_ICC__)
    MEMORY_BARRIER_AGAINST_COMPILER_REORDERING();

    if (register_length == 4)
    {
        *( (uint32_t *)value ) = REGISTER_READ(uint32_t, address);
    }
    else if (register_length == 2)
    {
        *( (uint16_t *)value ) = REGISTER_READ(uint16_t, address);
    }
    else if (register_length == 1)
    {
        *value = REGISTER_READ(uint8_t, address);
    }
    else
    {
        return WHD_WLAN_ERROR;
    }
#endif
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                     uint32_t address, uint8_t value_length, uint32_t value)
{
    // Not used by M2M bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function,
                                                    uint32_t address, uint8_t value_length, uint8_t *value)
{
    // Not used by M2M bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                               whd_bus_function_t function, uint32_t address, uint16_t size,
                                               whd_transfer_bytes_packet_t *data)
{
    if (function != BACKPLANE_FUNCTION)
    {
        return WHD_DOES_NOT_EXIST;
    }

    if (direction == BUS_WRITE)
    {
        DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
        memcpy( (uint8_t *)address, data->data, size );
        ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
        if (address == 0)
        {
            uint32_t resetinst = *( (uint32_t *)data->data );

            /* CR4_FF_ROM_SHADOW_INDEX_REGISTER */
            CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, GET_C_VAR(whd_driver, PMU_BASE_ADDRESS) + 0x080,
                                                       (uint8_t)1, 0) );

            /* CR4_FF_ROM_SHADOW_DATA_REGISTER */
            CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, GET_C_VAR(whd_driver, PMU_BASE_ADDRESS) + 0x084,
                                                       (uint8_t)4, resetinst) );
        }
    }
    else
    {
        DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
        memcpy(data->data, (uint8_t *)address, size);
        ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_poke_wlan(whd_driver_t whd_driver)
{
    // Not used by M2M bus
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_wakeup(whd_driver_t whd_driver)
{
    WPRINT_WHD_INFO( ("bus_m2m_wakeup\n") );
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_sleep(whd_driver_t whd_driver)
{
    WPRINT_WHD_INFO( ("bus_m2m_sleep\n") );
    return WHD_SUCCESS;
}

static uint8_t whd_bus_m2m_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return WHD_BUS_M2M_BACKPLANE_READ_PADD_SIZE;
}

static whd_result_t whd_bus_m2m_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t timeout_ms;

#ifdef PROTO_MSGBUF
    timeout_ms = 1;
    uint32_t delayed_release_timeout_ms;

    delayed_release_timeout_ms = whd_bus_handle_delayed_release(whd_driver);
    if (delayed_release_timeout_ms != 0)
    {
        timeout_ms = delayed_release_timeout_ms;
    }
    else
    {
        result = whd_bus_suspend(whd_driver);

        if (result == WHD_SUCCESS)
        {
            timeout_ms = CY_RTOS_NEVER_TIMEOUT;
        }
    }

    result = cy_rtos_get_semaphore(transceive_semaphore, (uint32_t)MIN_OF(timeout_ms,
                                                                          WHD_THREAD_POLL_TIMEOUT), WHD_FALSE);
#else
    timeout_ms = CY_RTOS_NEVER_TIMEOUT;
    whd_bus_m2m_irq_enable(whd_driver, WHD_TRUE);
    result = cy_rtos_get_semaphore(transceive_semaphore, timeout_ms, WHD_FALSE);
#endif
    return result;
}

static whd_bool_t whd_bus_m2m_use_status_report_scheme(whd_driver_t whd_driver)
{
    /* !M2M_RX_POLL_MODE */
    return WHD_FALSE;
}

static uint32_t whd_bus_m2m_get_max_transfer_size(whd_driver_t whd_driver)
{
    return WHD_BUS_M2M_MAX_BACKPLANE_TRANSFER_SIZE;
}

static void whd_bus_m2m_init_stats(whd_driver_t whd_driver)
{
    /* To be implemented */
}

static whd_result_t whd_bus_m2m_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    /* To be implemented */
    UNUSED_VARIABLE(reset_after_print);
    WPRINT_MACRO( ("Bus stats not available\n") );
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_m2m_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    UNUSED_PARAMETER(wake_from_firmware);
    /* functionality is only currently needed and present on SDIO */
    return WHD_UNSUPPORTED;
}

static whd_result_t whd_bus_m2m_irq_register(whd_driver_t whd_driver)
{
    return WHD_TRUE;
}

static whd_result_t whd_bus_m2m_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
    if (enable)
    {
        WPRINT_WHD_INFO( ("M2M interrupt enable\n") );
        _cyhal_system_m2m_enable_irq();
        _cyhal_system_sw0_enable_irq();
    }
    else
    {
        WPRINT_WHD_INFO( ("M2M interrupt disable\n") );
        _cyhal_system_m2m_disable_irq();
        _cyhal_system_sw0_disable_irq();
    }
    return WHD_TRUE;
}

whd_result_t m2m_bus_write_wifi_firmware_image(whd_driver_t whd_driver)
{
#ifndef PROTO_MSGBUF
    /* Halt ARM and remove from reset */
    WPRINT_WHD_INFO( ("Reset wlan core..\n") );
    VERIFY_RESULT(whd_reset_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_CPU_HALT) );
#endif /* PROTO_MSGBUF */

    return whd_bus_write_wifi_firmware_image(whd_driver);
}

static whd_result_t boot_wlan(whd_driver_t whd_driver)
{
    whd_result_t result = WHD_SUCCESS;

    /* Load wlan firmware from sflash */
    result = m2m_bus_write_wifi_firmware_image(whd_driver);
    if ( (result == WHD_UNFINISHED) || (result != WHD_SUCCESS) )
    {
        /* for user abort, then put wifi module into known good state */
        return result;
    }

    VERIFY_RESULT(whd_bus_m2m_write_wifi_nvram_image(whd_driver) );

#ifndef PROTO_MSGBUF
    /* Release ARM core */
    WPRINT_WHD_INFO( ("Release WLAN core..\n") );
    VERIFY_RESULT(whd_wlan_armcore_run(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_NONE) );

#if (PLATFORM_BACKPLANE_ON_CPU_CLOCK_ENABLE == 0)
    /*
     * Wifi firmware initialization will change some BBPLL settings. When backplane clock
     * source is not on CPU clock, accessing backplane during that period might wedge the
     * ACPU. Running a delay loop in cache can avoid the wedge. At least 3ms is required
     * to avoid the problem.
     */
    cy_rtos_delay_milliseconds(10);
#endif  /* PLATFORM_BACKPLANE_ON_CPU_CLOCK_ENABLE == 0 */
#endif /* PROTO_MSGBUF */

    return result;
}

#ifdef BLHS_SUPPORT
uint8_t whd_bus_m2m_blhs_read_h2d(whd_driver_t whd_driver, uint32_t *val)
{
    return whd_bus_m2m_read_backplane_value(whd_driver, (uint32_t)M2M_REG_DAR_H2D_MSG_0, 1, (uint8_t *)val);
}

uint8_t whd_bus_m2m_blhs_write_h2d(whd_driver_t whd_driver, uint32_t val)
{
    return whd_bus_m2m_write_backplane_value(whd_driver, (uint32_t)M2M_REG_DAR_H2D_MSG_0, 1, val);
}

uint8_t whd_bus_m2m_blhs_wait_d2h(whd_driver_t whd_driver, uint8_t state)
{
    uint8_t byte_data;
    uint32_t loop_count = 0;

    /* while ( ( ( whd_bus_read_backplane_value(whd_driver, M2M_REG_DAR_SC0_MSG_0, 1, &byte_data) ) == 0 ) &&
            ( (byte_data & M2M_BLHS_WLRDY_BIT) == 0 ) )
       {
        vt_printf("%d ", byte_data);
       } */

    byte_data = 0;

    WPRINT_WHD_DEBUG( ("Wait for D2H - %d \n", state) );

    while ( ( (whd_bus_m2m_read_backplane_value(whd_driver, M2M_REG_DAR_D2H_MSG_0, 1, &byte_data) ) == 0 ) &&
            ( (byte_data & state) == 0 )  &&
            (loop_count < 30000) )
    {
        loop_count += 10;
    }
    if (loop_count >= 30000)
    {
        WPRINT_WHD_ERROR( ("%s: D2H Wait TimeOut! \n", __FUNCTION__) );
        return -1;
    }

    return 0;
}

static whd_result_t whd_bus_m2m_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage)
{
    uint32_t val;

    switch (stage)
    {
        case PREP_FW_DOWNLOAD:
            CHECK_RETURN(whd_bus_m2m_blhs_write_h2d(whd_driver, M2M_BLHS_H2D_BL_INIT) );
            CHECK_RETURN(whd_bus_m2m_blhs_wait_d2h(whd_driver, M2M_BLHS_D2H_READY) );
            CHECK_RETURN(whd_bus_m2m_blhs_write_h2d(whd_driver, M2M_BLHS_H2D_DL_FW_START) );
            break;
        case POST_FW_DOWNLOAD:
            CHECK_RETURN(whd_bus_m2m_blhs_write_h2d(whd_driver, M2M_BLHS_H2D_DL_FW_DONE) );
            if (whd_bus_m2m_blhs_wait_d2h(whd_driver, M2M_BLHS_D2H_TRXHDR_PARSE_DONE) != 0)
            {
                whd_bus_m2m_blhs_read_h2d(whd_driver, &val);
                whd_bus_m2m_blhs_write_h2d(whd_driver, (val | M2M_BLHS_H2D_BL_RESET_ON_ERROR) );
                return 1;
            }
            break;
        case CHK_FW_VALIDATION:
            if ( (whd_bus_m2m_blhs_wait_d2h(whd_driver, M2M_BLHS_D2H_VALDN_DONE) != 0) ||
                 (whd_bus_m2m_blhs_wait_d2h(whd_driver, M2M_BLHS_D2H_VALDN_RESULT) != 0) )
            {
                whd_bus_m2m_blhs_read_h2d(whd_driver, &val);
                whd_bus_m2m_blhs_write_h2d(whd_driver, (val | M2M_BLHS_H2D_BL_RESET_ON_ERROR) );
                return 1;
            }
            break;
        case POST_NVRAM_DOWNLOAD:
            CHECK_RETURN(whd_bus_m2m_blhs_read_h2d(whd_driver, &val) );
            CHECK_RETURN(whd_bus_m2m_blhs_write_h2d(whd_driver, (val | M2M_BLHS_H2D_DL_NVRAM_DONE) ) );
            break;
        case POST_WATCHDOG_RESET:
            CHECK_RETURN(whd_bus_m2m_blhs_write_h2d(whd_driver, M2M_BLHS_H2D_BL_INIT) );
            CHECK_RETURN(whd_bus_m2m_blhs_wait_d2h(whd_driver, M2M_BLHS_D2H_READY) );
        default:
            return 1;
    }

    return 0;
}

#endif /* BLHS_SUPPORT */

static whd_result_t whd_bus_m2m_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address,
                                                  uint32_t image_size)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t size_out;

#ifdef PROTO_MSGBUF
    uint8_t *image;

    CHECK_RETURN(whd_get_resource_block(whd_driver, resource, 0, (const uint8_t **)&image, &size_out) );

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
    memcpy( (void *)TRANS_ADDR(address), image, image_size );
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

static whd_result_t whd_bus_m2m_write_wifi_nvram_image(whd_driver_t whd_driver)
{
    uint32_t nvram_size;
    uint32_t nvram_destination_address;
    uint32_t nvram_size_in_words;
    uint32_t size_out;
    uint32_t image_size;

    /* Get the size of the variable image */
    CHECK_RETURN(whd_resource_size(whd_driver, WHD_RESOURCE_WLAN_NVRAM, &image_size) );

    /* Round up the size of the image */
    nvram_size = ROUND_UP(image_size, 4);

    /* Put the NVRAM image at the end of RAM leaving the last 4 bytes for the size */
    nvram_destination_address = (GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4) - nvram_size;
    nvram_destination_address += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

#ifdef PROTO_MSGBUF
    CHECK_RETURN(whd_resource_read(whd_driver, WHD_RESOURCE_WLAN_NVRAM, 0,
                                   image_size, &size_out, (uint8_t *)TRANS_ADDR(nvram_destination_address) ) );
#else
    /* Write NVRAM image into WLAN RAM */
    CHECK_RETURN(whd_resource_read(whd_driver, WHD_RESOURCE_WLAN_NVRAM, 0,
                                   image_size, &size_out, (uint8_t *)nvram_destination_address) );
#endif /* PROTO_MSGBUF */

    /* Calculate the NVRAM image size in words (multiples of 4 bytes) and its bitwise inverse */
    nvram_size_in_words = nvram_size / 4;
    nvram_size_in_words = (~nvram_size_in_words << 16) | (nvram_size_in_words & 0x0000FFFF);

#ifdef PROTO_MSGBUF
    memcpy( (uint8_t *)TRANS_ADDR( (GET_C_VAR(whd_driver,
                                              ATCM_RAM_BASE_ADDRESS) + GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4) ),
            (uint8_t *)&nvram_size_in_words, 4 );
#else
    memcpy( (uint8_t *)(GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS) + GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4),
            (uint8_t *)&nvram_size_in_words, 4 );
#endif /* PROTO_MSGBUF */

    return WHD_SUCCESS;
}

whd_result_t whd_bus_m2m_set_backplane_window(whd_driver_t whd_driver, uint32_t addr, uint32_t *cur_addr)
{
    return WHD_UNSUPPORTED;
}

whd_bool_t whd_ensure_wlan_is_up(whd_driver_t whd_driver)
{
    if ( (whd_driver != NULL) && (whd_driver->internal_info.whd_wlan_status.state == WLAN_UP) )
        return WHD_TRUE;
    else
        return WHD_FALSE;
}

#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_M2M_INTERFACE) */

