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

/**
 * @file WHD proto
 *
 * Utilities to help do specialized (not general purpose) WHD specific things
 */

#ifndef INCLUDED_WHD_PROTO_H_
#define INCLUDED_WHD_PROTO_H_

#include "whd.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
*             Structures
******************************************************/
struct whd_proto
{
    void *(*get_ioctl_buffer)(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t data_length);
    void *(*get_iovar_buffer)(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t data_length, const char *name);
    whd_result_t (*set_ioctl)(whd_interface_t ifp, uint32_t command, whd_buffer_t send_buffer_hnd,
                              whd_buffer_t *response_buffer_hnd);
    whd_result_t (*get_ioctl)(whd_interface_t ifp, uint32_t command, whd_buffer_t send_buffer_hnd,
                              whd_buffer_t *response_buffer_hnd);
    whd_result_t (*set_iovar)(whd_interface_t ifp, whd_buffer_t send_buffer_hnd, whd_buffer_t *response_buffer_hnd);
    whd_result_t (*get_iovar)(whd_interface_t ifp, whd_buffer_t send_buffer_hnd, whd_buffer_t *response_buffer_hnd);
    whd_result_t (*tx_queue_data)(whd_interface_t ifp, whd_buffer_t buffer);
    void *pd;
};

whd_result_t whd_proto_attach(whd_driver_t whd_driver);

whd_result_t whd_proto_detach(whd_driver_t whd_driver);

static inline void *whd_proto_get_ioctl_buffer(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t data_length)
{
    return whd_driver->proto->get_ioctl_buffer(whd_driver, buffer, data_length);
}

static inline void *whd_proto_get_iovar_buffer(whd_driver_t whd_driver, whd_buffer_t *buffer, uint16_t data_length,
                                               const char *name)
{
    return whd_driver->proto->get_iovar_buffer(whd_driver, buffer, data_length, name);
}

static inline whd_result_t whd_proto_set_ioctl(whd_interface_t ifp, uint32_t command, whd_buffer_t send_buffer_hnd,
                                               whd_buffer_t *response_buffer_hnd)
{
    return ifp->whd_driver->proto->set_ioctl(ifp, command, send_buffer_hnd, response_buffer_hnd);
}

static inline whd_result_t whd_proto_get_ioctl(whd_interface_t ifp, uint32_t command, whd_buffer_t send_buffer_hnd,
                                               whd_buffer_t *response_buffer_hnd)
{
    return ifp->whd_driver->proto->get_ioctl(ifp, command, send_buffer_hnd, response_buffer_hnd);
}

static inline whd_result_t whd_proto_set_iovar(whd_interface_t ifp, whd_buffer_t send_buffer_hnd,
                                               whd_buffer_t *response_buffer_hnd)
{
    return ifp->whd_driver->proto->set_iovar(ifp, send_buffer_hnd, response_buffer_hnd);
}

static inline whd_result_t whd_proto_get_iovar(whd_interface_t ifp, whd_buffer_t send_buffer_hnd,
                                               whd_buffer_t *response_buffer_hnd)
{
    return ifp->whd_driver->proto->get_iovar(ifp, send_buffer_hnd, response_buffer_hnd);
}

static inline whd_result_t whd_proto_tx_queue_data(whd_interface_t ifp, whd_buffer_t buffer)
{
    return ifp->whd_driver->proto->tx_queue_data(ifp, buffer);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
