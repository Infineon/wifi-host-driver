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
 *
 */

#include "whd_bus_protocol_interface.h"
#include "whd_resource_api.h"
#include "whd_bus_common.h"

#ifndef INCLUDED_WHD_BUS_H_
#define INCLUDED_WHD_BUS_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef whd_result_t (*whd_bus_init_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_deinit_t)(whd_driver_t whd_driver);

typedef whd_result_t (*whd_bus_ack_interrupt_t)(whd_driver_t whd_driver, uint32_t intstatus);
typedef whd_bool_t (*whd_bus_wake_interrupt_present_t)(whd_driver_t whd_driver);
typedef uint32_t (*whd_bus_packet_available_to_read_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_read_frame_t)(whd_driver_t whd_driver, whd_buffer_t *buffer);

typedef whd_result_t (*whd_bus_set_backplane_window_t)(whd_driver_t whd_driver, uint32_t addr, uint32_t *cur_base_addr);
typedef whd_result_t (*whd_bus_write_backplane_value_t)(whd_driver_t whd_driver, uint32_t address,
                                                        uint8_t register_length, uint32_t value);
typedef whd_result_t (*whd_bus_read_backplane_value_t)(whd_driver_t whd_driver, uint32_t address,
                                                       uint8_t register_length, uint8_t *value);

typedef whd_result_t (*whd_bus_write_register_value_t)(whd_driver_t whd_driver, whd_bus_function_t function,
                                                       uint32_t address, uint8_t value_length, uint32_t value);
typedef whd_result_t (*whd_bus_read_register_value_t)(whd_driver_t whd_driver, whd_bus_function_t function,
                                                      uint32_t address, uint8_t value_length, uint8_t *value);

typedef whd_result_t (*whd_bus_transfer_bytes_t)(whd_driver_t whd_driver, whd_bus_transfer_direction_t direction,
                                                 whd_bus_function_t function, uint32_t address, uint16_t size,
                                                 whd_transfer_bytes_packet_t *data);

typedef whd_result_t (*whd_bus_poke_wlan_t)(whd_driver_t whd_driver);

typedef whd_result_t (*whd_bus_wakeup_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_sleep_t)(whd_driver_t whd_driver);
typedef uint8_t (*whd_bus_backplane_read_padd_size_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_send_buffer_t)(whd_driver_t whd_driver, whd_buffer_t buffer);
typedef whd_result_t (*whd_bus_wait_for_wlan_event_t)(whd_driver_t whd_driver,
                                                      cy_semaphore_t *transceive_semaphore);
typedef whd_bool_t (*whd_bus_use_status_report_scheme_t)(whd_driver_t whd_driver);
typedef uint32_t (*whd_bus_get_max_transfer_size_t)(whd_driver_t whd_driver);

typedef void (*whd_bus_init_stats_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_print_stats_t)(whd_driver_t whd_driver, whd_bool_t reset_after_print);
typedef whd_result_t (*whd_bus_reinit_stats_t)(whd_driver_t whd_driver, whd_bool_t wake_from_firmware);
typedef whd_result_t (*whd_bus_irq_register_t)(whd_driver_t whd_driver);
typedef whd_result_t (*whd_bus_irq_enable_t)(whd_driver_t whd_driver, whd_bool_t enable);
typedef whd_result_t (*whd_bus_download_resource_t)(whd_driver_t whd_driver, whd_resource_type_t resource,
                                                    whd_bool_t direct_resource, uint32_t address,
                                                    uint32_t image_size);
typedef whd_result_t (*whd_bus_blhs_t)(whd_driver_t whd_driver, whd_bus_blhs_stage_t stage);

typedef struct whd_bus_info
{
    whd_bus_init_t whd_bus_init_fptr;
    whd_bus_deinit_t whd_bus_deinit_fptr;

    whd_bus_ack_interrupt_t whd_bus_ack_interrupt_fptr;
    whd_bus_send_buffer_t whd_bus_send_buffer_fptr;

    whd_bus_wake_interrupt_present_t whd_bus_wake_interrupt_present_fptr;
    whd_bus_packet_available_to_read_t whd_bus_packet_available_to_read_fptr;
    whd_bus_read_frame_t whd_bus_read_frame_fptr;
#ifndef PROTO_MSGBUF
    whd_bus_set_backplane_window_t whd_bus_set_backplane_window_fptr;
#endif
    whd_bus_write_backplane_value_t whd_bus_write_backplane_value_fptr;
    whd_bus_read_backplane_value_t whd_bus_read_backplane_value_fptr;

    whd_bus_write_register_value_t whd_bus_write_register_value_fptr;
    whd_bus_read_register_value_t whd_bus_read_register_value_fptr;

    whd_bus_transfer_bytes_t whd_bus_transfer_bytes_fptr;

    whd_bus_poke_wlan_t whd_bus_poke_wlan_fptr;

    whd_bus_wakeup_t whd_bus_wakeup_fptr;
    whd_bus_sleep_t whd_bus_sleep_fptr;

    whd_bus_backplane_read_padd_size_t whd_bus_backplane_read_padd_size_fptr;

    whd_bus_wait_for_wlan_event_t whd_bus_wait_for_wlan_event_fptr;
    whd_bus_use_status_report_scheme_t whd_bus_use_status_report_scheme_fptr;

    whd_bus_get_max_transfer_size_t whd_bus_get_max_transfer_size_fptr;

    whd_bus_init_stats_t whd_bus_init_stats_fptr;
    whd_bus_print_stats_t whd_bus_print_stats_fptr;
    whd_bus_reinit_stats_t whd_bus_reinit_stats_fptr;
    whd_bus_irq_register_t whd_bus_irq_register_fptr;
    whd_bus_irq_enable_t whd_bus_irq_enable_fptr;
    whd_bus_download_resource_t whd_bus_download_resource_fptr;
#ifdef BLHS_SUPPORT
    whd_bus_blhs_t whd_bus_blhs_fptr;
#endif
} whd_bus_info_t;


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ifndef INCLUDED_WHD_BUS_H_ */

