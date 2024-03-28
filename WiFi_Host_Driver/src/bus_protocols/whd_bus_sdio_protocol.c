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
 *  Broadcom WLAN SDIO Protocol interface
 *
 *  Implements the WHD Bus Protocol Interface for SDIO
 *  Provides functions for initialising, de-intitialising 802.11 device,
 *  sending/receiving raw packets etc
 */

#include "cybsp.h"
#include "whd_utils.h"

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) && !defined(COMPONENT_WIFI_INTERFACE_OCI)

#include "cyabs_rtos.h"
#include "cyhal_sdio.h"
#include "cyhal_gpio.h"

#include "whd_bus_sdio_protocol.h"
#include "whd_bus.h"
#include "whd_bus_common.h"
#include "whd_chip_reg.h"
#include "whd_chip_constants.h"
#include "whd_int.h"
#include "whd_chip.h"
#include "whd_sdpcm.h"
#include "whd_debug.h"
#include "whd_sdio.h"
#include "whd_buffer_api.h"
#include "whd_resource_if.h"
#include "whd_types_int.h"
#include "whd_types.h"
#include "whd_proto.h"

#ifdef DM_43022C1
#include "resources.h"
#endif

#ifdef CYCFG_ULP_SUPPORT_ENABLED
#include "cy_network_mw_core.h"
#endif

/******************************************************
*             Constants
******************************************************/
/* function 1 OCP space */
#define SBSDIO_SB_OFT_ADDR_MASK     0x07FFF     /* sb offset addr is <= 15 bits, 32k */
#define SBSDIO_SB_OFT_ADDR_LIMIT    0x08000
#define SBSDIO_SB_ACCESS_2_4B_FLAG  0x08000     /* with b15, maps to 32-bit SB access */

#define F0_WORKING_TIMEOUT_MS (500)
#define F1_AVAIL_TIMEOUT_MS   (500)
#define F2_AVAIL_TIMEOUT_MS   (500)
#define F2_READY_TIMEOUT_MS   (1000)
#define ALP_AVAIL_TIMEOUT_MS  (100)
#define HT_AVAIL_TIMEOUT_MS   (2500)
#define ABORT_TIMEOUT_MS      (100)
/* Taken from FALCON_5_90_195_26 dhd/sys/dhd_sdio.c. */
#define SDIO_F2_WATERMARK     (8)

#define INITIAL_READ   4

#define WHD_THREAD_POLL_TIMEOUT      (CY_RTOS_NEVER_TIMEOUT)

#define WHD_THREAD_POKE_TIMEOUT      (100)

#define HOSTINTMASK                 (I_HMB_SW_MASK)


/******************************************************
*             Structures
******************************************************/
struct whd_bus_priv
{
    whd_sdio_config_t sdio_config;
    whd_bus_stats_t whd_bus_stats;
    cyhal_sdio_t *sdio_obj;
};

/* For BSP backward compatible, should be removed the macro once 1.0 is not supported */
#if (CYHAL_API_VERSION >= 2)
typedef cyhal_sdio_transfer_type_t cyhal_sdio_transfer_t;
#else
typedef cyhal_transfer_t cyhal_sdio_transfer_t;
#endif
/******************************************************
*             Variables
******************************************************/

/******************************************************
*             Static Function Declarations
******************************************************/

static whd_result_t whd_bus_sdio_transfer(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                          whd_bus_function_t function, uint32_t address, uint16_t data_size,
                                          uint8_t *data, sdio_response_needed_t response_expected);
static whd_result_t whd_bus_sdio_cmd52(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                       whd_bus_function_t function, uint32_t address, uint8_t value,
                                       sdio_response_needed_t response_expected, uint8_t *response);
static whd_result_t whd_bus_sdio_cmd53(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                       whd_bus_function_t function, sdio_transfer_mode_t mode, uint32_t address,
                                       uint16_t data_size, uint8_t *data,
                                       sdio_response_needed_t response_expected,
                                       uint32_t *response);
static whd_result_t whd_bus_sdio_abort_read(whd_driver_t whd_driver, whd_bool_t retry);
static whd_result_t whd_bus_sdio_download_firmware(whd_driver_t whd_driver);

static whd_result_t whd_bus_sdio_set_oob_interrupt(whd_driver_t whd_driver, uint8_t gpio_pin_number);
#if (CYHAL_API_VERSION >= 2)
static void whd_bus_sdio_irq_handler(void *handler_arg, cyhal_sdio_event_t event);
static void whd_bus_sdio_oob_irq_handler(void *arg, cyhal_gpio_event_t event);
#else
static void whd_bus_sdio_irq_handler(void *handler_arg, cyhal_sdio_irq_event_t event);
static void whd_bus_sdio_oob_irq_handler(void *arg, cyhal_gpio_irq_event_t event);
#endif
static whd_result_t whd_bus_sdio_irq_register(whd_driver_t whd_driver);
static whd_result_t whd_bus_sdio_irq_enable(whd_driver_t whd_driver, whd_bool_t enable);
static whd_result_t whd_bus_sdio_init_oob_intr(whd_driver_t whd_driver);
static whd_result_t whd_bus_sdio_deinit_oob_intr(whd_driver_t whd_driver);
static whd_result_t whd_bus_sdio_register_oob_intr(whd_driver_t whd_driver);
static whd_result_t whd_bus_sdio_unregister_oob_intr(whd_driver_t whd_driver);
static whd_result_t whd_bus_sdio_enable_oob_intr(whd_driver_t whd_driver, whd_bool_t enable);
#ifdef BLHS_SUPPORT
static whd_result_t whd_bus_sdio_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage);
#endif
static whd_result_t whd_bus_sdio_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                   whd_bool_t direct_resource, uint32_t address, uint32_t image_size);
static whd_result_t whd_bus_sdio_write_wifi_nvram_image(whd_driver_t whd_driver);
/******************************************************
*             Global Function definitions
******************************************************/

whd_result_t whd_bus_sdio_attach(whd_driver_t whd_driver, whd_sdio_config_t *whd_sdio_config, cyhal_sdio_t *sdio_obj)
{
    struct whd_bus_info *whd_bus_info;

    if (!whd_driver || !whd_sdio_config)
    {
        WPRINT_WHD_ERROR( ("Invalid param in func %s at line %d \n",
                           __func__, __LINE__) );
        return WHD_WLAN_BADARG;
    }

    whd_bus_info = (whd_bus_info_t *)whd_mem_malloc(sizeof(whd_bus_info_t) );

    if (whd_bus_info == NULL)
    {
        WPRINT_WHD_ERROR( ("Memory allocation failed for whd_bus_info in %s\n", __FUNCTION__) );
        return WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }
    memset(whd_bus_info, 0, sizeof(whd_bus_info_t) );

    whd_driver->bus_if = whd_bus_info;

    whd_driver->bus_priv = (struct whd_bus_priv *)whd_mem_malloc(sizeof(struct whd_bus_priv) );

    if (whd_driver->bus_priv == NULL)
    {
        WPRINT_WHD_ERROR( ("Memory allocation failed for whd_bus_priv in %s\n", __FUNCTION__) );
        return WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }
    memset(whd_driver->bus_priv, 0, sizeof(struct whd_bus_priv) );

    whd_driver->bus_priv->sdio_obj = sdio_obj;
    whd_driver->bus_priv->sdio_config = *whd_sdio_config;

    whd_driver->proto_type = WHD_PROTO_BCDC;

    whd_bus_info->whd_bus_init_fptr = whd_bus_sdio_init;
    whd_bus_info->whd_bus_deinit_fptr = whd_bus_sdio_deinit;

    whd_bus_info->whd_bus_write_backplane_value_fptr = whd_bus_sdio_write_backplane_value;
    whd_bus_info->whd_bus_read_backplane_value_fptr = whd_bus_sdio_read_backplane_value;
    whd_bus_info->whd_bus_write_register_value_fptr = whd_bus_sdio_write_register_value;
    whd_bus_info->whd_bus_read_register_value_fptr = whd_bus_sdio_read_register_value;

    whd_bus_info->whd_bus_send_buffer_fptr = whd_bus_sdio_send_buffer;
    whd_bus_info->whd_bus_transfer_bytes_fptr = whd_bus_sdio_transfer_bytes;

    whd_bus_info->whd_bus_read_frame_fptr = whd_bus_sdio_read_frame;

    whd_bus_info->whd_bus_packet_available_to_read_fptr = whd_bus_sdio_packet_available_to_read;
    whd_bus_info->whd_bus_poke_wlan_fptr = whd_bus_sdio_poke_wlan;
    whd_bus_info->whd_bus_wait_for_wlan_event_fptr = whd_bus_sdio_wait_for_wlan_event;

    whd_bus_info->whd_bus_ack_interrupt_fptr = whd_bus_sdio_ack_interrupt;
    whd_bus_info->whd_bus_wake_interrupt_present_fptr = whd_bus_sdio_wake_interrupt_present;

    whd_bus_info->whd_bus_wakeup_fptr = whd_bus_sdio_wakeup;
    whd_bus_info->whd_bus_sleep_fptr = whd_bus_sdio_sleep;

    whd_bus_info->whd_bus_backplane_read_padd_size_fptr = whd_bus_sdio_backplane_read_padd_size;
    whd_bus_info->whd_bus_use_status_report_scheme_fptr = whd_bus_sdio_use_status_report_scheme;

    whd_bus_info->whd_bus_get_max_transfer_size_fptr = whd_bus_sdio_get_max_transfer_size;

    whd_bus_info->whd_bus_init_stats_fptr = whd_bus_sdio_init_stats;
    whd_bus_info->whd_bus_print_stats_fptr = whd_bus_sdio_print_stats;
    whd_bus_info->whd_bus_reinit_stats_fptr = whd_bus_sdio_reinit_stats;
    whd_bus_info->whd_bus_irq_register_fptr = whd_bus_sdio_irq_register;
    whd_bus_info->whd_bus_irq_enable_fptr = whd_bus_sdio_irq_enable;
#ifdef BLHS_SUPPORT
    whd_bus_info->whd_bus_blhs_fptr = whd_bus_sdio_blhs;
#endif
    whd_bus_info->whd_bus_download_resource_fptr = whd_bus_sdio_download_resource;
    whd_bus_info->whd_bus_set_backplane_window_fptr = whd_bus_sdio_set_backplane_window;

    return WHD_SUCCESS;
}

void whd_bus_sdio_detach(whd_driver_t whd_driver)
{
    if (whd_driver->bus_if != NULL)
    {
        whd_mem_free(whd_driver->bus_if);
        whd_driver->bus_if = NULL;
    }
    if (whd_driver->bus_priv != NULL)
    {
        whd_mem_free(whd_driver->bus_priv);
        whd_driver->bus_priv = NULL;
    }
}

whd_result_t whd_bus_sdio_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus)
{
    return whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4, intstatus);
}

whd_result_t whd_bus_sdio_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t timeout_ms = 1;
    uint32_t delayed_release_timeout_ms;

    REFERENCE_DEBUG_ONLY_VARIABLE(result);

    delayed_release_timeout_ms = whd_bus_handle_delayed_release(whd_driver);
    if (delayed_release_timeout_ms != 0)
    {
        timeout_ms = delayed_release_timeout_ms;
    }
    else
    {
        result = whd_allow_wlan_bus_to_sleep(whd_driver);
        whd_assert("Error setting wlan sleep", (result == WHD_SUCCESS) || (result == WHD_PENDING) );

        if (result == WHD_SUCCESS)
        {
            timeout_ms = CY_RTOS_NEVER_TIMEOUT;
        }
    }

    /* Check if we have run out of bus credits */
    if ( (whd_sdpcm_has_tx_packet(whd_driver) == WHD_TRUE) && (whd_sdpcm_get_available_credits(whd_driver) == 0) )
    {
        /* Keep poking the WLAN until it gives us more credits */
        result = whd_bus_poke_wlan(whd_driver);
        whd_assert("Poking failed!", result == WHD_SUCCESS);

        result = cy_rtos_get_semaphore(transceive_semaphore, (uint32_t)MIN_OF(timeout_ms,
                                                                              WHD_THREAD_POKE_TIMEOUT), WHD_FALSE);
    }
    else
    {
        result = cy_rtos_get_semaphore(transceive_semaphore, (uint32_t)MIN_OF(timeout_ms,
                                                                              WHD_THREAD_POLL_TIMEOUT), WHD_FALSE);
    }
    whd_assert("Could not get whd sleep semaphore\n", (result == CY_RSLT_SUCCESS) || (result == CY_RTOS_TIMEOUT) );

    return result;
}

/* Device data transfer functions */
whd_result_t whd_bus_sdio_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_result_t retval;
    retval =
        whd_bus_transfer_bytes(whd_driver, BUS_WRITE, WLAN_FUNCTION, 0,
                               (uint16_t)(whd_buffer_get_current_piece_size(whd_driver,
                                                                            buffer) - sizeof(whd_buffer_t) ),
                               (whd_transfer_bytes_packet_t *)(whd_buffer_get_current_piece_data_pointer(whd_driver,
                                                                                                         buffer) +
                                                               sizeof(whd_buffer_t) ) );
    CHECK_RETURN(whd_buffer_release(whd_driver, buffer, WHD_NETWORK_TX) );
    if (retval == WHD_SUCCESS)
    {
        DELAYED_BUS_RELEASE_SCHEDULE(whd_driver, WHD_TRUE);
    }
    CHECK_RETURN (retval);

    return WHD_SUCCESS;
}

whd_result_t whd_bus_sdio_init(whd_driver_t whd_driver)
{
    uint8_t byte_data;
#ifdef DM_43022C1
    uint8_t secure_chk = -1;
#endif
    whd_result_t result;
    uint32_t loop_count;
    whd_time_t elapsed_time, current_time;
    uint32_t wifi_firmware_image_size = 0;
    uint16_t chip_id;
    uint8_t *aligned_addr = NULL;
    memset(&whd_driver->chip_info, 0, sizeof(whd_driver->chip_info) );

    whd_bus_set_flow_control(whd_driver, WHD_FALSE);

    whd_bus_init_backplane_window(whd_driver);

    /* Setup the backplane*/
    loop_count = 0;
    do
    {
        /* Enable function 1 (backplane) */
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1,
                                                  SDIO_FUNC_ENABLE_1) );
        if (loop_count != 0)
        {
            (void)cy_rtos_delay_milliseconds( (uint32_t)1 );    /* Ignore return - nothing can be done if it fails */
        }
        CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1, &byte_data) );
        loop_count++;
        if (loop_count >= (uint32_t)F0_WORKING_TIMEOUT_MS)
        {
            WPRINT_WHD_ERROR( ("Timeout while setting up the backplane, %s failed at %d \n", __func__, __LINE__) );
            return WHD_TIMEOUT;
        }
    } while (byte_data != (uint8_t)SDIO_FUNC_ENABLE_1);

    if (whd_driver->bus_priv->sdio_config.sdio_1bit_mode == WHD_FALSE)
    {
        /* Read the bus width and set to 4 bits */
        CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BICTRL, (uint8_t)1,
                                                  &byte_data) );
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BICTRL, (uint8_t)1,
                                                  (byte_data & (~BUS_SD_DATA_WIDTH_MASK) ) |
                                                  BUS_SD_DATA_WIDTH_4BIT) );
        /* NOTE: We don't need to change our local bus settings since we're not sending any data (only using CMD52)
         * until after we change the bus speed further down */
    }

    /* Set the block size */

    /* Wait till the backplane is ready */
    loop_count = 0;
    while ( ( (result = whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0, (uint8_t)1,
                                                     (uint32_t)SDIO_64B_BLOCK) ) == WHD_SUCCESS ) &&
            ( (result = whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0, (uint8_t)1,
                                                     &byte_data) ) == WHD_SUCCESS ) &&
            (byte_data != (uint8_t)SDIO_64B_BLOCK) &&
            (loop_count < (uint32_t)F0_WORKING_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );    /* Ignore return - nothing can be done if it fails */
        loop_count++;
        if (loop_count >= (uint32_t)F0_WORKING_TIMEOUT_MS)
        {
            /* If the system fails here, check the high frequency crystal is working */
            WPRINT_WHD_ERROR( ("Timeout while setting block size, %s failed at %d \n", __func__, __LINE__) );
            return WHD_TIMEOUT;
        }
    }

    CHECK_RETURN(result);

    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0,   (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F1BLKSIZE_0, (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F2BLKSIZE_0, (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F2BLKSIZE_1, (uint8_t)1,
                                              (uint32_t)0) );                                                                                  /* Function 2 = 64 */

    /* Register interrupt handler*/
    whd_bus_sdio_irq_register(whd_driver);
    /* Enable SDIO IRQ */
    whd_bus_sdio_irq_enable(whd_driver, WHD_TRUE);

    /* Enable/Disable Client interrupts */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_INTEN, (uint8_t)1,
                                              INTR_CTL_MASTER_EN | INTR_CTL_FUNC1_EN | INTR_CTL_FUNC2_EN) );

    if (whd_driver->bus_priv->sdio_config.high_speed_sdio_clock)
    {
        /* This code is required if we want more than 25 MHz clock */
        CHECK_RETURN(whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1, &byte_data) );
        if ( (byte_data & 0x1) != 0 )
        {
            CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1,
                                                      byte_data | SDIO_SPEED_EHS) );
        }
        else
        {
            WPRINT_WHD_ERROR( ("Error reading bus register, %s failed at %d \n", __func__, __LINE__) );
            return WHD_BUS_READ_REGISTER_ERROR;
        }
    }/* HIGH_SPEED_SDIO_CLOCK */



    /* Wait till the backplane is ready */
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1,
                                                    &byte_data) ) == WHD_SUCCESS ) &&
            ( (byte_data & SDIO_FUNC_READY_1) == 0 ) &&
            (loop_count < (uint32_t)F1_AVAIL_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }
    if (loop_count >= (uint32_t)F1_AVAIL_TIMEOUT_MS)
    {
        WPRINT_WHD_ERROR( ("Timeout while waiting for backplane to be ready\n") );
        return WHD_TIMEOUT;
    }
    CHECK_RETURN(result);

    /* Set the ALP */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                              (uint32_t)(SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ |
                                                         SBSDIO_FORCE_ALP) ) );

    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                    &byte_data) ) == WHD_SUCCESS ) &&
            ( (byte_data & SBSDIO_ALP_AVAIL) == 0 ) &&
            (loop_count < (uint32_t)ALP_AVAIL_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }
    if (loop_count >= (uint32_t)ALP_AVAIL_TIMEOUT_MS)
    {
        WPRINT_WHD_ERROR( ("Timeout while waiting for alp clock\n") );
        return WHD_TIMEOUT;
    }
    CHECK_RETURN(result);

    /* Clear request for ALP */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1, 0) );

    /* Disable the extra SDIO pull-ups */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_PULL_UP, (uint8_t)1, 0) );
    /* Enable F1 and F2 */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1,
                                              SDIO_FUNC_ENABLE_1 | SDIO_FUNC_ENABLE_2) );

    /* Setup host-wake signals */
    CHECK_RETURN(whd_bus_sdio_init_oob_intr(whd_driver) );

    /* Enable F2 interrupt only */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_INTEN, (uint8_t)1,
                                              INTR_CTL_MASTER_EN | INTR_CTL_FUNC2_EN) );

    CHECK_RETURN(whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1, &byte_data) );

#ifndef DM_43022C1
    /* Check if chip supports CHIPID Read from SDIO core and bootloader handshake */
    CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BRCM_CARDCAP, (uint8_t)1,
                                              &byte_data) );

    if ( (byte_data & SDIOD_CCCR_BRCM_CARDCAP_SECURE_MODE) != 0 )
    {
        WPRINT_WHD_INFO( ("Chip supports bootloader handshake \n") );
    }

    if ( (byte_data & SDIOD_CCCR_BRCM_CARDCAP_CHIPID_PRESENT) != 0 )
    {
        uint8_t addrlow, addrmid, addrhigh;
        uint32_t reg_addr;
        uint8_t devctl;
        whd_driver->chip_info.chipid_in_sdiocore = 1;
        CHECK_RETURN(whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_DEVICE_CTL, 1, &devctl) );
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_DEVICE_CTL,
                                                  (uint8_t)1, (devctl | SBSDIO_DEVCTL_ADDR_RST) ) );

        CHECK_RETURN(whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_FUNC1_SBADDRLOW, 1,
                                                      &addrlow) );
        CHECK_RETURN(whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_FUNC1_SBADDRMID, 1,
                                                      &addrmid) );
        CHECK_RETURN(whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_FUNC1_SBADDRHIGH, 1,
                                                      &addrhigh) );

        reg_addr = ( (addrlow << 8) | (addrmid << 16) | (addrhigh << 24) ) + SDIO_CORE_CHIPID_REG;

        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SBSDIO_DEVICE_CTL,
                                                  (uint8_t)1, devctl) );
        CHECK_RETURN(whd_bus_read_backplane_value(whd_driver, reg_addr, 2, (uint8_t *)&chip_id) );
        whd_chip_set_chip_id(whd_driver, chip_id);
        WPRINT_WHD_INFO( ("chip ID: %d, Support ChipId Read from SDIO Core \n", chip_id) );
    }
    else
    {
        /* Read the chip id */
        CHECK_RETURN(whd_bus_read_backplane_value(whd_driver, CHIPCOMMON_BASE_ADDRESS, 2, (uint8_t *)&chip_id) );
        whd_chip_set_chip_id(whd_driver, chip_id);
        WPRINT_WHD_INFO( ("chip ID: %d \n", chip_id) );
    }
#else

    CHECK_RETURN(whd_bus_read_register_value (whd_driver, BACKPLANE_FUNCTION, SBSDIO_FUNC1_SECURE_MODE, (uint8_t)1,
                                              &secure_chk) );
    if ( secure_chk == 0 )	/* Secure mode register -  will be updated later */
    {
        WPRINT_WHD_INFO( ("43022DM : Chip supports bootloader handshake \n") );
    }
    else
    {
        WPRINT_WHD_ERROR( ("43022DM : Error in the Secure Bootloader Check \n") );
    }

    /* Read the chip id */
    CHECK_RETURN(whd_bus_read_backplane_value(whd_driver, CHIPCOMMON_BASE_ADDRESS, 2, (uint8_t *)&chip_id) );
    whd_chip_set_chip_id(whd_driver, chip_id);
    WPRINT_WHD_INFO( ("chip ID: %d \n", chip_id) );

#endif

    cy_rtos_get_time(&elapsed_time);
    result = whd_bus_sdio_download_firmware(whd_driver);
    cy_rtos_get_time(&current_time);
    elapsed_time = current_time - elapsed_time;
    CHECK_RETURN(whd_resource_size(whd_driver, WHD_RESOURCE_WLAN_FIRMWARE, &wifi_firmware_image_size) );
    WPRINT_WHD_INFO( ("WLAN FW download size: %" PRIu32 " bytes\n", wifi_firmware_image_size) );
    WPRINT_WHD_INFO( ("WLAN FW download time: %" PRIu32 " ms\n", elapsed_time) );

    if (result != WHD_SUCCESS)
    {
        /*  either an error or user abort */
        WPRINT_WHD_ERROR( ("SDIO firmware download error, %s failed at %d \n", __func__, __LINE__) );
        return result;
    }

    /* Wait for F2 to be ready */
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1,
                                                    &byte_data) ) == WHD_SUCCESS ) &&
            ( (byte_data & SDIO_FUNC_READY_2) == 0 ) &&
            (loop_count < (uint32_t)F2_READY_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }
    if (loop_count >= (uint32_t)F2_READY_TIMEOUT_MS)
    {
        /* If your system fails here, it could be due to incorrect NVRAM variables.
         * Check which 'wifi_nvram_image.h' file your platform is using, and
         * check that it matches the WLAN device on your platform, including the
         * crystal frequency.
         */
        WPRINT_WHD_ERROR( ("Timeout while waiting for function 2 to be ready\n") );
        /* Reachable after hitting assert */
        return WHD_TIMEOUT;
    }
    if (whd_driver->aligned_addr == NULL)
    {
        if ( (aligned_addr = whd_mem_malloc(WHD_LINK_MTU) ) == NULL )
        {
            WPRINT_WHD_ERROR( ("Memory allocation failed for aligned_addr in %s \n", __FUNCTION__) );
            return WHD_MALLOC_FAILURE;
        }
        whd_driver->aligned_addr = aligned_addr;
    }
    result = whd_chip_specific_init(whd_driver);
    if (result != WHD_SUCCESS)
    {
        whd_mem_free(whd_driver->aligned_addr);
        whd_driver->aligned_addr = NULL;
    }
    CHECK_RETURN(result);
    result = whd_ensure_wlan_bus_is_up(whd_driver);
    if (result != WHD_SUCCESS)
    {
        whd_mem_free(whd_driver->aligned_addr);
        whd_driver->aligned_addr = NULL;
    }
    CHECK_RETURN(result);
#if (CYHAL_API_VERSION >= 2)
    cyhal_sdio_enable_event(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, CYHAL_ISR_PRIORITY_DEFAULT,
                            WHD_TRUE);
#else
    cyhal_sdio_irq_enable(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, WHD_TRUE);
#endif
    UNUSED_PARAMETER(elapsed_time);

    return result;
}

whd_result_t whd_bus_sdio_deinit(whd_driver_t whd_driver)
{
    if (whd_driver->aligned_addr)
    {
        whd_mem_free(whd_driver->aligned_addr);
        whd_driver->aligned_addr = NULL;
    }

    CHECK_RETURN(whd_bus_sdio_deinit_oob_intr(whd_driver) );
#if (CYHAL_API_VERSION >= 2)
    cyhal_sdio_enable_event(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, CYHAL_ISR_PRIORITY_DEFAULT,
                            WHD_FALSE);
#else
    cyhal_sdio_irq_enable(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, WHD_FALSE);
#endif

    CHECK_RETURN(whd_allow_wlan_bus_to_sleep(whd_driver) );
    whd_bus_set_resource_download_halt(whd_driver, WHD_FALSE);

    DELAYED_BUS_RELEASE_SCHEDULE(whd_driver, WHD_FALSE);

    return WHD_SUCCESS;
}

whd_bool_t whd_bus_sdio_wake_interrupt_present(whd_driver_t whd_driver)
{
    uint32_t int_status = 0;

    /* Ensure the wlan backplane bus is up */
    if (WHD_SUCCESS != whd_ensure_wlan_bus_is_up(whd_driver) )
        return WHD_FALSE;

    if (whd_bus_read_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                     (uint8_t *)&int_status) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("%s: Error reading interrupt status\n", __FUNCTION__) );
        goto exit;
    }
    if ( (I_HMB_HOST_INT & int_status) != 0 )
    {
        /* Clear any interrupts */
        if (whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                          I_HMB_HOST_INT) != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Error clearing interrupts\n", __FUNCTION__) );
            goto exit;
        }
        if (whd_bus_read_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                         (uint8_t *)&int_status) != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Error reading interrupt status\n", __FUNCTION__) );
            goto exit;
        }
        WPRINT_WHD_DEBUG( ("whd_bus_sdio_wake_interrupt_present after clearing int_status  = [%x]\n",
                           (uint8_t)int_status) );
        return WHD_TRUE;
    }
exit:
    return WHD_FALSE;
}

uint32_t whd_bus_sdio_packet_available_to_read(whd_driver_t whd_driver)
{
    uint32_t int_status = 0;
    uint32_t hmb_data = 0;
    uint8_t error_type = 0;
    whd_bt_dev_t btdev = whd_driver->bt_dev;
#ifdef CYCFG_ULP_SUPPORT_ENABLED
    uint16_t wlan_chip_id;
    wlan_chip_id = whd_chip_get_chip_id(whd_driver);
#endif

    /* Ensure the wlan backplane bus is up */
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );

    /* Read the IntStatus */
    if (whd_bus_read_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                     (uint8_t *)&int_status) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("%s: Error reading interrupt status\n", __FUNCTION__) );
        int_status = 0;
        return WHD_BUS_FAIL;
    }

    if ( (I_HMB_HOST_INT & int_status) != 0 )
    {
        /* Read mailbox data and ack that we did so */
        if ((whd_bus_read_backplane_value(whd_driver,  SDIO_TO_HOST_MAILBOX_DATA(whd_driver), 4,
                                         (uint8_t *)&hmb_data) == WHD_SUCCESS) && (hmb_data > 0))
            if (whd_bus_write_backplane_value(whd_driver, SDIO_TO_SB_MAILBOX(whd_driver), (uint8_t)4,
                                              SMB_INT_ACK) != WHD_SUCCESS)
                WPRINT_WHD_ERROR( ("%s: Failed writing SMB_INT_ACK\n", __FUNCTION__) );

        /* dongle indicates the firmware has halted/crashed */
        if ( (I_HMB_DATA_FWHALT & hmb_data) != 0 )
        {
            WPRINT_WHD_ERROR( ("%s: mailbox indicates firmware halted\n", __FUNCTION__) );
            whd_wifi_print_whd_log(whd_driver);
            error_type = WLC_ERR_FW;
            whd_set_error_handler_locally(whd_driver, &error_type, NULL, NULL, NULL);
        }
#ifdef CYCFG_ULP_SUPPORT_ENABLED
        else if(hmb_data == 0 && ( wlan_chip_id == 43012 || wlan_chip_id == 43022 ))
        {
            WPRINT_WHD_DEBUG( ("%s: mailbox indication about DS1/DS2 Exit\n", __FUNCTION__) );
            if(whd_driver->ds_exit_in_progress == WHD_FALSE)
            {
                if(whd_wlan_bus_complete_ds_wake(whd_driver, true) == WHD_SUCCESS)
                {
                    WPRINT_WHD_DEBUG(("DS EXIT Triggered: Start Re-Downloading Firmware \n"));
                    /* If we are running LPA, then LPA will disable MCU SDIO clock.So, re-dowanload fails.
                     in ordet to avoid this, releasing the clock before FW re-download */
                    cyhal_syspm_lock_deepsleep();
                    whd_sdpcm_quit(whd_driver);
                    /* Re-Download Firmware no need to check return, as it affects sync of whd thread, error will be thrown */
                    whd_bus_reinit_stats(whd_driver, true);
                    cyhal_syspm_unlock_deepsleep();
                    /* Notify the network activity callback to not to miss network packets, if any
                       (TBD) This place is temporary, this will be fixed later */
                    cy_network_activity_notify(CY_NETWORK_ACTIVITY_RX);
                }
            }
            else
            {
                 WPRINT_WHD_ERROR(("DS1/DS2 Exit(FW Re-download) is already in progress \n"));
            }
        }

        if (whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                            (int_status & I_HMB_HOST_INT)) != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Error clearing interrupts\n", __FUNCTION__) );
            int_status = 0;
            goto exit;
        }
        int_status &= ~I_HMB_HOST_INT;

        if ((int_status & I_HMB_FC_STATE) != 0 )
        {
            WPRINT_WHD_DEBUG(("Dongle reports I_HMB_FC_STATE\n"));
            if (whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                          (int_status & I_HMB_FC_STATE)) != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("%s: Error clearing interrupts\n", __FUNCTION__) );
                int_status = 0;
                goto exit;
            }
            int_status &= ~I_HMB_FC_STATE;
       }
#endif	/* CYCFG_ULP_SUPPORT_ENABLED */

    }

    if (btdev && btdev->bt_int_cb)
    {
        if ( (I_HMB_FC_CHANGE & int_status) != 0 )
        {
            btdev->bt_int_cb(btdev->bt_data);
            int_status = 0;
        }
    }

    if ( (HOSTINTMASK & int_status) != 0 )
    {
        /* Clear any interrupts */
        if (whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                          int_status & HOSTINTMASK) != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Error clearing interrupts\n", __FUNCTION__) );
            int_status = 0;
            goto exit;
        }
    }
exit:
    return ( (int_status) & (FRAME_AVAILABLE_MASK) );
}

/*
 * From internal documentation: hwnbu-twiki/SdioMessageEncapsulation
 * When data is available on the device, the device will issue an interrupt:
 * - the device should signal the interrupt as a hint that one or more data frames may be available on the device for reading
 * - the host may issue reads of the 4 byte length tag at any time -- that is, whether an interupt has been issued or not
 * - if a frame is available, the tag read should return a nonzero length (>= 4) and the host can then read the remainder of the frame by issuing one or more CMD53 reads
 * - if a frame is not available, the 4byte tag read should return zero
 */
whd_result_t whd_bus_sdio_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer)
{
    uint16_t hwtag[8];
    uint16_t extra_space_required;
    whd_result_t result;
    uint8_t *data = NULL;

    *buffer = NULL;

    /* Ensure the wlan backplane bus is up */
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );

    /* Read the frame header and verify validity */
    memset(hwtag, 0, sizeof(hwtag) );

    result = whd_bus_sdio_transfer(whd_driver, BUS_READ, WLAN_FUNCTION, 0, (uint16_t)INITIAL_READ, (uint8_t *)hwtag,
                                   RESPONSE_NEEDED);
    if (result != WHD_SUCCESS)
    {
        (void)whd_bus_sdio_abort_read(whd_driver, WHD_FALSE);    /* ignore return - not much can be done if this fails */
        WPRINT_WHD_ERROR( ("Error during SDIO receive, %s failed at %d \n", __func__, __LINE__) );
        return WHD_SDIO_RX_FAIL;
    }

    if ( ( (hwtag[0] | hwtag[1]) == 0 ) ||
         ( (hwtag[0] ^ hwtag[1]) != (uint16_t)0xFFFF ) )
    {
        return WHD_HWTAG_MISMATCH;
    }

    if ( (hwtag[0] == (uint16_t)12) &&
         (whd_driver->internal_info.whd_wlan_status.state == WLAN_UP) )
    {
        result = whd_bus_sdio_transfer(whd_driver, BUS_READ, WLAN_FUNCTION, 0, (uint16_t)8, (uint8_t *)&hwtag[2],
                                       RESPONSE_NEEDED);
        if (result != WHD_SUCCESS)
        {
            /* ignore return - not much can be done if this fails */
            (void)whd_bus_sdio_abort_read(whd_driver, WHD_FALSE);
            WPRINT_WHD_ERROR( ("Error during SDIO receive, %s failed at %d \n", __func__, __LINE__) );
            return WHD_SDIO_RX_FAIL;
        }
        whd_sdpcm_update_credit(whd_driver, (uint8_t *)hwtag);
        return WHD_SUCCESS;
    }

    /* Calculate the space we need to store entire packet */
    if ( (hwtag[0] > (uint16_t)INITIAL_READ) )
    {
        extra_space_required = (uint16_t)(hwtag[0] - (uint16_t)INITIAL_READ);
    }
    else
    {
        extra_space_required = 0;
    }

    /* Allocate a suitable buffer */
    result = whd_host_buffer_get(whd_driver, buffer, WHD_NETWORK_RX,
                                 (uint16_t)(INITIAL_READ + extra_space_required + sizeof(whd_buffer_header_t) ),
                                 (whd_sdpcm_has_tx_packet(whd_driver) ? 0 : WHD_RX_BUF_TIMEOUT) );
    if (result != WHD_SUCCESS)
    {
        /* Read out the first 12 bytes to get the bus credit information, 4 bytes are already read in hwtag */
        whd_assert("Get buffer error",
                   ( (result == WHD_BUFFER_UNAVAILABLE_TEMPORARY) || (result == WHD_BUFFER_UNAVAILABLE_PERMANENT) ) );
        result = whd_bus_sdio_transfer(whd_driver, BUS_READ, WLAN_FUNCTION, 0, (uint16_t)8, (uint8_t *)&hwtag[2],
                                       RESPONSE_NEEDED);
        if (result != WHD_SUCCESS)
        {
            /* ignore return - not much can be done if this fails */
            (void)whd_bus_sdio_abort_read(whd_driver, WHD_FALSE);
            WPRINT_WHD_ERROR( ("Error during SDIO receive, %s failed at %d \n", __func__, __LINE__) );
            return WHD_SDIO_RX_FAIL;
        }
        result = whd_bus_sdio_abort_read(whd_driver, WHD_FALSE);
        whd_assert("Read-abort failed", result == WHD_SUCCESS);
        REFERENCE_DEBUG_ONLY_VARIABLE(result);

        whd_sdpcm_update_credit(whd_driver, (uint8_t *)hwtag);
        WPRINT_WHD_ERROR( ("Failed to allocate a buffer to receive into, %s failed at %d \n", __func__, __LINE__) );
        return WHD_RX_BUFFER_ALLOC_FAIL;
    }
    data = whd_buffer_get_current_piece_data_pointer(whd_driver, *buffer);
    CHECK_PACKET_NULL(data, WHD_NO_REGISTER_FUNCTION_POINTER);
    /* Copy the data already read */
    memcpy(data + sizeof(whd_buffer_header_t), hwtag, (size_t)INITIAL_READ);

    /* Read the rest of the data */
    if (extra_space_required > 0)
    {
        data = whd_buffer_get_current_piece_data_pointer(whd_driver, *buffer);
        CHECK_PACKET_NULL(data, WHD_NO_REGISTER_FUNCTION_POINTER);
        result = whd_bus_sdio_transfer(whd_driver, BUS_READ, WLAN_FUNCTION, 0, extra_space_required,
                                       data + sizeof(whd_buffer_header_t) +
                                       INITIAL_READ, RESPONSE_NEEDED);

        if (result != WHD_SUCCESS)
        {
            (void)whd_bus_sdio_abort_read(whd_driver, WHD_FALSE);     /* ignore return - not much can be done if this fails */
            CHECK_RETURN(whd_buffer_release(whd_driver, *buffer, WHD_NETWORK_RX) );
            WPRINT_WHD_ERROR( ("Error during SDIO receive, %s failed at %d \n", __func__, __LINE__) );
            return WHD_SDIO_RX_FAIL;
        }
    }
    DELAYED_BUS_RELEASE_SCHEDULE(whd_driver, WHD_TRUE);
    return WHD_SUCCESS;
}

/******************************************************
*     Function definitions for Protocol Common
******************************************************/

/* Device register access functions */
whd_result_t whd_bus_sdio_write_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                                uint32_t value)
{
    CHECK_RETURN(whd_bus_set_backplane_window(whd_driver, address) );

    address &= SBSDIO_SB_OFT_ADDR_MASK;

    if (register_length == 4)
        address |= SBSDIO_SB_ACCESS_2_4B_FLAG;

    CHECK_RETURN(whd_bus_sdio_transfer(whd_driver, BUS_WRITE, BACKPLANE_FUNCTION, address, register_length,
                                       (uint8_t *)&value, RESPONSE_NEEDED) );

    return whd_bus_set_backplane_window(whd_driver, CHIPCOMMON_BASE_ADDRESS);
}

whd_result_t whd_bus_sdio_read_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                               uint8_t *value)
{
    *value = 0;
    CHECK_RETURN(whd_bus_set_backplane_window(whd_driver, address) );

    address &= SBSDIO_SB_OFT_ADDR_MASK;

    if (register_length == 4)
        address |= SBSDIO_SB_ACCESS_2_4B_FLAG;

    CHECK_RETURN(whd_bus_sdio_transfer(whd_driver, BUS_READ, BACKPLANE_FUNCTION, address, register_length, value,
                                       RESPONSE_NEEDED) );

    return whd_bus_set_backplane_window(whd_driver, CHIPCOMMON_BASE_ADDRESS);
}

whd_result_t whd_bus_sdio_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                               uint8_t value_length, uint32_t value)
{
    return whd_bus_sdio_transfer(whd_driver, BUS_WRITE, function, address, value_length, (uint8_t *)&value,
                                 RESPONSE_NEEDED);
}

whd_result_t whd_bus_sdio_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                         whd_bus_function_t function, uint32_t address, uint16_t size,
                                         whd_transfer_bytes_packet_t *data)
{
    DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
    return whd_bus_sdio_transfer(whd_driver, direction, function, address, size, (uint8_t *)data->data,
                                 RESPONSE_NEEDED);
    ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
}

/******************************************************
*             Static  Function definitions
******************************************************/

static whd_result_t whd_bus_sdio_transfer(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                          whd_bus_function_t function, uint32_t address, uint16_t data_size,
                                          uint8_t *data, sdio_response_needed_t response_expected)
{
    /* Note: this function had broken retry logic (never retried), which has been removed.
     * Failing fast helps problems on the bus get brought to light more quickly
     * and preserves the original behavior.
     */
    whd_result_t result = WHD_SUCCESS;
    uint16_t data_byte_size;
    uint16_t data_blk_size;

    if (data_size == 0)
    {
        return WHD_BADARG;
    }
    else if (data_size == (uint16_t)1)
    {
        return whd_bus_sdio_cmd52(whd_driver, direction, function, address, *data, response_expected, data);
    }
    else if (whd_driver->internal_info.whd_wlan_status.state == WLAN_UP)
    {
        return whd_bus_sdio_cmd53(whd_driver, direction, function,
                                  (data_size >= (uint16_t)64) ? SDIO_BLOCK_MODE : SDIO_BYTE_MODE, address, data_size,
                                  data, response_expected, NULL);
    }
    else
    {
        /* We need to handle remaining size for source image download */
        data_byte_size = data_size % SDIO_64B_BLOCK;
        data_blk_size = data_size - data_byte_size;
        if (data_blk_size != 0)
        {
            result = whd_bus_sdio_cmd53(whd_driver, direction, function, SDIO_BLOCK_MODE, address,
                                        data_blk_size, data, response_expected, NULL);
            if (result != WHD_SUCCESS)
            {
                return result;
            }
            data += data_blk_size;
            address += data_blk_size;
        }
        if (data_byte_size)
        {
            result = whd_bus_sdio_cmd53(whd_driver, direction, function, SDIO_BYTE_MODE, address,
                                        data_byte_size, data, response_expected, NULL);
        }
        return result;
    }
}

static whd_result_t whd_bus_sdio_cmd52(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                       whd_bus_function_t function, uint32_t address, uint8_t value,
                                       sdio_response_needed_t response_expected, uint8_t *response)
{
    uint32_t sdio_response = 0;
    whd_result_t result;
    sdio_cmd_argument_t arg;
    arg.value = 0;
    arg.cmd52.function_number = (uint32_t)(function & BUS_FUNCTION_MASK);
    arg.cmd52.register_address = (uint32_t)(address & 0x00001ffff);
    arg.cmd52.rw_flag = (uint32_t)( (direction == BUS_WRITE) ? 1 : 0 );
    arg.cmd52.write_data = value;

    WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, cmd52);
    result = cyhal_sdio_send_cmd(whd_driver->bus_priv->sdio_obj, (cyhal_sdio_transfer_t)direction,
                                 CYHAL_SDIO_CMD_IO_RW_DIRECT, arg.value,
                                 &sdio_response);
    WHD_BUS_STATS_CONDITIONAL_INCREMENT_VARIABLE(whd_driver->bus_priv, (result != WHD_SUCCESS), cmd52_fail);

    if (response != NULL)
    {
        *response = (uint8_t)(sdio_response & 0x00000000ff);
    }

    /* Possibly device might not respond to this cmd. So, don't check return value here */
    if ( (result != WHD_SUCCESS) && (address == SDIO_SLEEP_CSR) )
    {
        return result;
    }

    CHECK_RETURN(result);
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_cmd53(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                       whd_bus_function_t function, sdio_transfer_mode_t mode, uint32_t address,
                                       uint16_t data_size, uint8_t *data,
                                       sdio_response_needed_t response_expected, uint32_t *response)
{
    sdio_cmd_argument_t arg;
    whd_result_t result;

    if (direction == BUS_WRITE)
    {
        WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, cmd53_write);
    }

    arg.value = 0;
    arg.cmd53.function_number = (uint32_t)(function & BUS_FUNCTION_MASK);
    arg.cmd53.register_address = (uint32_t)(address & BIT_MASK(17) );
    arg.cmd53.op_code = (uint32_t)1;
    arg.cmd53.rw_flag = (uint32_t)( (direction == BUS_WRITE) ? 1 : 0 );

    if (mode == SDIO_BYTE_MODE)
    {
        whd_assert("whd_bus_sdio_cmd53: data_size > 512 for byte mode", (data_size <= (uint16_t )512) );
        arg.cmd53.count = (uint32_t)(data_size & 0x1FF);

        result =
            cyhal_sdio_bulk_transfer(whd_driver->bus_priv->sdio_obj, (cyhal_sdio_transfer_t)direction, arg.value,
                                     (uint32_t *)data, data_size, response);

        if (result != CY_RSLT_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s:%d cyhal_sdio_bulk_transfer SDIO_BYTE_MODE failed\n", __func__, __LINE__) );
            goto done;
        }
    }
    else
    {
        arg.cmd53.count = (uint32_t)( (data_size / (uint16_t)SDIO_64B_BLOCK) & BIT_MASK(9) );
        if ( (uint32_t)(arg.cmd53.count * (uint16_t)SDIO_64B_BLOCK) < data_size )
        {
            ++arg.cmd53.count;
        }
        arg.cmd53.block_mode = (uint32_t)1;

        result =
            cyhal_sdio_bulk_transfer(whd_driver->bus_priv->sdio_obj, (cyhal_sdio_transfer_t)direction, arg.value,
                                     (uint32_t *)data, data_size, response);

        if (result != CY_RSLT_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s:%d cyhal_sdio_bulk_transfer SDIO_BLOCK_MODE failed\n", __func__, __LINE__) );
            goto done;
        }
    }

    if (direction == BUS_READ)
    {
        WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, cmd53_read);
    }

done:
    WHD_BUS_STATS_CONDITIONAL_INCREMENT_VARIABLE(whd_driver->bus_priv,
                                                 ( (result != WHD_SUCCESS) && (direction == BUS_READ) ),
                                                 cmd53_read_fail);
    WHD_BUS_STATS_CONDITIONAL_INCREMENT_VARIABLE(whd_driver->bus_priv,
                                                 ( (result != WHD_SUCCESS) && (direction == BUS_WRITE) ),
                                                 cmd53_write_fail);
    CHECK_RETURN(result);
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_download_firmware(whd_driver_t whd_driver)
{
    uint8_t csr_val = 0;
    whd_result_t result;
    uint32_t loop_count;

#ifndef BLHS_SUPPORT
    uint32_t ram_start_address;

    ram_start_address = GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    if (ram_start_address != 0)
    {
        CHECK_RETURN(whd_reset_core(whd_driver, WLAN_ARM_CORE, SICF_CPUHALT, SICF_CPUHALT) );
    }
    else
    {
        CHECK_RETURN(whd_disable_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_NONE) );
        CHECK_RETURN(whd_disable_device_core(whd_driver, SOCRAM_CORE, WLAN_CORE_FLAG_NONE) );
        CHECK_RETURN(whd_reset_device_core(whd_driver, SOCRAM_CORE, WLAN_CORE_FLAG_NONE) );
        CHECK_RETURN(whd_chip_specific_socsram_init(whd_driver) );
    }
#endif

#ifdef BLHS_SUPPORT
    CHECK_RETURN(whd_bus_common_blhs(whd_driver, CHK_BL_INIT) );
#endif

#ifndef DM_43022C1
    result = whd_bus_write_wifi_firmware_image(whd_driver);

    if (result == WHD_UNFINISHED)
    {
        WPRINT_WHD_INFO( ("User aborted fw download\n") );
        /* user aborted */
        return result;
    }
    else if (result != WHD_SUCCESS)
    {
        whd_assert("Failed to load wifi firmware\n", result == WHD_SUCCESS);
        return result;
    }

    CHECK_RETURN(whd_bus_sdio_write_wifi_nvram_image(whd_driver) );
#else
    /* For the Chip - 43022(DM) : Download NVRAM before Firmware */
    CHECK_RETURN(whd_bus_sdio_write_wifi_nvram_image(whd_driver) );

    result = whd_bus_write_wifi_firmware_image(whd_driver);

    if (result == WHD_UNFINISHED)
    {
        WPRINT_WHD_INFO( ("User aborted fw download\n") );
        /* user aborted */
        return result;
    }
    else if (result != WHD_SUCCESS)
    {
        whd_assert("Failed to load wifi firmware\n", result == WHD_SUCCESS);
        return result;
    }
#endif

    /* Take the ARM core out of reset */
#ifndef BLHS_SUPPORT
    if (ram_start_address != 0)
    {
        CHECK_RETURN(whd_reset_core(whd_driver, WLAN_ARM_CORE, 0, 0) );
    }
    else
    {
        CHECK_RETURN(whd_reset_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_NONE) );

        result = whd_device_core_is_up(whd_driver, WLAN_ARM_CORE);
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("Could not bring ARM core up\n") );
            /* Reachable after hitting assert */
            return result;
        }
    }
#endif
    /* Wait until the High Throughput clock is available */
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                    &csr_val) ) == WHD_SUCCESS ) &&
            ( (csr_val & SBSDIO_HT_AVAIL) == 0 ) &&
            (loop_count < (uint32_t)HT_AVAIL_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }
    if (loop_count >= (uint32_t)HT_AVAIL_TIMEOUT_MS)
    {
        /* If your system times out here, it means that the WLAN firmware is not booting.
         * Check that your WLAN chip matches the 'wifi_image.c' being built - in GNU toolchain, $(CHIP)
         * makefile variable must be correct.
         */
        WPRINT_WHD_ERROR( ("Timeout while waiting for high throughput clock\n") );
        /* Reachable after hitting assert */
        return WHD_TIMEOUT;
    }
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Error while waiting for high throughput clock\n") );
        /* Reachable after hitting assert */
        return result;
    }

    /* Set up the interrupt mask and enable interrupts */
    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, SDIO_INT_HOST_MASK(whd_driver), (uint8_t)4, HOSTINTMASK) );

    /* Enable F2 interrupts. This wasn't required for 4319 but is for the 43362 */
    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, SDIO_FUNCTION_INT_MASK(whd_driver), (uint8_t)1,
                                               SDIO_FUNC_MASK_F1 | SDIO_FUNC_MASK_F2) );

    /* Lower F2 Watermark to avoid DMA Hang in F2 when SD Clock is stopped. */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_FUNCTION2_WATERMARK, (uint8_t)1,
                                              (uint32_t)SDIO_F2_WATERMARK) );

    return WHD_SUCCESS;
}

/** Aborts a SDIO read of a packet from the 802.11 device
 *
 * This function is necessary because the only way to obtain the size of the next
 * available received packet is to read the first four bytes of the packet.
 * If the system reads these four bytes, and then fails to allocate the required
 * memory, then this function allows the system to abort the packet read cleanly,
 * and to optionally tell the 802.11 device to keep it allowing reception once
 * memory is available.
 *
 * In order to do this abort, the following actions are performed:
 * - Sets abort bit for Function 2 (WLAN Data) to request stopping transfer
 * - Sets Read Frame Termination bit to flush and reset fifos
 * - If packet is to be kept and resent by 802.11 device, a NAK  is sent
 * - Wait whilst the Fifo is emptied of the packet ( reading during this period would cause all zeros to be read )
 *
 * @param retry : WHD_TRUE if 802.11 device is to keep and resend packet
 *                WHD_FALSE if 802.11 device is to drop packet
 *
 * @return WHD_SUCCESS if successful, otherwise error code
 */
static whd_result_t whd_bus_sdio_abort_read(whd_driver_t whd_driver, whd_bool_t retry)
{
    WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, read_aborts);

    /* Abort transfer on WLAN_FUNCTION */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOABORT, (uint8_t)1,
                                              (uint32_t)WLAN_FUNCTION) );

    /* Send frame terminate */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_FRAME_CONTROL, (uint8_t)1,
                                              SFC_RF_TERM) );

    /* If we want to retry message, send NAK */
    if (retry == WHD_TRUE)
    {
        CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)SDIO_TO_SB_MAILBOX(whd_driver), (uint8_t)1,
                                                   SMB_NAK) );
    }

    return WHD_SUCCESS;
}

whd_result_t whd_bus_sdio_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                              uint8_t value_length, uint8_t *value)
{
    memset(value, 0, (size_t)value_length);
    return whd_bus_sdio_transfer(whd_driver, BUS_READ, function, address, value_length, value, RESPONSE_NEEDED);
}

whd_result_t whd_bus_sdio_poke_wlan(whd_driver_t whd_driver)
{
    return whd_bus_write_backplane_value(whd_driver, SDIO_TO_SB_MAILBOX(whd_driver), (uint8_t)4, SMB_DEV_INT);
}

whd_result_t whd_bus_sdio_wakeup(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}

whd_result_t whd_bus_sdio_sleep(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_set_oob_interrupt(whd_driver_t whd_driver, uint8_t gpio_pin_number)
{
    if (gpio_pin_number != 0)
    {
        /* Redirect to OOB interrupt to GPIO1 */
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_GPIO_SELECT, (uint8_t)1,
                                                  (uint32_t)0xF) );
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_GPIO_OUTPUT, (uint8_t)1,
                                                  (uint32_t)0x0) );

        /* Enable GPIOx (bit x) */
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_GPIO_ENABLE, (uint8_t)1,
                                                  (uint32_t)0x2) );

        /* Set GPIOx (bit x) on Chipcommon GPIO Control register */
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, CHIPCOMMON_GPIO_CONTROL, (uint8_t)4,
                                                  (uint32_t)0x2) );
    }

    return WHD_SUCCESS;
}

void whd_bus_sdio_init_stats(whd_driver_t whd_driver)
{
    memset(&whd_driver->bus_priv->whd_bus_stats, 0, sizeof(whd_bus_stats_t) );
}

whd_result_t whd_bus_sdio_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    WPRINT_MACRO( ("Bus Stats.. \n"
                   "cmd52:%" PRIu32 ", cmd53_read:%" PRIu32 ", cmd53_write:%" PRIu32 "\n"
                   "cmd52_fail:%" PRIu32 ", cmd53_read_fail:%" PRIu32 ", cmd53_write_fail:%" PRIu32 "\n"
                   "oob_intrs:%" PRIu32 ", sdio_intrs:%" PRIu32 ", error_intrs:%" PRIu32 ", read_aborts:%" PRIu32
                   "\n",
                   whd_driver->bus_priv->whd_bus_stats.cmd52, whd_driver->bus_priv->whd_bus_stats.cmd53_read,
                   whd_driver->bus_priv->whd_bus_stats.cmd53_write,
                   whd_driver->bus_priv->whd_bus_stats.cmd52_fail,
                   whd_driver->bus_priv->whd_bus_stats.cmd53_read_fail,
                   whd_driver->bus_priv->whd_bus_stats.cmd53_write_fail,
                   whd_driver->bus_priv->whd_bus_stats.oob_intrs,
                   whd_driver->bus_priv->whd_bus_stats.sdio_intrs,
                   whd_driver->bus_priv->whd_bus_stats.error_intrs,
                   whd_driver->bus_priv->whd_bus_stats.read_aborts) );

    if (reset_after_print == WHD_TRUE)
    {
        memset(&whd_driver->bus_priv->whd_bus_stats, 0, sizeof(whd_bus_stats_t) );
    }

#ifdef CYCFG_ULP_SUPPORT_ENABLED
    if (whd_driver->ds_cb_info.callback != NULL)
    {
        CHECK_RETURN(whd_wifi_get_deepsleep_stats(whd_driver, whd_driver->ds_cb_info.buf, whd_driver->ds_cb_info.buflen) );
    }
    else
    {
        WPRINT_WHD_ERROR( ("No callback registered") );
    }
#endif

    return WHD_SUCCESS;
}

/* Waking the firmware up from Deep Sleep */
whd_result_t whd_bus_sdio_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    whd_result_t result = WHD_SUCCESS;
    uint8_t byte_data;
    uint32_t loop_count;
    loop_count = 0;

    /* Setup the backplane*/
    loop_count = 0;

    do
    {
        /* Enable function 1 (backplane) */
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1,
                                                  SDIO_FUNC_ENABLE_1) );
        if (loop_count != 0)
        {
            (void)cy_rtos_delay_milliseconds( (uint32_t)1 );  /* Ignore return - nothing can be done if it fails */
        }

        CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1, &byte_data) );
        loop_count++;
        if (loop_count >= (uint32_t)F0_WORKING_TIMEOUT_MS)
        {
            WPRINT_WHD_ERROR( ("Timeout on CCCR update\n") );
            return WHD_TIMEOUT;
        }
    } while (byte_data != (uint8_t)SDIO_FUNC_ENABLE_1);

    if (whd_driver->bus_priv->sdio_config.sdio_1bit_mode == WHD_FALSE)
    {
        /* Read the bus width and set to 4 bits */
        CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BICTRL, (uint8_t)1,
                                                  &byte_data) );
        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BICTRL, (uint8_t)1,
                                                  (byte_data & (~BUS_SD_DATA_WIDTH_MASK) ) | BUS_SD_DATA_WIDTH_4BIT) );
        /* NOTE: We don't need to change our local bus settings since we're not sending any data (only using CMD52)
         * until after we change the bus speed further down */
    }

    /* Set the block size */
    /* Wait till the backplane is ready */
    loop_count = 0;
    while ( ( (result = whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0, (uint8_t)1,
                                                     (uint32_t)SDIO_64B_BLOCK) ) == WHD_SUCCESS ) &&
            ( (result = whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0, (uint8_t)1,
                                                     &byte_data) ) == WHD_SUCCESS ) &&
            (byte_data != (uint8_t)SDIO_64B_BLOCK) &&
            (loop_count < (uint32_t)F0_WORKING_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );    /* Ignore return - nothing can be done if it fails */
        loop_count++;
        if (loop_count >= (uint32_t)F0_WORKING_TIMEOUT_MS)
        {
            /* If the system fails here, check the high frequency crystal is working */
            WPRINT_WHD_ERROR( ("Timeout while setting block size\n") );
            return WHD_TIMEOUT;
        }
    }

    CHECK_RETURN(result);

    WPRINT_WHD_DEBUG( ("Modding registers for blocks\n") );

    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BLKSIZE_0,   (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F1BLKSIZE_0, (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F2BLKSIZE_0, (uint8_t)1,
                                              (uint32_t)SDIO_64B_BLOCK) );
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_F2BLKSIZE_1, (uint8_t)1,
                                              (uint32_t)0) );                                                                                  /* Function 2 = 64 */

    /* Enable/Disable Client interrupts */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_INTEN,       (uint8_t)1,
                                              INTR_CTL_MASTER_EN | INTR_CTL_FUNC1_EN | INTR_CTL_FUNC2_EN) );


    if (whd_driver->bus_priv->sdio_config.high_speed_sdio_clock)
    {
        WPRINT_WHD_DEBUG( ("SDIO HS clock enable\n") );

        /* This code is required if we want more than 25 MHz clock */
        CHECK_RETURN(whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1, &byte_data) );
        if ( (byte_data & 0x1) != 0 )
        {
            CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_SPEED_CONTROL, 1,
                                                      byte_data | SDIO_SPEED_EHS) );
        }
        else
        {
            WPRINT_WHD_ERROR( ("Error writing to WLAN register, %s failed at %d \n", __func__, __LINE__) );
            return WHD_BUS_READ_REGISTER_ERROR;
        }
    } /* HIGH_SPEED_SDIO_CLOCK */

    /* Wait till the backplane is ready */
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1,
                                                    &byte_data) ) == WHD_SUCCESS ) &&
            ( (byte_data & SDIO_FUNC_READY_1) == 0 ) &&
            (loop_count < (uint32_t)F1_AVAIL_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }

    if (loop_count >= (uint32_t)F1_AVAIL_TIMEOUT_MS)
    {
        WPRINT_WHD_ERROR( ("Timeout while waiting for backplane to be ready\n") );
        return WHD_TIMEOUT;
    }
    CHECK_RETURN(result);

    /* Set the ALP */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                              (uint32_t)(SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ |
                                                         SBSDIO_FORCE_ALP) ) );
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                    &byte_data) ) != WHD_SUCCESS ) ||
            ( ( (byte_data & SBSDIO_ALP_AVAIL) == 0 ) &&
              (loop_count < (uint32_t)ALP_AVAIL_TIMEOUT_MS) ) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }
    if (loop_count >= (uint32_t)ALP_AVAIL_TIMEOUT_MS)
    {
        WPRINT_WHD_ERROR( ("Timeout while waiting for alp clock\n") );
        return WHD_TIMEOUT;
    }
    CHECK_RETURN(result);

    /* Clear request for ALP */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1, 0) );

    /* Disable the extra SDIO pull-ups */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_PULL_UP, (uint8_t)1, 0) );

    /* Enable F1 and F2 */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOEN, (uint8_t)1,
                                              SDIO_FUNC_ENABLE_1 | SDIO_FUNC_ENABLE_2) );
    /* Enable F2 interrupt only */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_INTEN, (uint8_t)1,
                                              INTR_CTL_MASTER_EN | INTR_CTL_FUNC2_EN) );

    CHECK_RETURN(whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1, &byte_data) );
    WPRINT_WHD_DEBUG(("FW RE-DOWNL STARTS \n"));

    result = whd_bus_sdio_download_firmware(whd_driver);

    if (result != WHD_SUCCESS)
    {
        /*  either an error or user abort */
        WPRINT_WHD_ERROR( ("FW re-download failed\n") );
        return result;
    }
    else
    {
        WPRINT_WHD_ERROR( (" # DS0-FW DOWNLOAD DONE # \n") );
        whd_driver->internal_info.whd_wlan_status.state = WLAN_DOWN;
    }

    /* Wait for F2 to be ready */
    loop_count = 0;
    while ( ( (result = whd_bus_read_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IORDY, (uint8_t)1,
                                                    &byte_data) ) != WHD_SUCCESS ) ||
            ( ( (byte_data & SDIO_FUNC_READY_2) == 0 ) &&
              (loop_count < (uint32_t)F2_READY_TIMEOUT_MS) ) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)1 );   /* Ignore return - nothing can be done if it fails */
        loop_count++;
    }

    if (loop_count >= (uint32_t)F2_READY_TIMEOUT_MS)
    {
        WPRINT_WHD_DEBUG( ("Timeout while waiting for function 2 to be ready\n") );

        if (WHD_TRUE == wake_from_firmware)
        {
            /* If your system fails here, it could be due to incorrect NVRAM variables.
             * Check which 'wifi_nvram_image.h' file your platform is using, and
             * check that it matches the WLAN device on your platform, including the
             * crystal frequency.
             */
            WPRINT_WHD_ERROR( ("F2 failed on wake fr FW\n") );
            /* Reachable after hitting assert */
            return WHD_TIMEOUT;
        }
        /* Else: Ignore this failure if we're doing a reinit due to host wake: Linux DHD also ignores */

    }

    /* Update wlan state */
    whd_driver->internal_info.whd_wlan_status.state = WLAN_UP;
    /* Do chip specific init */
    CHECK_RETURN(whd_chip_specific_init(whd_driver) );

    /* Ensure Bus is up */
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );

    /* Allow bus to go to  sleep */
    CHECK_RETURN(whd_allow_wlan_bus_to_sleep(whd_driver) );

    CHECK_RETURN(whd_sdpcm_init(whd_driver));

#ifdef CYCFG_ULP_SUPPORT_ENABLED
    whd_driver->ds_exit_in_progress = WHD_FALSE;
#endif

    WPRINT_WHD_INFO( ("whd_bus_reinit Completed \n") );
    return WHD_SUCCESS;
}

uint8_t whd_bus_sdio_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return WHD_BUS_SDIO_BACKPLANE_READ_PADD_SIZE;
}

whd_bool_t whd_bus_sdio_use_status_report_scheme(whd_driver_t whd_driver)
{
    return WHD_FALSE;
}

uint32_t whd_bus_sdio_get_max_transfer_size(whd_driver_t whd_driver)
{
    return WHD_BUS_SDIO_MAX_BACKPLANE_TRANSFER_SIZE;
}

#if (CYHAL_API_VERSION >= 2)
static void whd_bus_sdio_irq_handler(void *handler_arg, cyhal_sdio_event_t event)
#else
static void whd_bus_sdio_irq_handler(void *handler_arg, cyhal_sdio_irq_event_t event)
#endif
{
    whd_driver_t whd_driver = (whd_driver_t)handler_arg;

    /* WHD registered only for CY_CYHAL_SDIO_CARD_INTERRUPT */
    if (event != CYHAL_SDIO_CARD_INTERRUPT)
    {
        WPRINT_WHD_ERROR( ("Unexpected interrupt event %d\n", event) );
        WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, error_intrs);
        return;
    }

    WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, sdio_intrs);

    /* call thread notify to wake up WHD thread */
    whd_thread_notify_irq(whd_driver);
}

whd_result_t whd_bus_sdio_irq_register(whd_driver_t whd_driver)
{
#if (CYHAL_API_VERSION >= 2)
    cyhal_sdio_register_callback(whd_driver->bus_priv->sdio_obj, whd_bus_sdio_irq_handler, whd_driver);
#else
    cyhal_sdio_register_irq(whd_driver->bus_priv->sdio_obj, whd_bus_sdio_irq_handler, whd_driver);
#endif
    return WHD_SUCCESS;
}

whd_result_t whd_bus_sdio_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
#if (CYHAL_API_VERSION >= 2)
    cyhal_sdio_enable_event(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, CYHAL_ISR_PRIORITY_DEFAULT,
                            enable);
#else
    cyhal_sdio_irq_enable(whd_driver->bus_priv->sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, enable);
#endif
    return WHD_SUCCESS;
}

#if (CYHAL_API_VERSION >= 2)
static void whd_bus_sdio_oob_irq_handler(void *arg, cyhal_gpio_event_t event)
#else
static void whd_bus_sdio_oob_irq_handler(void *arg, cyhal_gpio_irq_event_t event)
#endif
{
    whd_driver_t whd_driver = (whd_driver_t)arg;
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;
#if (CYHAL_API_VERSION >= 2)
    const cyhal_gpio_event_t expected_event = (config->is_falling_edge == WHD_TRUE)
                                              ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;
#else
    const cyhal_gpio_irq_event_t expected_event = (config->is_falling_edge == WHD_TRUE)
                                                  ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;
#endif
    if (event != expected_event)
    {
        WPRINT_WHD_ERROR( ("Unexpected interrupt event %d\n", event) );
        WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, error_intrs);
        return;
    }

    WHD_BUS_STATS_INCREMENT_VARIABLE(whd_driver->bus_priv, oob_intrs);

    /* Call thread notify to wake up WHD thread */
    whd_thread_notify_irq(whd_driver);
}

static whd_result_t whd_bus_sdio_register_oob_intr(whd_driver_t whd_driver)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;

    CHECK_RETURN(cyhal_gpio_init(config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, config->drive_mode, config->init_drive_state));

#if (CYHAL_API_VERSION >= 2)
    static cyhal_gpio_callback_data_t cbdata;
    cbdata.callback = whd_bus_sdio_oob_irq_handler;
    cbdata.callback_arg = whd_driver;
    cyhal_gpio_register_callback(config->host_oob_pin, &cbdata);
#else
    cyhal_gpio_register_irq(config->host_oob_pin, config->intr_priority, whd_bus_sdio_oob_irq_handler,
                            whd_driver);
#endif
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_unregister_oob_intr(whd_driver_t whd_driver)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;

    cyhal_gpio_free(config->host_oob_pin);
#if (CYHAL_API_VERSION >= 2)
    cyhal_gpio_register_callback(config->host_oob_pin, NULL);
#else
    cyhal_gpio_register_irq(config->host_oob_pin, config->intr_priority, NULL, NULL);
#endif
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_enable_oob_intr(whd_driver_t whd_driver, whd_bool_t enable)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;
#if (CYHAL_API_VERSION >= 2)
    const cyhal_gpio_event_t event =
        (config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;

    cyhal_gpio_enable_event(config->host_oob_pin, event, config->intr_priority, (enable == WHD_TRUE) ? true : false);
#else
    const cyhal_gpio_irq_event_t event =
        (config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;

    cyhal_gpio_irq_enable(config->host_oob_pin, event, (enable == WHD_TRUE) ? true : false);
#endif
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_init_oob_intr(whd_driver_t whd_driver)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;
    uint8_t sepintpol;

    /* OOB isn't configured so bail */
    if (config->host_oob_pin == CYHAL_NC_PIN_VALUE)
        return WHD_SUCCESS;

    /* Choose out-of-band interrupt polarity */
    if (config->is_falling_edge == WHD_FALSE)
    {
        sepintpol = SEP_INTR_CTL_POL;
    }
    else
    {
        sepintpol = 0;
    }

    /* Set OOB interrupt to the correct WLAN GPIO pin (default to GPIO0) */
    if (config->dev_gpio_sel)
        CHECK_RETURN(whd_bus_sdio_set_oob_interrupt(whd_driver, config->dev_gpio_sel) );

    /* Enable out-of-band interrupt on the device */
    CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_SEP_INT_CTL, (uint8_t)1,
                                              SEP_INTR_CTL_MASK | SEP_INTR_CTL_EN | sepintpol) );

    /* Register and enable OOB */
    /* XXX Remove this when BSP377 is implemented */
    CHECK_RETURN(whd_bus_sdio_register_oob_intr(whd_driver) );
    CHECK_RETURN(whd_bus_sdio_enable_oob_intr(whd_driver, WHD_TRUE) );

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_deinit_oob_intr(whd_driver_t whd_driver)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->sdio_config.oob_config;

    if (config->host_oob_pin != CYHAL_NC_PIN_VALUE)
    {
        CHECK_RETURN(whd_bus_sdio_enable_oob_intr(whd_driver, WHD_FALSE) );
        CHECK_RETURN(whd_bus_sdio_unregister_oob_intr(whd_driver) );
    }

    return WHD_SUCCESS;
}

#ifdef BLHS_SUPPORT
static whd_result_t whd_bus_sdio_blhs_read_h2d(whd_driver_t whd_driver, uint32_t *val)
{
    return whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_REG_DAR_H2D_MSG_0, 1, (uint8_t *)val);
}

static whd_result_t whd_bus_sdio_blhs_write_h2d(whd_driver_t whd_driver, uint32_t val)
{
    return whd_bus_sdio_write_register_value(whd_driver, BACKPLANE_FUNCTION, (uint32_t)SDIO_REG_DAR_H2D_MSG_0,
                                             (uint8_t)1, val);
}

static whd_result_t whd_bus_sdio_blhs_wait_d2h(whd_driver_t whd_driver, uint16_t state)
{
    uint16_t byte_data = 0;
    uint32_t loop_count = 0;
    whd_result_t result;
#ifdef DM_43022C1
    uint8_t no_of_bytes = 2;
#else
    uint8_t no_of_bytes = 1;
#endif

    while ( ( (result =
                   whd_bus_sdio_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_REG_DAR_D2H_MSG_0, (uint8_t)no_of_bytes,
                                                    (uint8_t*)&byte_data) ) == WHD_SUCCESS ) &&
            ( (byte_data & state) == 0 ) &&
            (loop_count < SDIO_BLHS_D2H_TIMEOUT_MS) )
    {
        (void)cy_rtos_delay_milliseconds( (uint32_t)10 );
        loop_count += 10;
    }

    if (loop_count >= SDIO_BLHS_D2H_TIMEOUT_MS)
    {
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("Read D2H message failed\n") );
        }
        else
        {
            WPRINT_WHD_ERROR( ("Timeout while waiting for D2H_MSG(0x%x) expected 0x%x\n", byte_data, state) );
        }
        return WHD_TIMEOUT;
    }

    return WHD_SUCCESS;
}

static whd_result_t whd_bus_sdio_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage)
{
    uint32_t val = 0;
#ifdef DM_43022C1
    uint32_t loop = 0;
    uint32_t value = 0;
#endif

    /* Skip bootloader handshake if it is not need */

    switch (stage)
    {
       case CHK_BL_INIT:
            WPRINT_WHD_DEBUG(("CHK_BL_INIT \n"));
#ifdef DM_43022C1
            CHECK_RETURN(whd_bus_sdio_write_register_value(whd_driver, BACKPLANE_FUNCTION, (uint32_t)SDIO_REG_DAR_H2D_MSG_0,
                                               (uint8_t)4, SDIO_BLHS_H2D_BL_INIT));
#else
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_BL_INIT) );
#endif
            CHECK_RETURN(whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_READY) );
            break;
        case PREP_FW_DOWNLOAD:
            WPRINT_WHD_DEBUG(("PREP_FW_DOWNLOAD!! \n"));
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_DL_FW_START) );
            break;
        case POST_FW_DOWNLOAD:
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_DL_FW_DONE) );
            WPRINT_WHD_DEBUG(("POST_FW_DOWNLOAD \n"));

#ifdef DM_43022C1

            if (whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_BP_CLK_DIS_REQ) != WHD_SUCCESS)
            {
                whd_wifi_print_whd_log(whd_driver);
                whd_bus_sdio_blhs_read_h2d(whd_driver, &val);
                whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_BL_RESET_ON_ERROR) );
                return WHD_BLHS_VALIDATE_FAILED;
            }
            else
            {
                WPRINT_WHD_DEBUG(("Received SDIO_BLHS_D2H_BP_CLK_DIS_REQ from Dongle!! \n"));
                whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_BP_CLK_DIS_ACK);
                WPRINT_WHD_DEBUG(("SDIO_BLHS_H2D_BP_CLK_DIS_ACK \n"));
            }

            /* Check for any interrupt pending. Here Interrupt pending will be treated as HS bit
             * to know that the backplane is enabled. Interrupt pending register is choosen here
             * because the backplane will be disabled and this is the only tested register which
             * is accessible while backplane is disabled
             */
             for (loop = 0; loop < 80000; loop++) {
                CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_INTPEND, (uint8_t)4, (uint8_t*)&value) );
                if (value & SDIOD_CCCR_INTPEND_INT1) {
                    WPRINT_WHD_DEBUG(("%s: Backplane enabled.\n",  __func__));
                    break;
                }
            }

            /* Bootloader hung after backplane disable */
            if (loop == 80000) {
                WPRINT_WHD_ERROR(("%s: Device hung, return failure.\n", __func__));
                return WHD_BLHS_VALIDATE_FAILED;
            }
#else
            if (whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_TRXHDR_PARSE_DONE) != WHD_SUCCESS)
            {
                whd_wifi_print_whd_log(whd_driver);
                whd_bus_sdio_blhs_read_h2d(whd_driver, &val);
                whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_BL_RESET_ON_ERROR) );
                return WHD_BLHS_VALIDATE_FAILED;
            }
            else
            {
                WPRINT_WHD_DEBUG(("Received TRX parsing Succeed MSG from Dongle \n"));
            }
#endif /* DM_43022C1 */
            break;
        case CHK_FW_VALIDATION:
            WPRINT_WHD_DEBUG(("CHK_FW_VALIDATION \n"));
            if ( (whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_VALDN_DONE) != WHD_SUCCESS) ||
                 (whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_VALDN_RESULT) != WHD_SUCCESS) )
            {
                whd_wifi_print_whd_log(whd_driver);
                whd_bus_sdio_blhs_read_h2d(whd_driver, &val);
                whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_BL_RESET_ON_ERROR) );
                return WHD_BLHS_VALIDATE_FAILED;
            }
            break;
#ifdef DM_43022C1
        case PREP_NVRAM_DOWNLOAD:
            WPRINT_WHD_DEBUG(("PREP_NVRAM_DOWNLOAD \n"));
            CHECK_RETURN(whd_bus_sdio_blhs_read_h2d(whd_driver, &val) );
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_DL_NVRAM_START) ) );
            break;
#endif
        case POST_NVRAM_DOWNLOAD:
            WPRINT_WHD_DEBUG(("POST_NVRAM_DOWNLOAD \n"));
            CHECK_RETURN(whd_bus_sdio_blhs_read_h2d(whd_driver, &val) );
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_DL_NVRAM_DONE) ) );
#ifdef DM_43022C1
            /* For chip 43022(DM), NVRAM is downloaded at 512KB region and BL moves the NVRAM at the RAM end portion and gives ACK(SDIO_BLHS_D2H_NVRAM_DONE) */
            if (whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_NVRAM_MV_DONE) != WHD_SUCCESS)
            {
                whd_wifi_print_whd_log(whd_driver);
                whd_bus_sdio_blhs_read_h2d(whd_driver, &val);
                whd_bus_sdio_blhs_write_h2d(whd_driver, (val | SDIO_BLHS_H2D_BL_RESET_ON_ERROR) );
                return WHD_BLHS_VALIDATE_FAILED;
            }
#endif
            break;
        case POST_WATCHDOG_RESET:
            CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_BL_INIT) );
            CHECK_RETURN(whd_bus_sdio_blhs_wait_d2h(whd_driver, SDIO_BLHS_D2H_READY) );
        default:
            return WHD_BADARG;
    }

    return WHD_SUCCESS;
}

#endif

#ifdef WPRINT_ENABLE_WHD_DEBUG
#define WHD_BLOCK_SIZE       (1024)
static whd_result_t whd_bus_sdio_verify_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                 whd_bool_t direct_resource, uint32_t address, uint32_t image_size)
{
    whd_result_t result = WHD_SUCCESS;
    uint8_t *image;
    uint8_t *cmd_img = NULL;
    uint32_t blocks_count = 0;
    uint32_t i;
    uint32_t size_out;

    result = whd_get_resource_no_of_blocks(whd_driver, resource, &blocks_count);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Fatal error: download_resource blocks count not known, %s failed at line %d \n", __func__,
                           __LINE__) );
        goto exit;
    }
    cmd_img = whd_mem_malloc(WHD_BLOCK_SIZE);
    if (cmd_img != NULL)
    {
        for (i = 0; i < blocks_count; i++)
        {
            CHECK_RETURN(whd_get_resource_block(whd_driver, resource, i, (const uint8_t **)&image, &size_out) );
            result = whd_bus_transfer_backplane_bytes(whd_driver, BUS_READ, address, size_out, cmd_img);
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("%s: Failed to read firmware image\n", __FUNCTION__) );
                goto exit;
            }
            if (memcmp(cmd_img, &image[0], size_out) )
            {
                WPRINT_WHD_ERROR( ("%s: Downloaded image is corrupted, address is %d, len is %d, resource is %d \n",
                                   __FUNCTION__, (int)address, (int)size_out, (int)resource) );
            }
            address += size_out;
        }
    }
exit:
    if (cmd_img)
        whd_mem_free(cmd_img);
    return WHD_SUCCESS;
}

#endif

static whd_result_t whd_bus_sdio_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                   whd_bool_t direct_resource, uint32_t address, uint32_t image_size)
{

#ifdef DM_43022C1
#define TRX_HDR_START_ADDR 0x7fd4c /* TRX header start address */
#endif

    whd_result_t result = WHD_SUCCESS;
    uint8_t *image;
    uint32_t blocks_count = 0;
    uint32_t i;
    uint32_t size_out;
    uint32_t reset_instr = 0;
#ifdef WPRINT_ENABLE_WHD_DEBUG
    uint32_t pre_addr = address;
#endif

    result = whd_get_resource_no_of_blocks(whd_driver, resource, &blocks_count);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Fatal error: download_resource blocks count not known, %s failed at line %d \n", __func__,
                           __LINE__) );
        goto exit;
    }

    for (i = 0; i < blocks_count && image_size > 0; i++)
    {
        CHECK_RETURN(whd_get_resource_block(whd_driver, resource, i, (const uint8_t **)&image, &size_out) );
        if (resource == WHD_RESOURCE_WLAN_FIRMWARE)
        {
#ifdef BLHS_SUPPORT
            trx_header_t *trx;
            if (i == 0)
            {
#ifndef DM_43022C1
                trx = (trx_header_t *)&image[0];
#else
#ifndef WLAN_MFG_FIRMWARE
                trx = (trx_header_t *)&wifi_firmware_image_data;
#else
                trx = (trx_header_t *)&wifi_mfg_firmware_image_data;
#endif /* WLAN_MFG_FIRMWARE */
#endif /* DM_43022C1 */

                if (trx->magic == TRX_MAGIC)
                {

#ifdef DM_43022C1
                    /* For 43022DM, Reading the trx header and writing at 512KB area */
                    whd_bus_transfer_backplane_bytes(whd_driver, BUS_WRITE, TRX_HDR_START_ADDR, sizeof(*trx), (uint8_t*)trx);
#else
                    image_size = trx->len;
                    address -= sizeof(*trx);
#endif /* DM_43022C1 */

#ifdef WPRINT_ENABLE_WHD_DEBUG
                    pre_addr = address;
#endif
                }
                else
                {
                    result = WHD_BADARG;
                    WPRINT_WHD_ERROR( ("%s: TRX header mismatch\n", __FUNCTION__) );
                    goto exit;
                }
            }
#endif
            if (size_out > image_size)
            {
                size_out = image_size;
                image_size = 0;
            }
            else
            {
                image_size -= size_out;
            }
            if (reset_instr == 0)
            {
                /* Copy the starting address of the firmware into a global variable */
                reset_instr = *( (uint32_t *)(&image[0]) );
            }
        }
        result = whd_bus_transfer_backplane_bytes(whd_driver, BUS_WRITE, address, size_out, &image[0]);
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Failed to write firmware image\n", __FUNCTION__) );
            goto exit;
        }
        address += size_out;
    }
#ifdef WPRINT_ENABLE_WHD_DEBUG
    whd_bus_sdio_verify_resource(whd_driver, resource, direct_resource, pre_addr, image_size);
#endif
    /* Below part of the code is applicable to arm_CR4 type chips only
     * The CR4 chips by default firmware is not loaded at 0. So we need
     * load the first 32 bytes with the offset of the firmware load address
     * which is been copied before during the firmware download
     */
#ifndef BLHS_SUPPORT
    if ( (address != 0) && (reset_instr != 0) )
    {
        /* write address 0 with reset instruction */
        result = whd_bus_write_backplane_value(whd_driver, 0, sizeof(reset_instr), reset_instr);

        if (result == WHD_SUCCESS)
        {
            uint32_t tmp;

            /* verify reset instruction value */
            result = whd_bus_read_backplane_value(whd_driver, 0, sizeof(tmp), (uint8_t *)&tmp);

            if ( (result == WHD_SUCCESS) && (tmp != reset_instr) )
            {
                WPRINT_WHD_ERROR( ("%s: Failed to write 0x%08" PRIx32 " to addr 0\n", __FUNCTION__, reset_instr) );
                WPRINT_WHD_ERROR( ("%s: contents of addr 0 is 0x%08" PRIx32 "\n", __FUNCTION__, tmp) );
                return WHD_WLAN_SDIO_ERROR;
            }
        }
    }
#endif
exit: return result;
}

static whd_result_t whd_bus_sdio_write_wifi_nvram_image(whd_driver_t whd_driver)
{
    uint32_t img_base;
    uint32_t img_end;
    uint32_t image_size;

#if defined(BLHS_SUPPORT) && defined(DM_43022C1)
    CHECK_RETURN(whd_bus_common_blhs(whd_driver, PREP_NVRAM_DOWNLOAD) );
#endif /* defined(BLHS_SUPPORT) && defined(DM_43022C1) */

    /* Get the size of the variable image */
    CHECK_RETURN(whd_resource_size(whd_driver, WHD_RESOURCE_WLAN_NVRAM, &image_size) );

    /* Round up the size of the image */
    image_size = ROUND_UP(image_size, NVM_IMAGE_SIZE_ALIGNMENT);

    /* Write image */

#ifndef DM_43022C1
    img_end = GET_C_VAR(whd_driver, CHIP_RAM_SIZE) - 4;
#else
    /* 43022DM - NVRAM is downloaded at 512KB region,
    later will be moved to RAM END region by Bootloader */
    img_end = GET_C_VAR(whd_driver, NVRAM_DNLD_ADDR) - 4;
#endif

    img_base = (img_end - image_size);
    img_base += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    CHECK_RETURN(whd_bus_sdio_download_resource(whd_driver, WHD_RESOURCE_WLAN_NVRAM, WHD_FALSE, img_base, image_size) );

    /* Write the variable image size at the end */
    image_size = (~(image_size / 4) << 16) | (image_size / 4);

    img_end += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)img_end, 4, image_size) );

#ifdef BLHS_SUPPORT
    CHECK_RETURN(whd_bus_common_blhs(whd_driver, POST_NVRAM_DOWNLOAD) );
#endif /* BLHS_SUPPORT */

    return WHD_SUCCESS;
}

/*
 * Update the backplane window registers
 */
whd_result_t whd_bus_sdio_set_backplane_window(whd_driver_t whd_driver, uint32_t addr, uint32_t *curbase)
{
    whd_result_t result = WHD_BUS_WRITE_REGISTER_ERROR;
    uint32_t base = addr & ( (uint32_t) ~BACKPLANE_ADDRESS_MASK );
    const uint32_t upper_32bit_mask = 0xFF000000;
    const uint32_t upper_middle_32bit_mask = 0x00FF0000;
    const uint32_t lower_middle_32bit_mask = 0x0000FF00;

    if (base == *curbase)
    {
        return WHD_SUCCESS;
    }
    if ( (base & upper_32bit_mask) != (*curbase & upper_32bit_mask) )
    {
        if (WHD_SUCCESS !=
            (result = whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_BACKPLANE_ADDRESS_HIGH,
                                                   (uint8_t)1, (base >> 24) ) ) )
        {
            WPRINT_WHD_ERROR( ("Failed to write register value to the bus, %s failed at %d \n", __func__,
                               __LINE__) );
            return result;
        }
        /* clear old */
        *curbase &= ~upper_32bit_mask;
        /* set new */
        *curbase |= (base & upper_32bit_mask);
    }

    if ( (base & upper_middle_32bit_mask) !=
         (*curbase & upper_middle_32bit_mask) )
    {
        if (WHD_SUCCESS !=
            (result = whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_BACKPLANE_ADDRESS_MID,
                                                   (uint8_t)1, (base >> 16) ) ) )
        {
            WPRINT_WHD_ERROR( ("Failed to write register value to the bus, %s failed at %d \n", __func__,
                               __LINE__) );
            return result;
        }
        /* clear old */
        *curbase &= ~upper_middle_32bit_mask;
        /* set new */
        *curbase |= (base & upper_middle_32bit_mask);
    }

    if ( (base & lower_middle_32bit_mask) !=
         (*curbase & lower_middle_32bit_mask) )
    {
        if (WHD_SUCCESS !=
            (result = whd_bus_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_BACKPLANE_ADDRESS_LOW,
                                                   (uint8_t)1, (base >> 8) ) ) )
        {
            WPRINT_WHD_ERROR( ("Failed to write register value to the bus, %s failed at %d \n", __func__,
                               __LINE__) );
            return result;
        }

        /* clear old */
        *curbase &= ~lower_middle_32bit_mask;
        /* set new */
        *curbase |= (base & lower_middle_32bit_mask);
    }

    return WHD_SUCCESS;
}

whd_result_t whd_wlan_reset_sdio(whd_driver_t whd_driver)
{

    CHECK_DRIVER_NULL(whd_driver);

#ifdef BLHS_SUPPORT
    uint8_t byte_data = 0;
    uint8_t result = -1;
    /* Host to Dongle Registers are initialized to zero */
    CHECK_RETURN(whd_bus_sdio_blhs_write_h2d(whd_driver, SDIO_BLHS_H2D_BL_INIT) );

    if (whd_driver->chip_info.chipid_in_sdiocore == 1)
    {
        /* Option1 - Configure registers to trigger WLAN reset on "SDIO Soft Reset (Func0 RES bit)",
           and set RES bit to trigger SDIO as well as WLAN reset (instead of using PMU/CC Watchdog register) */
        CHECK_RETURN(whd_bus_read_register_value (whd_driver, BUS_FUNCTION, SDIOD_CCCR_BRCM_CARDCTL, (uint8_t)1,
                                                  &byte_data) );

        byte_data |= SDIOD_CCCR_BRCM_WLANRST_ONF0ABORT;

        CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_BRCM_CARDCTL, (uint8_t)1,
                                                  byte_data) );
        /* Option2 - To trigger only WLAN Reset the corresponding new bit in rev 31 */
        //CHECK_RETURN(whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SDIOD_CCCR_IOABORT, (uint8_t)1,
        //                                            IO_ABORT_RESET_ALL ));
        return WHD_SUCCESS;
    }
    else
    {
        /* using PMU Watchdog register reset */
        uint32_t watchdog_reset = 0xFFFF;

        result =
            whd_bus_write_backplane_value(whd_driver, (uint32_t)PMU_WATCHDOG(
                                              whd_driver), (uint8_t)sizeof(watchdog_reset),
                                          (uint8_t)watchdog_reset);
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("Error: pmuwatchdog reset failed, result=%d \n", result) );
            return WHD_FALSE;
        }
        else
        {
            WPRINT_WHD_INFO( ("pmuwatchdog reset done\n") );
            return WHD_SUCCESS;
        }
    }
#else
    WPRINT_WHD_INFO( ("Wifi Driver Deinit not implemented , %s failed at %d \n", __func__,
                      __LINE__) );
    return WHD_FALSE;
#endif
}

#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

