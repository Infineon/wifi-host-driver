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
 *  Provides generic COMMON RING functionality that chip specific files use
 */
#ifdef PROTO_MSGBUF

#include "whd_debug.h"
#include "whd_commonring.h"
#include "whd_types_int.h"

void whd_commonring_register_cb(struct whd_commonring *commonring,
                                int (*cr_ring_bell)(void *ctx),
                                int (*cr_update_rptr)(void *ctx),
                                int (*cr_update_wptr)(void *ctx),
                                int (*cr_write_rptr)(void *ctx),
                                int (*cr_write_wptr)(void *ctx), void *ctx)
{
    commonring->cr_ring_bell = cr_ring_bell;
    commonring->cr_update_rptr = cr_update_rptr;
    commonring->cr_update_wptr = cr_update_wptr;
    commonring->cr_write_rptr = cr_write_rptr;
    commonring->cr_write_wptr = cr_write_wptr;
    commonring->cr_ctx = ctx;
}

whd_result_t whd_commonring_config(struct whd_commonring *commonring, uint16_t depth,
                           uint16_t item_len, void *buf_addr)
{
    commonring->depth = depth;
    commonring->item_len = item_len;
    commonring->buf_addr = buf_addr;
    if (!commonring->inited)
    {
        (void)cy_rtos_init_semaphore(&commonring->lock, 1, 0);
        CHECK_RETURN(cy_rtos_set_semaphore(&commonring->lock, WHD_FALSE));
        commonring->inited = true;
    }
    commonring->r_ptr = 0;
    if (commonring->cr_write_rptr)
        commonring->cr_write_rptr(commonring->cr_ctx);
    commonring->w_ptr = 0;
    if (commonring->cr_write_wptr)
        commonring->cr_write_wptr(commonring->cr_ctx);
    commonring->f_ptr = 0;

    return 0;
}

whd_result_t whd_commonring_lock(struct whd_commonring *commonring)
{
    uint32_t result;
    result = cy_rtos_get_semaphore(&commonring->lock, (uint32_t)10000,
                                   WHD_FALSE);
    return result;
}

void whd_commonring_unlock(struct whd_commonring *commonring)
{
    uint32_t result;
    result = cy_rtos_set_semaphore(&commonring->lock, WHD_FALSE);
    if (result != WHD_SUCCESS)
        WPRINT_WHD_ERROR( ("Error setting semaphore in %s at %d \n", __func__, __LINE__) );
}

bool whd_commonring_write_available(struct whd_commonring *commonring)
{
    uint16_t available;
    uint8_t retry = 1;

again:
    if (commonring->r_ptr <= commonring->w_ptr)
        available = commonring->depth - commonring->w_ptr +
                    commonring->r_ptr;
    else
        available = commonring->r_ptr - commonring->w_ptr;

    if (available > 1)
    {
        if (!commonring->was_full)
            return true;
        if (available > commonring->depth / 8)
        {
            commonring->was_full = false;
            return true;
        }
        if (retry)
        {
            if (commonring->cr_update_rptr)
                commonring->cr_update_rptr(commonring->cr_ctx);
            retry = false;
            goto again;
        }
        return false;
    }

    if (retry)
    {
        if (commonring->cr_update_rptr)
            commonring->cr_update_rptr(commonring->cr_ctx);
        retry = false;
        goto again;
    }

    commonring->was_full = true;
    return false;
}

void *whd_commonring_reserve_for_write(struct whd_commonring *commonring)
{
    void *ret_ptr;
    uint16_t available;
    uint8_t retry = true;

again:
    if (commonring->r_ptr <= commonring->w_ptr)
        available = commonring->depth - commonring->w_ptr +
                    commonring->r_ptr;
    else
        available = commonring->r_ptr - commonring->w_ptr;

    WPRINT_WHD_DEBUG( ("Write Availability for the Ring - %d \n", available) );

    if (available > 1)
    {
        WPRINT_WHD_DEBUG( ("%s: available more than 1 buf_addr - 0x%lx, w_ptr - 0x%x, item_len - 0x%x \n", __func__,
                           (uint32_t)commonring->buf_addr, commonring->w_ptr, commonring->item_len) );
        ret_ptr = (uint8_t *)commonring->buf_addr +
                  (commonring->w_ptr * commonring->item_len);
        commonring->w_ptr++;
        if (commonring->w_ptr == commonring->depth)
            commonring->w_ptr = 0;
        return ret_ptr;
    }

    if (retry)
    {
        if (commonring->cr_update_rptr)
            commonring->cr_update_rptr(commonring->cr_ctx);
        retry = false;
        goto again;
    }

    commonring->was_full = true;
    WPRINT_WHD_DEBUG( ("Error: CommonRing was Full!!! \n") );

    return NULL;
}

void *
whd_commonring_reserve_for_write_multiple(struct whd_commonring *commonring,
                                          uint16_t n_items, uint16_t *alloced)
{
    void *ret_ptr;
    uint16_t available;
    uint8_t retry = true;

again:
    if (commonring->r_ptr <= commonring->w_ptr)
        available = commonring->depth - commonring->w_ptr +
                    commonring->r_ptr;
    else
        available = commonring->r_ptr - commonring->w_ptr;

    if (available > 1)
    {
        ret_ptr = (uint8_t *)commonring->buf_addr +
                  (commonring->w_ptr * commonring->item_len);
        *alloced = MIN(n_items, available - 1);
        if (*alloced + commonring->w_ptr > commonring->depth)
            *alloced = commonring->depth - commonring->w_ptr;
        commonring->w_ptr += *alloced;
        if (commonring->w_ptr == commonring->depth)
            commonring->w_ptr = 0;
        return ret_ptr;
    }

    if (retry)
    {
        if (commonring->cr_update_rptr)
            commonring->cr_update_rptr(commonring->cr_ctx);
        retry = false;
        goto again;
    }

    commonring->was_full = true;
    return NULL;
}

int whd_commonring_write_complete(struct whd_commonring *commonring)
{
    if (commonring->f_ptr > commonring->w_ptr)
        commonring->f_ptr = 0;

    commonring->f_ptr = commonring->w_ptr;

    if (commonring->cr_write_wptr)
        commonring->cr_write_wptr(commonring->cr_ctx);
    if (commonring->cr_ring_bell)
        return commonring->cr_ring_bell(commonring->cr_ctx);

    return -1;
}

void whd_commonring_write_cancel(struct whd_commonring *commonring,
                                 uint16_t n_items)
{
    if (commonring->w_ptr == 0)
        commonring->w_ptr = commonring->depth - n_items;
    else
        commonring->w_ptr -= n_items;
}

void *whd_commonring_get_read_ptr(struct whd_commonring *commonring,
                                  uint16_t *n_items)
{
    if (commonring->cr_update_wptr)
        commonring->cr_update_wptr(commonring->cr_ctx);

    *n_items = (commonring->w_ptr >= commonring->r_ptr) ?
               (commonring->w_ptr - commonring->r_ptr) :
               (commonring->depth - commonring->r_ptr);

    if (*n_items == 0)
        return NULL;

    return (uint8_t *)commonring->buf_addr +
           (commonring->r_ptr * commonring->item_len);
}

int whd_commonring_read_complete(struct whd_commonring *commonring,
                                 uint16_t n_items)
{
    commonring->r_ptr += n_items;

    if (commonring->r_ptr == commonring->depth)
        commonring->r_ptr = 0;

    if (commonring->cr_write_rptr)
        return commonring->cr_write_rptr(commonring->cr_ctx);

    return -1;
}

#endif /* PROTO_MSGBUF */
