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
 *  Broadcom WLAN SPI Protocol interface
 *
 *  Implements the WHD Bus Protocol Interface for SPI
 *  Provides functions for initialising, de-intitialising 802.11 device,
 *  sending/receiving raw packets etc
 */

#include <string.h>  /* For memcpy */

#include "cybsp.h"
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)


#include "cy_result.h"
#include "cyabs_rtos.h"
#include "cyhal_gpio.h"

#include "whd_thread.h"
#include "whd_chip.h"
#include "whd_sdpcm.h"
#include "whd_chip_constants.h"
#include "whd_int.h"
#include "whd_bus_spi_protocol.h"
#include "whd_bus_common.h"
#include "whd_chip_reg.h"
#include "whd_debug.h"
#include "whd_bus.h"
#include "whd_spi.h"
#include "whd_sdio.h"
#include "whd_buffer_api.h"
#include "whd_debug.h"
#include "whd_types_int.h"
#include "whd_resource_if.h"
#include "whd_proto.h"
#include "whd_utils.h"

/******************************************************
*             Constants
******************************************************/

#define F2_READY_TIMEOUT_MS    (1000)
#define F2_READY_TIMEOUT_LOOPS (1000)
#define F1_READY_TIMEOUT_LOOPS (1000)
#define FEADBEAD_TIMEOUT_MS    (500)
#define ALP_AVAIL_TIMEOUT_MS   (100)

/* function 1 OCP space */
#define SBSDIO_SB_OFT_ADDR_MASK  0x07FFF /* sb offset addr is <= 15 bits, 32k */
#define SBSDIO_SB_OFT_ADDR_LIMIT  0x08000
#define SBSDIO_SB_ACCESS_2_4B_FLAG  0x08000 /* with b15, maps to 32-bit SB access */

#define HT_AVAIL_TIMEOUT_MS    (1000)

/* Taken from FALCON_5_90_195_26 dhd/sys/dhd_sdio.c. For 43362, MUST be >= 8 and word-aligned otherwise dongle fw crashes */
#define SPI_F2_WATERMARK       (32)

#define GSPI_PACKET_AVAILABLE  (1 << 8)
#define GSPI_UNDERFLOW         (1 << 1)

#define SWAP32_16BIT_PARTS(val) ( (uint32_t)( ( ( (uint32_t)(val) ) >> 16 ) + \
                                              ( ( ( (uint32_t)(val) ) & 0xffff ) << 16 ) ) )

#define WHD_BUS_GSPI_PACKET_OVERHEAD    (sizeof(whd_buffer_header_t) )

#define MAX_GSPI_TRANSFER_LEN  2048

#define H32TO16LE(x)           ( ( uint32_t )( ( ( ( uint32_t )(x) & ( uint32_t )0x000000ffU ) << 8 ) | \
                                               ( ( ( uint32_t )(x) & ( uint32_t )0x0000ff00U ) >> 8 ) | \
                                               ( ( ( uint32_t )(x) & ( uint32_t )0x00ff0000U ) << 8 ) | \
                                               ( ( ( uint32_t )(x) & ( uint32_t )0xff000000U ) >> 8 ) ) )

#define WHD_THREAD_POLL_TIMEOUT      (CY_RTOS_NEVER_TIMEOUT)

#define WHD_THREAD_POKE_TIMEOUT      (100)

#define HOSTINTMASK (I_HMB_SW_MASK)

#define BT_POLLING_TIME            (100)
#define ALIGNED_ADDRESS            ( (uint32_t)0x3 )
typedef enum
{
    GSPI_INCREMENT_ADDRESS = 1, GSPI_FIXED_ADDRESS = 0
} gspi_transfer_access_t;

/******************************************************
*             Structures
******************************************************/

#pragma pack(1)

typedef struct
{
    whd_bus_gspi_header_t header;
    uint8_t response_delay[4];
} gspi_backplane_f1_read_header_t;

#pragma pack()

typedef struct
{
    gspi_backplane_f1_read_header_t gspi_header;
    uint32_t data[1];
} gspi_backplane_f1_read_packet_t;

/******************************************************
*             Static variables
******************************************************/

static const uint8_t whd_bus_gspi_command_mapping[] = { 0, 1 };

/******************************************************
*             Structures
******************************************************/
struct whd_bus_priv
{
    whd_spi_config_t spi_config;
    cyhal_spi_t *spi_obj;
};

/******************************************************
*             Variables
******************************************************/

/******************************************************
*             Static Function Declarations
******************************************************/

static whd_result_t whd_spi_download_firmware(whd_driver_t whd_driver);
static whd_result_t whd_bus_spi_transfer_buffer(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                                whd_bus_function_t function, uint32_t address, whd_buffer_t buffer);
static whd_result_t whd_bus_spi_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage);
static whd_result_t whd_bus_spi_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address, uint32_t image_size);
static whd_result_t whd_bus_spi_write_wifi_nvram_image(whd_driver_t whd_driver);
static whd_result_t whd_bus_spi_set_backplane_window(whd_driver_t whd_driver, uint32_t addr, uint32_t *curbase);
/******************************************************
*             Global Function definitions
******************************************************/
uint32_t whd_bus_spi_bt_packet_available_to_read(whd_driver_t whd_driver);

whd_result_t whd_bus_spi_attach(whd_driver_t whd_driver, whd_spi_config_t *whd_spi_config, cyhal_spi_t *spi_obj)
{
    struct whd_bus_info *whd_bus_info;

    if (!whd_driver || !whd_spi_config)
    {
        WPRINT_WHD_ERROR( ("Invalid param in func %s at line %d \n",
                           __func__, __LINE__) );
        return WHD_WLAN_BADARG;
    }

    if (whd_spi_config->oob_config.host_oob_pin == CYHAL_NC_PIN_VALUE)
    {
        WPRINT_WHD_ERROR( ("OOB interrupt pin argument must be provided in %s\n", __FUNCTION__) );
        return WHD_BADARG;
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

    /* Pass the SPI object to bus private spi_obj pointer */
    whd_driver->bus_priv->spi_obj = spi_obj;
    whd_driver->bus_priv->spi_config = *whd_spi_config;

    whd_driver->proto_type = WHD_PROTO_BCDC;

    whd_bus_info->whd_bus_init_fptr = whd_bus_spi_init;
    whd_bus_info->whd_bus_deinit_fptr = whd_bus_spi_deinit;

    whd_bus_info->whd_bus_write_backplane_value_fptr = whd_bus_spi_write_backplane_value;
    whd_bus_info->whd_bus_read_backplane_value_fptr = whd_bus_spi_read_backplane_value;
    whd_bus_info->whd_bus_write_register_value_fptr = whd_bus_spi_write_register_value;
    whd_bus_info->whd_bus_read_register_value_fptr = whd_bus_spi_read_register_value;

    whd_bus_info->whd_bus_send_buffer_fptr = whd_bus_spi_send_buffer;
    whd_bus_info->whd_bus_transfer_bytes_fptr = whd_bus_spi_transfer_bytes;

    whd_bus_info->whd_bus_read_frame_fptr = whd_bus_spi_read_frame;

    whd_bus_info->whd_bus_packet_available_to_read_fptr = whd_bus_spi_packet_available_to_read;
    whd_bus_info->whd_bus_poke_wlan_fptr = whd_bus_spi_poke_wlan;
    whd_bus_info->whd_bus_wait_for_wlan_event_fptr = whd_bus_spi_wait_for_wlan_event;

    whd_bus_info->whd_bus_ack_interrupt_fptr = whd_bus_spi_ack_interrupt;
    whd_bus_info->whd_bus_wake_interrupt_present_fptr = whd_bus_spi_wake_interrupt_present;

    whd_bus_info->whd_bus_wakeup_fptr = whd_bus_spi_wakeup;
    whd_bus_info->whd_bus_sleep_fptr = whd_bus_spi_sleep;

    whd_bus_info->whd_bus_backplane_read_padd_size_fptr = whd_bus_spi_backplane_read_padd_size;
    whd_bus_info->whd_bus_use_status_report_scheme_fptr = whd_bus_spi_use_status_report_scheme;

    whd_bus_info->whd_bus_get_max_transfer_size_fptr = whd_bus_spi_get_max_transfer_size;

    whd_bus_info->whd_bus_init_stats_fptr = whd_bus_spi_init_stats;
    whd_bus_info->whd_bus_print_stats_fptr = whd_bus_spi_print_stats;
    whd_bus_info->whd_bus_reinit_stats_fptr = whd_bus_spi_reinit_stats;
    whd_bus_info->whd_bus_irq_register_fptr = whd_bus_spi_irq_register;
    whd_bus_info->whd_bus_irq_enable_fptr = whd_bus_spi_irq_enable;
    whd_bus_info->whd_bus_blhs_fptr = whd_bus_spi_blhs;
    whd_bus_info->whd_bus_download_resource_fptr = whd_bus_spi_download_resource;
    whd_bus_info->whd_bus_set_backplane_window_fptr = whd_bus_spi_set_backplane_window;

    return WHD_SUCCESS;
}

void whd_bus_spi_detach(whd_driver_t whd_driver)
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

whd_result_t whd_bus_spi_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    whd_result_t result = whd_bus_spi_transfer_buffer(whd_driver, BUS_WRITE, WLAN_FUNCTION, 0, buffer);
    CHECK_RETURN(whd_buffer_release(whd_driver, buffer, WHD_NETWORK_TX) );
    if (result == WHD_SUCCESS)
    {
        DELAYED_BUS_RELEASE_SCHEDULE (whd_driver, WHD_TRUE);
    }
    CHECK_RETURN(result);

    return WHD_SUCCESS;
}

/*
 * Perform a transfer on the gSPI bus
 * Prerequisites: length < MAX_GSPI_TRANSFER_LEN
 */
static whd_result_t whd_bus_spi_transfer_buffer(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                                whd_bus_function_t function, uint32_t address, whd_buffer_t buffer)
{
    uint32_t *temp, addr;
    whd_result_t result;
    uint16_t newsize;
    whd_buffer_header_t *header = (whd_buffer_header_t *)whd_buffer_get_current_piece_data_pointer(whd_driver, buffer);
    whd_buffer_header_t *aligned_header = (whd_buffer_header_t *)whd_driver->aligned_addr;
    whd_bus_gspi_header_t *gspi_header;
    size_t transfer_size;

    uint16_t size = ( uint16_t )(whd_buffer_get_current_piece_size(whd_driver, buffer) - sizeof(whd_buffer_header_t) );
    CHECK_PACKET_NULL(header, WHD_NO_REGISTER_FUNCTION_POINTER);
    /* Round size up to 32-bit alignment */
    newsize = (uint16_t)ROUND_UP(size, 4);

    /* Send the data */
    if (direction == BUS_WRITE)
    {
        /* Wait for FIFO to be ready to accept data */
        if (function == WLAN_FUNCTION)
        {
            uint32_t whd_bus_gspi_status;
            uint32_t loop_count = 0;
            while ( ( (result =
                           whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_STATUS_REGISTER, (uint8_t)4,
                                                           (uint8_t *)&whd_bus_gspi_status) ) == WHD_SUCCESS ) &&
                    ( (whd_bus_gspi_status & (1 << 5) ) == 0 ) &&
                    (loop_count < ( uint32_t )F2_READY_TIMEOUT_LOOPS) )
            {
                loop_count++;
            }
            if (result != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("Error reading register value, %s failed at %d \n", __func__, __LINE__) );
                return result;
            }
            if (loop_count >= ( uint32_t )F2_READY_TIMEOUT_LOOPS)
            {
                WPRINT_WHD_ERROR( ("Timeout waiting for data FIFO to be ready\n") );
                return WHD_TIMEOUT;
            }
        }

    }

    transfer_size = (size_t)(newsize + sizeof(whd_bus_gspi_header_t) );
    gspi_header =
        (whd_bus_gspi_header_t *)( (char *)header->bus_header + MAX_BUS_HEADER_SIZE -
                                   sizeof(whd_bus_gspi_header_t) );
    /* Form the gSPI header */
    addr = (uint32_t )header;
    /* check 4byte alignment */
    if ( (addr & ALIGNED_ADDRESS) )
    {
        if (aligned_header)
        {
            /* use memcpy to get aligned event message */
            memcpy(aligned_header, header, sizeof(*aligned_header) + size);
            gspi_header =
                (whd_bus_gspi_header_t *)( (char *)aligned_header->bus_header + MAX_BUS_HEADER_SIZE -
                                           sizeof(whd_bus_gspi_header_t) );
        }
    }
    /* Form the gSPI header */
    *gspi_header =
        ( whd_bus_gspi_header_t )( ( uint32_t )( (whd_bus_gspi_command_mapping[(int)direction] & 0x1) << 31 ) |
                                   ( uint32_t )( (GSPI_INCREMENT_ADDRESS & 0x1) << 30 ) |
                                   ( uint32_t )( (function & 0x3) << 28 ) |
                                   ( uint32_t )( (address & 0x1FFFF) << 11 ) | ( uint32_t )( (size & 0x7FF) << 0 ) );

    /* Reshuffle the bits if we're not in 32 bit mode */
    if (whd_driver->bus_gspi_32bit == WHD_FALSE)
    {
        /* Note: This typecast should always be valid if the buffer containing the GSpi packet has been correctly declared as 32-bit aligned */
        temp = (uint32_t *)gspi_header;
        *temp = H32TO16LE(*temp);
    }

    /* Send the data */
    if (direction == BUS_READ)
    {
        result =  cyhal_spi_transfer(whd_driver->bus_priv->spi_obj, NULL,
                                     sizeof(whd_bus_gspi_header_t),
                                     (uint8_t *)gspi_header,
                                     transfer_size, 0);

    }
    else
    {
        result = cyhal_spi_transfer(whd_driver->bus_priv->spi_obj, (uint8_t *)gspi_header, transfer_size, NULL,
                                    0, 0);
    }

    return result;
}

whd_result_t whd_bus_spi_poke_wlan(whd_driver_t whd_driver)
{
    return WHD_SUCCESS;
}

whd_result_t whd_bus_spi_ack_interrupt(whd_driver_t whd_driver, uint32_t intstatus)
{
    return whd_bus_write_register_value(whd_driver, BUS_FUNCTION, SPI_INTERRUPT_REGISTER, (uint8_t)2, intstatus);
}

uint32_t whd_bus_spi_bt_packet_available_to_read(whd_driver_t whd_driver)
{
    whd_bt_dev_t btdev = whd_driver->bt_dev;
    uint32_t int_status = 0;

    if (btdev && btdev->bt_int_cb)
    {
        if (whd_bus_spi_read_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                             (uint8_t *)&int_status) != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Error reading interrupt status\n", __FUNCTION__) );
            int_status = 0;
        }

        if ( (I_HMB_FC_CHANGE & int_status) != 0 )
        {

            if (whd_bus_spi_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                                  int_status & I_HMB_FC_CHANGE)  != WHD_SUCCESS)
            {
                WPRINT_WHD_ERROR( ("%s: Error write interrupt status\n", __FUNCTION__) );
                int_status = 0;
            }
            btdev->bt_int_cb(btdev->bt_data);
        }
    }

    return 0;
}

uint32_t whd_bus_spi_packet_available_to_read(whd_driver_t whd_driver)
{
    uint16_t interrupt_register;
    uint32_t int_status = 0;

    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );

    whd_bus_spi_bt_packet_available_to_read(whd_driver);
    /* Read the interrupt register */
    if (whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_INTERRUPT_REGISTER, (uint8_t)2,
                                        (uint8_t *)&interrupt_register) != WHD_SUCCESS)
    {
        goto return_with_error;
    }

    if ( (interrupt_register & 0x0086) != 0 )   /* This should be 0x87, but occasional "data not available" errors are flagged seemingly for no reason */
    {
        /* Error condition detected */
        WPRINT_WHD_DEBUG( ("Bus error condition detected\n") );
    }
    /* Read the IntStatus */
    if (whd_bus_spi_read_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                         (uint8_t *)&int_status) != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("%s: Error reading interrupt status\n", __FUNCTION__) );
        int_status = 0;
        goto return_with_error;
    }

    if ( (HOSTINTMASK & int_status) != 0 )
    {
        if (whd_bus_spi_write_backplane_value(whd_driver, (uint32_t)SDIO_INT_STATUS(whd_driver), (uint8_t)4,
                                              int_status & HOSTINTMASK)  != WHD_SUCCESS)
        {
            int_status = 0;
            goto return_with_error;
        }

    }
    /* Clear interrupt register */
    if (interrupt_register != 0)
    {
        if (whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_INTERRUPT_REGISTER, (uint8_t)2,
                                             interrupt_register) != WHD_SUCCESS)
        {
            goto return_with_error;
        }
    }

    return ( uint32_t )( (interrupt_register) & (F2_PACKET_AVAILABLE) );

return_with_error: whd_assert("Error accessing backplane", 0 != 0);
    return 0;
}

whd_result_t whd_bus_spi_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer)
{
    uint32_t whd_bus_gspi_status;
    whd_result_t result;
    uint32_t whd_gspi_bytes_pending;

    /* Ensure the wlan backplane bus is up */
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );

    do
    {
        result = whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_STATUS_REGISTER, (uint8_t)4,
                                                 (uint8_t *)&whd_bus_gspi_status);
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("Error reading register value, %s failed at %d \n", __func__, __LINE__) );
            return result;
        }
    } while (whd_bus_gspi_status == 0xFFFFFFFF);

    if ( (whd_bus_gspi_status & GSPI_PACKET_AVAILABLE) != 0 )
    {
        if ( ( ( (whd_bus_gspi_status >> 9) & 0x7FF ) == 0 ) ||
             ( ( (whd_bus_gspi_status >> 9) & 0x7FF ) > (WHD_LINK_MTU - WHD_BUS_GSPI_PACKET_OVERHEAD) ) ||
             (whd_bus_gspi_status & GSPI_UNDERFLOW) )
        {
            CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, SPI_FRAME_CONTROL, 1,
                                                          (1 << 0) ) );
            return WHD_NO_PACKET_TO_RECEIVE;
        }
    }

    whd_gspi_bytes_pending = 0;

    if ( (whd_bus_gspi_status & GSPI_PACKET_AVAILABLE) != 0 )
    {
        whd_gspi_bytes_pending = ( (whd_bus_gspi_status >> 9) & 0x7FF );
    }

    if (whd_gspi_bytes_pending == 0)
    {
        return WHD_NO_PACKET_TO_RECEIVE;
    }

    /* Allocate a suitable buffer */
    result = whd_host_buffer_get(whd_driver, buffer, WHD_NETWORK_RX,
                                 (uint16_t)(whd_gspi_bytes_pending + WHD_BUS_GSPI_PACKET_OVERHEAD),
                                 (whd_sdpcm_has_tx_packet(whd_driver) ? 0 : WHD_RX_BUF_TIMEOUT) );

    if (result != WHD_SUCCESS)
    {
        /* Read out the first 12 bytes to get the bus credit information */
        uint8_t temp_buffer[12 + MAX_BUS_HEADER_SIZE];
        CHECK_RETURN(whd_bus_spi_transfer_bytes(whd_driver, BUS_READ, WLAN_FUNCTION, 0, 12,
                                                (whd_transfer_bytes_packet_t *)temp_buffer) );

        /* Abort the transfer to force the packet to be dropped */
        if (whd_gspi_bytes_pending > 12)
        {
            CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, SPI_FRAME_CONTROL, 1,
                                                          (1 << 0) ) );
        }

        /* Process bus data credit information */
        whd_sdpcm_update_credit(whd_driver, (uint8_t *)(temp_buffer + sizeof(whd_bus_header_t) ) );
        WPRINT_WHD_ERROR( ("Packet buffer allocation failed in %s at %d \n", __func__, __LINE__) );
        return result;
    }

    result = whd_bus_spi_transfer_buffer(whd_driver, BUS_READ, WLAN_FUNCTION, 0, *buffer);
    if (result != WHD_SUCCESS)
    {
        CHECK_RETURN(whd_buffer_release(whd_driver, *buffer, WHD_NETWORK_RX) );
        WPRINT_WHD_ERROR( ("SPI buffer transfer failed in %s at %d \n", __func__, __LINE__) );
        return result;
    }

    DELAYED_BUS_RELEASE_SCHEDULE (whd_driver, WHD_TRUE);
    return WHD_SUCCESS;
}

whd_result_t whd_bus_spi_init(whd_driver_t whd_driver)
{
    uint32_t data = 0;
    uint32_t whd_bus_gspi_status;
    uint16_t data16 = 0;
    uint32_t loop_count;
    whd_result_t result;
    uint8_t init_data[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32_t interrupt_polarity = 0;
    uint16_t chip_id;
    size_t transfer_size = 12;
    uint8_t *aligned_addr = NULL;
    whd_oob_config_t *config = &whd_driver->bus_priv->spi_config.oob_config;

    whd_driver->bus_gspi_32bit = WHD_FALSE;

    if (config->is_falling_edge == WHD_FALSE)
        interrupt_polarity = INTERRUPT_POLARITY_HIGH;

    whd_bus_init_backplane_window(whd_driver);

    whd_bus_gspi_header_t *gspi_header = (whd_bus_gspi_header_t *)init_data;

    /* Due to an chip issue, the first transfer will be corrupted.
     * This means a repeated safe read of a known value register is required until
     * the correct value is returned - signalling the bus is running.
     * This known value register must be accessed using fixed (non-incrementing) address
     * mode, hence a custom packet header must be constructed
     * Due to the chip issue, the data received could be left shifted by one bit.
     */
    loop_count = 0;
    do
    {
        /* Header needs to calculated every time as init_data gets modified in cyhal_spi_transfer() */
        *gspi_header =
            ( whd_bus_gspi_header_t )SWAP32_16BIT_PARTS(SWAP32( (uint32_t)( (whd_bus_gspi_command_mapping[(int)BUS_READ]
                                                                             & 0x1) << 31 ) |
                                                                (uint32_t)( (GSPI_FIXED_ADDRESS & 0x1) << 30 ) |
                                                                (uint32_t)( (BUS_FUNCTION & 0x3) << 28 ) |
                                                                (uint32_t)( (SPI_READ_TEST_REGISTER & 0x1FFFFu) <<
                                                                            11 ) |
                                                                (uint32_t)( (4u /*size*/ & 0x7FFu) << 0 ) ) );
        CHECK_RETURN(cyhal_spi_transfer(whd_driver->bus_priv->spi_obj, NULL, sizeof(whd_bus_gspi_header_t),
                                        init_data, transfer_size, 0) );
        loop_count++;
    } while ( (NULL == memchr(&init_data[4], SPI_READ_TEST_REG_LSB, (size_t)8) ) &&
              (NULL == memchr(&init_data[4], SPI_READ_TEST_REG_LSB_SFT1, (size_t)8) ) &&
              (NULL == memchr(&init_data[4], SPI_READ_TEST_REG_LSB_SFT2, (size_t)8) ) &&
              (NULL == memchr(&init_data[4], SPI_READ_TEST_REG_LSB_SFT3, (size_t)8) ) &&
              (loop_count < ( uint32_t )FEADBEAD_TIMEOUT_MS) &&
              (cy_rtos_delay_milliseconds( (uint32_t)1 ), (1 == 1) ) );

    /* Register interrupt handler */
    whd_bus_spi_irq_register(whd_driver);
    /* Enable SPI IRQ */
    whd_bus_spi_irq_enable(whd_driver, WHD_TRUE);

    /* Keep/reset defaults for registers 0x0-0x4 except for, 0x0: Change word length to 32bit,
     * set endianness, enable wakeup. 0x2: enable interrupt with status. */
#if defined(IL_BIGENDIAN)
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, (uint8_t)4,
                                                  (uint32_t)(WORD_LENGTH_32 | (0 & ENDIAN_BIG) |
                                                             (interrupt_polarity & INTERRUPT_POLARITY_HIGH) | WAKE_UP |
                                                             (0x4 << (8 * SPI_RESPONSE_DELAY) ) |
                                                             ( (0 & STATUS_ENABLE) << (8 * SPI_STATUS_ENABLE) ) |
                                                             (INTR_WITH_STATUS << (8 * SPI_STATUS_ENABLE) ) ) ) );
#else
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, (uint8_t)4,
                                                  ( uint32_t )(WORD_LENGTH_32 | ENDIAN_BIG |
                                                               (interrupt_polarity & INTERRUPT_POLARITY_HIGH) |
                                                               WAKE_UP | (0x4 << (8 * SPI_RESPONSE_DELAY) ) |
                                                               ( (0 & STATUS_ENABLE) << (8 * SPI_STATUS_ENABLE) ) |
                                                               (INTR_WITH_STATUS << (8 * SPI_STATUS_ENABLE) ) ) ) );
#endif
    whd_driver->bus_gspi_32bit = WHD_TRUE;
    CHECK_RETURN(whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, (uint8_t)4,
                                                 (uint8_t *)&data) );

    if (whd_driver->bus_priv->spi_config.is_spi_normal_mode)
    {
        /* Reset host SPI interface to re-sync */
        /*host_platform_bus_init( );*/
    }

    /* Check feedbead can be read - i.e. the device is alive */
    data = 0;
    CHECK_RETURN(whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_READ_TEST_REGISTER, (uint8_t)4,
                                                 (uint8_t *)&data) );

    if (data != SPI_READ_TEST_REGISTER_VALUE)
    {
        WPRINT_WHD_ERROR( ("Read %x, instead of 0xFEEDBEAD from the WLAN chip\n", (unsigned int)data) );
        return WHD_SPI_ID_READ_FAIL;
    }

    /* Make sure error interrupt bits are clear */
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_INTERRUPT_REGISTER, (uint8_t)1,
                                                  ( uint32_t )(DATA_UNAVAILABLE | COMMAND_ERROR | DATA_ERROR |
                                                               F1_OVERFLOW) ) );

    /* Enable a selection of interrupts */
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_INTERRUPT_ENABLE_REGISTER, (uint8_t)2,
                                                  ( uint32_t )(F2_F3_FIFO_RD_UNDERFLOW | F2_F3_FIFO_WR_OVERFLOW |
                                                               COMMAND_ERROR | DATA_ERROR | F2_PACKET_AVAILABLE |
                                                               F1_OVERFLOW) ) );

    /* Request ALP */
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                  SBSDIO_ALP_AVAIL_REQ) );

    /* Wait until ALP is available */
    loop_count = 0;
    while ( ( (result = whd_bus_spi_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)2,
                                                        (uint8_t *)&data16) ) == WHD_SUCCESS ) &&
            ( (data16 & SBSDIO_ALP_AVAIL) == 0 ) &&
            (loop_count < ( uint32_t )ALP_AVAIL_TIMEOUT_MS) )
    {
        cy_rtos_delay_milliseconds( (uint32_t)1 );
        loop_count++;
    }
    if (loop_count >= ( uint32_t )ALP_AVAIL_TIMEOUT_MS)
    {
        WPRINT_WHD_ERROR( ("ALP Clock timeout\n") );
        return WHD_TIMEOUT;
    }
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Can't read SDIO_CHIP_CLOCK_CSR\n") );
        return result;
    }

    /* Clear request for ALP */
    CHECK_RETURN(whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                  (uint32_t)0) );

    /* Read the chip id */
    CHECK_RETURN(whd_bus_spi_read_backplane_value(whd_driver, CHIPCOMMON_BASE_ADDRESS, 2, (uint8_t *)&chip_id) );
    whd_chip_set_chip_id(whd_driver, chip_id);

    /* Download the firmware */
    result = whd_spi_download_firmware(whd_driver);

    /* user abort */
    if (result == WHD_UNFINISHED)
    {
        /* host_platform_reset_wifi (WHD_TRUE); */
        /* host_platform_power_wifi (WHD_FALSE); */
        WPRINT_WHD_ERROR( ("User aborted download of firmware\n") );
        return result;
    }

    /* non user abort error */
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Could not download firmware\n") );
        return result;
    }
    /* else, successfully downloaded the firmware; continue with waiting for WIFi to live */

    /* Wait for F2 to be ready */
    loop_count = 0;
    while ( ( (result = whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_STATUS_REGISTER, (uint8_t)4,
                                                        (uint8_t *)&whd_bus_gspi_status) ) == WHD_SUCCESS ) &&
            ( (whd_bus_gspi_status & (1 << 5) ) == 0 ) &&
            (loop_count < ( uint32_t )F2_READY_TIMEOUT_MS) )
    {
        cy_rtos_delay_milliseconds( (uint32_t)1 );
        loop_count++;
    }
    if (loop_count >= ( uint32_t )F2_READY_TIMEOUT_MS)
    {
        /* If your system fails here, it could be due to incorrect NVRAM variables.
         * Check which 'wifi_nvram_image.h' file your platform is using, and
         * check that it matches the WLAN device on your platform, including the
         * crystal frequency.
         */
        WPRINT_WHD_ERROR( ("Timeout while waiting for function 2 to be ready\n") );
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

    return result;
}

whd_result_t whd_bus_spi_deinit(whd_driver_t whd_driver)
{
    CHECK_RETURN(whd_allow_wlan_bus_to_sleep(whd_driver) );

    /* put device in reset. */
    //host_platform_reset_wifi (WHD_TRUE);
    if (whd_driver->aligned_addr)
    {
        whd_mem_free(whd_driver->aligned_addr);
        whd_driver->aligned_addr = NULL;
    }
    whd_bus_set_resource_download_halt(whd_driver, WHD_FALSE);
    DELAYED_BUS_RELEASE_SCHEDULE (whd_driver, WHD_FALSE);
    return WHD_SUCCESS;
}

whd_bool_t whd_bus_spi_wake_interrupt_present(whd_driver_t whd_driver)
{
    /* functionality is only currently needed and present on SDIO */
    return WHD_FALSE;
}

whd_result_t whd_bus_spi_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore)
{
    whd_result_t result = WHD_SUCCESS;
    uint32_t timeout_ms = 1;
    whd_bt_dev_t btdev = whd_driver->bt_dev;
    uint32_t delayed_release_timeout_ms;

    REFERENCE_DEBUG_ONLY_VARIABLE(result);

    delayed_release_timeout_ms = whd_bus_handle_delayed_release(whd_driver);
    if (delayed_release_timeout_ms != 0)
    {
        timeout_ms = delayed_release_timeout_ms;
    }
    else if ( (btdev && !btdev->intr) )
    {
        timeout_ms = BT_POLLING_TIME;
        whd_driver->thread_info.bus_interrupt = WHD_TRUE;
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
    if (whd_sdpcm_get_available_credits(whd_driver) == 0)
    {
        /* Keep poking the WLAN until it gives us more credits */
        result = whd_bus_spi_poke_wlan(whd_driver);
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

/******************************************************
*     Function definitions for Protocol Common
******************************************************/

/*
 * Write a value to a register NOT on the backplane
 * Prerequisites: value_length <= 4
 */
whd_result_t whd_bus_spi_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                              uint8_t value_length, uint32_t value)
{
    char gspi_internal_buffer[MAX_BUS_HEADER_SIZE + sizeof(uint32_t) + sizeof(uint32_t)] = {0};
    whd_transfer_bytes_packet_t *internal_gspi_packet = (whd_transfer_bytes_packet_t *)gspi_internal_buffer;

    /* Flip the bytes if we're not in 32 bit mode */
    if (whd_driver->bus_gspi_32bit == WHD_FALSE)
    {
        value = H32TO16LE(value);
    }
    /* Write the value and value_length into the packet */
    internal_gspi_packet->data[0] = value;

    /* Send it off */
    return whd_bus_spi_transfer_bytes(whd_driver, BUS_WRITE, function, address, value_length, internal_gspi_packet);
}

/*
 * Read the value of a register NOT on the backplane
 * Prerequisites: value_length <= 4
 */
whd_result_t whd_bus_spi_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                             uint8_t value_length, uint8_t *value)
{
    uint32_t *data_ptr;
    whd_result_t result;
    uint8_t padding = 0;

    char gspi_internal_buffer[MAX_BUS_HEADER_SIZE + sizeof(uint32_t) + sizeof(uint32_t)] = {0};

    /* Clear the receiving part of memory and set the value_length */
    if (function == BACKPLANE_FUNCTION)
    {
        gspi_backplane_f1_read_packet_t *pkt =
            (gspi_backplane_f1_read_packet_t *)(gspi_internal_buffer + MAX_BUS_HEADER_SIZE -
                                                sizeof(whd_bus_gspi_header_t) );
        data_ptr = pkt->data;
        padding = 4;   /* Add response delay */
    }
    else
    {
        whd_transfer_bytes_packet_t *pkt = (whd_transfer_bytes_packet_t *)gspi_internal_buffer;
        DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
        data_ptr = pkt->data;
        ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
    }

    *data_ptr = 0;
    result =
        whd_bus_spi_transfer_bytes(whd_driver, BUS_READ, function, address, ( uint16_t )(value_length + padding),
                                   (whd_transfer_bytes_packet_t *)gspi_internal_buffer);

    memcpy(value, data_ptr, value_length);

    return result;
}

/*
 * Write a value to a register on the backplane
 * Prerequisites: value_length <= 4
 */
whd_result_t whd_bus_spi_write_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                               uint32_t value)
{
    CHECK_RETURN(whd_bus_set_backplane_window(whd_driver, address) );

    address &= SBSDIO_SB_OFT_ADDR_MASK;

    if (register_length == 4)
        address |= SBSDIO_SB_ACCESS_2_4B_FLAG;

    return whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, address, register_length, value);
}

/*
 * Read the value of a register on the backplane
 * Prerequisites: value_length <= 4
 */
whd_result_t whd_bus_spi_read_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                              uint8_t *value)
{
    *value = 0;
    CHECK_RETURN(whd_bus_set_backplane_window(whd_driver, address) );

    address &= SBSDIO_SB_OFT_ADDR_MASK;

    if (register_length == 4)
        address |= SBSDIO_SB_ACCESS_2_4B_FLAG;

    return whd_bus_spi_read_register_value(whd_driver, BACKPLANE_FUNCTION, address, register_length, value);
}

whd_result_t whd_bus_spi_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                        whd_bus_function_t function, uint32_t address, uint16_t size,
                                        whd_transfer_bytes_packet_t *packet)
{
    uint32_t *temp;
    whd_result_t result;
    uint16_t newsize;
    DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
    whd_bus_gspi_header_t *gspi_header =
        (whd_bus_gspi_header_t *)( (char *)packet->data - sizeof(whd_bus_gspi_header_t) );
    ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
    size_t transfer_size;

    *gspi_header =
        ( whd_bus_gspi_header_t )( ( uint32_t )( (whd_bus_gspi_command_mapping[(int)direction] & 0x1) << 31 ) |
                                   ( uint32_t )( (GSPI_INCREMENT_ADDRESS & 0x1) << 30 ) |
                                   ( uint32_t )( (function & 0x3) << 28 ) |
                                   ( uint32_t )( (address & 0x1FFFF) << 11 ) | ( uint32_t )( (size & 0x7FF) << 0 ) );

    /* Reshuffle the bits if we're not in 32 bit mode */
    if (whd_driver->bus_gspi_32bit == WHD_FALSE)
    {
        /* Note: This typecast should always be valid if the buffer
         * containing the GSpi packet has been correctly declared as 32-bit aligned */
        temp = (uint32_t *)gspi_header;
        *temp = H32TO16LE(*temp);
    }

    /* Round size up to 32-bit alignment */
    newsize = (uint16_t)ROUND_UP(size, 4);

    /* Ensure we are clear to write */
    if ( (direction == BUS_WRITE) && (function == WLAN_FUNCTION) )
    {
        uint32_t whd_bus_gspi_status;
        uint32_t loop_count = 0;

        /* Verify the SDPCM size and stated size match */
        DISABLE_COMPILER_WARNING(diag_suppress = Pa039)
        uint16_t *frametag_ptr = (uint16_t *)&packet->data;
        ENABLE_COMPILER_WARNING(diag_suppress = Pa039)
        if (size != *frametag_ptr)
        {
            WPRINT_WHD_DEBUG( ("Error - gSPI size does not match SDPCM size!\n") );
            return WHD_SPI_SIZE_MISMATCH;
        }

        /* Wait for WLAN FIFO to be ready to accept data */
        while ( ( (result =
                       whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_STATUS_REGISTER, (uint8_t)4,
                                                       (uint8_t *)&whd_bus_gspi_status) ) == WHD_SUCCESS ) &&
                ( (whd_bus_gspi_status & (1 << 5) ) == 0 ) &&
                (loop_count < ( uint32_t )F2_READY_TIMEOUT_LOOPS) )
        {
            ++loop_count;
        }

        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("Failed to read SPI register value in %s at %d \n", __func__, __LINE__) );
            return result;
        }

        if (loop_count >= ( uint32_t )F2_READY_TIMEOUT_LOOPS)
        {
            WPRINT_WHD_DEBUG( ("Timeout waiting for data FIFO to be ready\n") );
            return WHD_TIMEOUT;
        }

    }

    transfer_size = (size_t)(newsize + sizeof(whd_bus_gspi_header_t) );

    /* Send the data */
    if (direction == BUS_READ)
    {
        result = cyhal_spi_transfer(whd_driver->bus_priv->spi_obj, NULL,
                                    sizeof(whd_bus_gspi_header_t),
                                    (uint8_t *)gspi_header,
                                    transfer_size, 0);
    }
    else
    {
        result = cyhal_spi_transfer(whd_driver->bus_priv->spi_obj, (uint8_t *)gspi_header, transfer_size, NULL,
                                    0, 0);
    }

    CHECK_RETURN(result);
    return WHD_SUCCESS;
}

/******************************************************
*             Static  Function definitions
******************************************************/

static whd_result_t whd_spi_download_firmware(whd_driver_t whd_driver)
{
    uint8_t csr_val;
    whd_result_t result;
    uint32_t loop_count = 0;

    CHECK_RETURN(whd_disable_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_NONE) );
    CHECK_RETURN(whd_disable_device_core(whd_driver, SOCRAM_CORE, WLAN_CORE_FLAG_NONE) );
    CHECK_RETURN(whd_reset_device_core(whd_driver, SOCRAM_CORE, WLAN_CORE_FLAG_NONE) );

    CHECK_RETURN(whd_chip_specific_socsram_init(whd_driver) );

    CHECK_RETURN(whd_bus_write_wifi_firmware_image(whd_driver) );
    CHECK_RETURN(whd_bus_spi_write_wifi_nvram_image(whd_driver) );

    /* Take the ARM core out of reset */
    CHECK_RETURN(whd_reset_device_core(whd_driver, WLAN_ARM_CORE, WLAN_CORE_FLAG_NONE) );
    result = whd_device_core_is_up(whd_driver, WLAN_ARM_CORE);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Could not bring ARM core up\n") );
        return result;
    }

    /* Wait until the HT clock is available */
    while ( ( (result =
                   whd_bus_spi_read_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_CHIP_CLOCK_CSR, (uint8_t)1,
                                                   &csr_val) ) == WHD_SUCCESS ) &&
            ( (csr_val & SBSDIO_HT_AVAIL) == 0 ) &&
            (loop_count < ( uint32_t )HT_AVAIL_TIMEOUT_MS) )
    {
        cy_rtos_delay_milliseconds( (uint32_t)1 );
        loop_count++;
    }
    if (loop_count >= ( uint32_t )HT_AVAIL_TIMEOUT_MS)
    {
        return WHD_TIMEOUT;
    }
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("HT clock not available\n") );
        return result;
    }

    /* Set up the interrupt mask and enable interrupts */
    CHECK_RETURN(whd_bus_spi_write_backplane_value(whd_driver, SDIO_INT_HOST_MASK(
                                                       whd_driver), (uint8_t)4, I_HMB_SW_MASK) );

    /* Lower F2 Watermark to avoid DMA Hang in F2 when SD Clock is stopped. */
    return whd_bus_spi_write_register_value(whd_driver, BACKPLANE_FUNCTION, SDIO_FUNCTION2_WATERMARK, (uint8_t)1,
                                            ( uint32_t )SPI_F2_WATERMARK);
}

whd_result_t whd_bus_spi_wakeup(whd_driver_t whd_driver)
{
    uint32_t spi_bus_reg_value;

    /* Wake up WLAN SPI interface module */
    CHECK_RETURN(whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, sizeof(uint32_t),
                                                 (uint8_t *)&spi_bus_reg_value) );
    spi_bus_reg_value |= ( uint32_t )(WAKE_UP);
    return whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, sizeof(uint32_t),
                                            spi_bus_reg_value);
}

whd_result_t whd_bus_spi_sleep(whd_driver_t whd_driver)
{
    uint32_t spi_bus_reg_value;

    /* Put SPI interface block to sleep */
    CHECK_RETURN(whd_bus_spi_read_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, sizeof(uint32_t),
                                                 (uint8_t *)&spi_bus_reg_value) );
    spi_bus_reg_value &= ~( uint32_t )(WAKE_UP);
    return whd_bus_spi_write_register_value(whd_driver, BUS_FUNCTION, SPI_BUS_CONTROL, sizeof(uint32_t),
                                            spi_bus_reg_value);
}

void whd_bus_spi_init_stats(whd_driver_t whd_driver)
{
    whd_bt_dev_t btdev = whd_driver->bt_dev;

    if (btdev && btdev->intr)
    {
        whd_result_t res;
        /* Enable F1 INT in order to receive interrupt from BT FW with HMB_FC_CHANGED in SPI */
        res = whd_bus_write_register_value(whd_driver, BUS_FUNCTION, (uint32_t)SPI_INTERRUPT_ENABLE_REGISTER,
                                           (uint8_t)2,
                                           ( uint32_t )(F1_INTR | F2_F3_FIFO_RD_UNDERFLOW | F2_F3_FIFO_WR_OVERFLOW |
                                                        COMMAND_ERROR | DATA_ERROR | F2_PACKET_AVAILABLE |
                                                        F1_OVERFLOW) );
        if (res != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("spi interrupt register failed to enable\n") );
        }

        res = whd_bus_spi_write_backplane_value(whd_driver, SDIO_INT_HOST_MASK(
                                                    whd_driver), (uint8_t)4, I_HMB_FC_CHANGE);
        if (res != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("sdio int host mask failed to enable\n") );
        }
    }
}

whd_result_t whd_bus_spi_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    return WHD_TRUE;
}

/* Waking the firmware up from Deep Sleep */
whd_result_t whd_bus_spi_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    return WHD_TRUE;
}

uint8_t whd_bus_spi_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return WHD_BUS_SPI_BACKPLANE_READ_PADD_SIZE;
}

whd_bool_t whd_bus_spi_use_status_report_scheme(whd_driver_t whd_driver)
{
    return WHD_FALSE;
}

uint32_t whd_bus_spi_get_max_transfer_size(whd_driver_t whd_driver)
{
    return WHD_BUS_SPI_MAX_BACKPLANE_TRANSFER_SIZE;
}

#if (CYHAL_API_VERSION >= 2)
static void whd_bus_spi_oob_irq_handler(void *arg, cyhal_gpio_event_t event)
#else
static void whd_bus_spi_oob_irq_handler(void *arg, cyhal_gpio_irq_event_t event)
#endif
{
    whd_driver_t whd_driver = (whd_driver_t)arg;
    const whd_oob_config_t *config = &whd_driver->bus_priv->spi_config.oob_config;
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

        return;
    }

    /* call thread notify to wake up WHD thread */
    whd_thread_notify_irq(whd_driver);
}

/* XXX FIXME */
#define WLAN_INTR_PRIORITY 1
whd_result_t whd_bus_spi_irq_register(whd_driver_t whd_driver)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->spi_config.oob_config;

    cyhal_gpio_init(config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_ANALOG,
                    (config->is_falling_edge == WHD_TRUE) ? 1 : 0);
#if (CYHAL_API_VERSION >= 2)
    static cyhal_gpio_callback_data_t cbdata;
    cbdata.callback = whd_bus_spi_oob_irq_handler;
    cbdata.callback_arg = whd_driver;
    cyhal_gpio_register_callback(config->host_oob_pin, &cbdata);
#else
    cyhal_gpio_register_irq(config->host_oob_pin, WLAN_INTR_PRIORITY, whd_bus_spi_oob_irq_handler,
                            whd_driver);
#endif
    return WHD_TRUE;
}

whd_result_t whd_bus_spi_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
    const whd_oob_config_t *config = &whd_driver->bus_priv->spi_config.oob_config;
#if (CYHAL_API_VERSION >= 2)
    const cyhal_gpio_event_t event =
        (config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;

    cyhal_gpio_enable_event(config->host_oob_pin, event, WLAN_INTR_PRIORITY, (enable == WHD_TRUE) ? true : false);
#else
    const cyhal_gpio_irq_event_t event =
        (config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;

    cyhal_gpio_irq_enable(config->host_oob_pin, event, (enable == WHD_TRUE) ? true : false);
#endif
    return WHD_TRUE;
}

static whd_result_t whd_bus_spi_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage)
{
    return WHD_SUCCESS;
}

static whd_result_t whd_bus_spi_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                  whd_bool_t direct_resource, uint32_t address, uint32_t image_size)
{
    whd_result_t result = WHD_SUCCESS;
    uint8_t *image;
    uint32_t blocks_count = 0;
    uint32_t i;
    uint32_t size_out;
    uint32_t reset_instr = 0;

    result = whd_get_resource_no_of_blocks(whd_driver, resource, &blocks_count);
    if (result != WHD_SUCCESS)
    {
        WPRINT_WHD_ERROR( ("Fatal error: download_resource blocks count not known, %s failed at line %d \n", __func__,
                           __LINE__) );
        goto exit;
    }

    for (i = 0; i < blocks_count; i++)
    {
        CHECK_RETURN(whd_get_resource_block(whd_driver, resource, i, (const uint8_t **)&image, &size_out) );
        if ( (resource == WHD_RESOURCE_WLAN_FIRMWARE) && (reset_instr == 0) )
        {
            /* Copy the starting address of the firmware into a global variable */
            reset_instr = *( (uint32_t *)(&image[0]) );
        }
        result = whd_bus_transfer_backplane_bytes(whd_driver, BUS_WRITE, address, size_out, &image[0]);
        if (result != WHD_SUCCESS)
        {
            WPRINT_WHD_ERROR( ("%s: Failed to write firmware image\n", __FUNCTION__) );
            goto exit;
        }
        address += size_out;
    }

    /* Below part of the code is applicable to arm_CR4 type chips only
     * The CR4 chips by default firmware is not loaded at 0. So we need
     * load the first 32 bytes with the offset of the firmware load address
     * which is been copied before during the firmware download
     */
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
exit: return result;
}

static whd_result_t whd_bus_spi_write_wifi_nvram_image(whd_driver_t whd_driver)
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

    CHECK_RETURN(whd_bus_spi_download_resource(whd_driver, WHD_RESOURCE_WLAN_NVRAM, WHD_FALSE, img_base, image_size) );

    /* Write the variable image size at the end */
    image_size = (~(image_size / 4) << 16) | (image_size / 4);

    img_end += GET_C_VAR(whd_driver, ATCM_RAM_BASE_ADDRESS);

    CHECK_RETURN(whd_bus_write_backplane_value(whd_driver, (uint32_t)img_end, 4, image_size) );
    return WHD_SUCCESS;
}

/*
 * Update the backplane window registers
 */
static whd_result_t whd_bus_spi_set_backplane_window(whd_driver_t whd_driver, uint32_t addr, uint32_t *curbase)
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

#endif /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */

