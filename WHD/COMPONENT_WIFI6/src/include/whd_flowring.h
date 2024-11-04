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

#ifndef INCLUDED_WHD_FLOWRING_H
#define INCLUDED_WHD_FLOWRING_H

#include "stdlib.h"
#include "whd_msgbuf.h"
#include "whd_int.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define WHD_FLOWRING_HASHSIZE       16     /* has to be 2^x */
#define WHD_FLOWRING_INVALID_ID     0xFFFFFFFF

enum proto_addr_mode
{
    ADDR_INDIRECT   = 0,
    ADDR_DIRECT
};

static const uint8_t whd_flowring_prio2fifo[] =
{
    0,
    1,
    1,
    0,
    2,
    2,
    3,
    3
};

static inline int is_multicast_ether_addr(const uint8_t *a)
{
    return a[0] & 0x01;
}

#define WHD_MAX_IFS 2 //todo_check

struct list_head
{
    struct list_head *next, *prev;
};

struct whd_flowring_hash
{
    uint8_t mac[WHD_ETHER_ADDR_LEN];
    uint8_t fifo;
    uint8_t ifidx;
    uint16_t flowid;
};

enum ring_status
{
    RING_CLOSED,
    RING_CLOSING,
    RING_OPEN
};

struct whd_flowring_ring
{
    uint16_t hash_id;
    uint8_t blocked;
    enum ring_status status;
    whd_msgbuftx_info_t txflow_queue;
    uint32_t ac_prio;
};

struct whd_flowring_tdls_entry
{
    uint8_t mac[WHD_ETHER_ADDR_LEN];
    struct whd_flowring_tdls_entry *next;
};

struct whd_flowring
{
    struct whd_driver *dev;
    struct whd_flowring_hash hash[WHD_FLOWRING_HASHSIZE];
    struct whd_flowring_ring **rings;
    uint8_t block_lock;
    enum proto_addr_mode addr_mode[WHD_MAX_IFS];
    uint16_t nrofrings;
    uint8_t tdls_active;
    struct whd_flowring_tdls_entry *tdls_entry;
};

extern uint32_t whd_flowring_lookup(struct whd_flowring *flow, uint8_t da[ETHER_ADDR_LEN], uint8_t prio, uint8_t ifidx);
extern uint32_t whd_flowring_create(struct whd_flowring *flow, uint8_t da[ETHER_ADDR_LEN],
                                    uint8_t prio, uint8_t ifidx);
extern void whd_flowring_delete(struct whd_flowring *flow, uint16_t flowid);
extern uint8_t whd_flowring_tid(struct whd_flowring *flow, uint16_t flowid);
extern uint32_t whd_flowring_qlen(struct whd_flowring *flow, uint16_t flowid);
extern uint8_t whd_flowring_ifidx_get(struct whd_flowring *flow, uint16_t flowid);
extern void whd_flowring_detach(struct whd_flowring *flow);
extern struct whd_flowring *whd_flowring_attach(struct whd_driver *dev, uint16_t nrofrings);
extern void whd_flowring_open(struct whd_flowring *flow, uint16_t flowid);
extern void whd_flowring_delete_peers(struct whd_flowring *flow, uint8_t peer_addr[ETHER_ADDR_LEN], uint8_t ifidx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
