/*
 * Copyright 2025, Cypress Semiconductor Corporation (an Infineon company)
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
 *
 */

#include "whd_utils.h"
#include "whd_bus.h"
#include "whd_int.h"


whd_driver_t g_bt_whd_driver;

whd_result_t whd_bus_write_reg_value(whd_driver_t whd_driver, uint32_t address,
                                     uint8_t value_length, uint32_t value)
{
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );
    return whd_bus_write_backplane_value(whd_driver, address, value_length, value);

}

whd_result_t whd_bus_read_reg_value(whd_driver_t whd_driver, uint32_t address,
                                    uint8_t value_length, uint8_t *value)
{
    CHECK_RETURN(whd_ensure_wlan_bus_is_up(whd_driver) );
    return whd_bus_read_backplane_value(whd_driver, address, value_length, value);

}

whd_result_t whd_bus_share_bt_init(whd_driver_t whd_driver)
{
    if (!whd_driver)
        return WHD_WLAN_ERROR;
    g_bt_whd_driver = whd_driver;
    return WHD_SUCCESS;
}

whd_driver_t whd_bt_get_whd_driver(void)
{
    if (g_bt_whd_driver)
        return g_bt_whd_driver;
    else
        return NULL;
}

whd_result_t whd_bus_bt_attach(whd_driver_t whd_driver, void *btdata,
                               void (*bt_int_fun)(void *data) )
{
    whd_bt_dev_t btdev;
    if (whd_driver->bt_dev)
    {
        return WHD_SUCCESS;
    }
    /* Allocate bt dev */
    btdev = (whd_bt_dev_t)whd_mem_malloc(sizeof(struct whd_bt_dev) );
    if (btdev == NULL)
    {
        WPRINT_WHD_ERROR( ("Memory allocation failed for whd_bt_dev_t in %s\n", __FUNCTION__) );
        return WHD_BUFFER_UNAVAILABLE_PERMANENT;
    }
    btdev->bt_data = btdata;
    btdev->intr = WHD_TRUE;
    whd_driver->bt_dev = btdev;
    whd_bus_init_stats(whd_driver);
    btdev->bt_int_cb = bt_int_fun;
    if (!btdev->bt_int_cb)
    {
        btdev->intr = WHD_FALSE;
    }
    return WHD_SUCCESS;
}

void whd_bus_bt_detach(whd_driver_t whd_driver)
{
    whd_bt_dev_t btdev = whd_driver->bt_dev;
    if (btdev)
    {
        if (btdev->bt_data)
            btdev->bt_data = NULL;
        if (btdev->bt_int_cb)
            btdev->bt_int_cb = NULL;
        if (whd_driver->bt_dev)
        {
            whd_driver->bt_dev = NULL;
            whd_mem_free(btdev);
        }
    }
}

whd_result_t whd_bus_init(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_init_fptr(whd_driver);
}

whd_result_t whd_bus_deinit(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_deinit_fptr(whd_driver);
}

whd_bool_t whd_bus_wake_interrupt_present(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_wake_interrupt_present_fptr(whd_driver);
}

whd_result_t whd_bus_send_buffer(whd_driver_t whd_driver, whd_buffer_t buffer)
{
    return whd_driver->bus_if->whd_bus_send_buffer_fptr(whd_driver, buffer);
}

uint32_t whd_bus_packet_available_to_read(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_packet_available_to_read_fptr(whd_driver);
}

whd_result_t whd_bus_read_frame(whd_driver_t whd_driver, whd_buffer_t *buffer)
{
    return whd_driver->bus_if->whd_bus_read_frame_fptr(whd_driver, buffer);
}

whd_result_t whd_bus_write_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                           uint32_t value)
{
    return whd_driver->bus_if->whd_bus_write_backplane_value_fptr(whd_driver, address, register_length, value);
}

whd_result_t whd_bus_read_backplane_value(whd_driver_t whd_driver, uint32_t address, uint8_t register_length,
                                          uint8_t *value)
{
    return whd_driver->bus_if->whd_bus_read_backplane_value_fptr(whd_driver, address, register_length, value);
}

whd_result_t whd_bus_read_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                         uint8_t value_length, uint8_t *value)
{
    return whd_driver->bus_if->whd_bus_read_register_value_fptr(whd_driver, function, address, value_length, value);
}

whd_result_t whd_bus_write_register_value(whd_driver_t whd_driver, whd_bus_function_t function, uint32_t address,
                                          uint8_t value_length, uint32_t value)
{
    return whd_driver->bus_if->whd_bus_write_register_value_fptr(whd_driver, function, address, value_length, value);
}

whd_result_t whd_bus_transfer_bytes(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                    whd_bus_function_t function, uint32_t address, uint16_t size,
                                    whd_transfer_bytes_packet_t *data)
{
    return whd_driver->bus_if->whd_bus_transfer_bytes_fptr(whd_driver, direction, function, address, size, data);
}

whd_result_t whd_bus_poke_wlan(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_poke_wlan_fptr(whd_driver);
}

whd_result_t whd_bus_wakeup(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_wakeup_fptr(whd_driver);
}

whd_result_t whd_bus_sleep(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_sleep_fptr(whd_driver);
}

whd_result_t whd_bus_wait_for_wlan_event(whd_driver_t whd_driver, cy_semaphore_t *transceive_semaphore)
{
    return whd_driver->bus_if->whd_bus_wait_for_wlan_event_fptr(whd_driver, transceive_semaphore);
}

whd_bool_t whd_bus_use_status_report_scheme(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_use_status_report_scheme_fptr(whd_driver);
}

uint8_t whd_bus_backplane_read_padd_size(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_backplane_read_padd_size_fptr(whd_driver);
}

uint32_t whd_bus_get_max_transfer_size(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_get_max_transfer_size_fptr(whd_driver);
}

void whd_bus_init_stats(whd_driver_t whd_driver)
{
    whd_driver->bus_if->whd_bus_init_stats_fptr(whd_driver);
}

whd_result_t whd_bus_print_stats(whd_driver_t whd_driver, whd_bool_t reset_after_print)
{
    return whd_driver->bus_if->whd_bus_print_stats_fptr(whd_driver, reset_after_print);
}

whd_result_t whd_bus_reinit_stats(whd_driver_t whd_driver, whd_bool_t wake_from_firmware)
{
    return whd_driver->bus_if->whd_bus_reinit_stats_fptr(whd_driver, wake_from_firmware);
}

whd_result_t whd_bus_irq_register(whd_driver_t whd_driver)
{
    return whd_driver->bus_if->whd_bus_irq_register_fptr(whd_driver);
}

whd_result_t whd_bus_irq_enable(whd_driver_t whd_driver, whd_bool_t enable)
{
    return whd_driver->bus_if->whd_bus_irq_enable_fptr(whd_driver, enable);
}

whd_result_t whd_bus_download_resource(whd_driver_t whd_driver, whd_resource_type_t resource,
                                       whd_bool_t direct_resource, uint32_t address, uint32_t image_size)
{
    return whd_driver->bus_if->whd_bus_download_resource_fptr(whd_driver, resource, direct_resource, address,
                                                              image_size);
}

#ifdef BLHS_SUPPORT
whd_result_t whd_bus_common_blhs(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage)
{
    return whd_driver->bus_if->whd_bus_blhs_fptr(whd_driver, stage);
}

#endif
