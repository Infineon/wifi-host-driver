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

#ifndef INCLUDED_WHD_COMMONRING_H
#define INCLUDED_WHD_COMMONRING_H

#include "cyabs_rtos.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MIN
#define MIN(x, y) ( (x) < (y) ? (x) : (y) )
#endif /* MIN */

#ifndef MAX
#define MAX(x, y) ( (x) > (y) ? (x) : (y) )
#endif /* MAX */

struct whd_commonring
{
    uint16_t r_ptr;
    uint16_t w_ptr;
    uint16_t f_ptr;
    uint16_t depth;
    uint16_t item_len;

    void *buf_addr;

    int (*cr_ring_bell)(void *ctx);
    int (*cr_update_rptr)(void *ctx);
    int (*cr_update_wptr)(void *ctx);
    int (*cr_write_rptr)(void *ctx);
    int (*cr_write_wptr)(void *ctx);

    void *cr_ctx;

    cy_semaphore_t lock;
    //unsigned long flags;
    uint8_t inited;
    uint8_t was_full;

    //atomic_t outstanding_tx;
};

void whd_commonring_register_cb(struct whd_commonring *commonring,
                                int (*cr_ring_bell)(void *ctx),
                                int (*cr_update_rptr)(void *ctx),
                                int (*cr_update_wptr)(void *ctx),
                                int (*cr_write_rptr)(void *ctx),
                                int (*cr_write_wptr)(void *ctx), void *ctx);
whd_result_t whd_commonring_config(struct whd_commonring *commonring, uint16_t depth,
                           uint16_t item_len, void *buf_addr);
whd_result_t whd_commonring_lock(struct whd_commonring *commonring);
void whd_commonring_unlock(struct whd_commonring *commonring);
bool whd_commonring_write_available(struct whd_commonring *commonring);
void *whd_commonring_reserve_for_write(struct whd_commonring *commonring);
void *whd_commonring_reserve_for_write_multiple(struct whd_commonring *commonring,
                                                uint16_t n_items, uint16_t *alloced);
int whd_commonring_write_complete(struct whd_commonring *commonring);
void whd_commonring_write_cancel(struct whd_commonring *commonring,
                                 uint16_t n_items);
void *whd_commonring_get_read_ptr(struct whd_commonring *commonring,
                                  uint16_t *n_items);
int whd_commonring_read_complete(struct whd_commonring *commonring,
                                 uint16_t n_items);

#define whd_commonring_n_items(commonring) (commonring->depth)
#define whd_commonring_len_item(commonring) (commonring->item_len)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif    /* INCLUDED_WHD_COMMONRING_H */
