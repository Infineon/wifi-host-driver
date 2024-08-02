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
 * Implements functions called by WHD user APIs, but not directly exposed to user
 *
 * This file provides functions which are not directly exposed to user but, called by end-user functions which allow actions such as
 * seting the MAC address, getting channel info, etc
 */

#include "whd_utils.h"
#include "whd_int.h"
#include "whd_proto.h"
#ifndef PROTO_MSGBUF
#include "whd_cdc_bdc.h"
#else
#include "whd_msgbuf.h"
#endif
#include "bus_protocols/whd_bus.h"

/******************************************************
* @cond       Constants
******************************************************/

/******************************************************
*             Local Structures
******************************************************/



/******************************************************
*                   Variables
******************************************************/

/******************************************************
*             Function definitions
******************************************************/

whd_result_t whd_proto_attach(whd_driver_t whd_driver)
{
    struct whd_proto *proto;

    proto = (struct whd_proto *)whd_mem_malloc(sizeof(struct whd_proto) );
    if (!proto)
    {
        return WHD_MALLOC_FAILURE;
    }
    whd_driver->proto = proto;

#ifndef PROTO_MSGBUF
    if (whd_cdc_bdc_info_init(whd_driver) != WHD_SUCCESS)
    {
        goto fail;
    }
#else
    if (whd_msgbuf_info_init(whd_driver) != WHD_SUCCESS)
    {
        goto fail;
    }
#endif /* PROTO_MSGBUF */

    return WHD_SUCCESS;

fail:
    WPRINT_WHD_ERROR( ("%s, proto type %d\n", __FUNCTION__, whd_driver->proto_type) );

    whd_mem_free(proto);
    whd_driver->proto = NULL;
    return WHD_WLAN_NOMEM;
}

whd_result_t whd_proto_detach(whd_driver_t whd_driver)
{
    if (whd_driver->proto)
    {
#ifndef PROTO_MSGBUF
        whd_cdc_bdc_info_deinit(whd_driver);
#else
        whd_msgbuf_info_deinit(whd_driver);
#endif /* PROTO_MSGBUF */
        whd_mem_free(whd_driver->proto);
        whd_driver->proto = NULL;
    }
    return WHD_SUCCESS;
}
