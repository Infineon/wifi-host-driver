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
#ifdef PROTO_MSGBUF

#include "whd_flowring.h"
#include "whd_buffer_api.h"
#include "whd_utils.h"

#define WHD_FLOWRING_HIGH           1024
#define WHD_FLOWRING_LOW            (WHD_FLOWRING_HIGH - 256)
#define WHD_FLOWRING_INVALID_IFIDX  0xff

#define WHD_FLOWRING_HASH_AP(da, fifo, ifidx) (da[5] * 2 + fifo + ifidx * 16)
#define WHD_FLOWRING_HASH_STA(fifo, ifidx)    (fifo + ifidx * 16)

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

static const uint8_t ALLFFMAC[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static uint8_t whd_flowring_is_tdls_mac(struct whd_flowring *flow, uint8_t mac[ETHER_ADDR_LEN])
{
    struct whd_flowring_tdls_entry *search;

    search = flow->tdls_entry;

    while (search)
    {
        if (memcmp(search->mac, mac, ETHER_ADDR_LEN) == 0)
            return 1;
        search = search->next;
    }

    return 0;
}

uint32_t whd_flowring_lookup(struct whd_flowring *flow, uint8_t da[ETHER_ADDR_LEN], uint8_t prio, uint8_t ifidx)
{
    struct whd_flowring_hash *hash;
    uint16_t hash_idx;
    uint32_t i;
    uint8_t found;
    uint8_t sta;
    uint8_t fifo;
    uint8_t *mac;

    fifo = whd_flowring_prio2fifo[prio];
    sta = (flow->addr_mode[ifidx] == ADDR_INDIRECT);
    mac = da;
    if ( (!sta) && (is_multicast_ether_addr(da) ) )
    {
        mac = (uint8_t *)ALLFFMAC;
        fifo = 0;
    }

    if ( (sta) && (flow->tdls_active) &&
         (whd_flowring_is_tdls_mac(flow, da) ) )
    {
        sta = 0;
    }

    hash_idx =  sta ? WHD_FLOWRING_HASH_STA(fifo, ifidx) :
               WHD_FLOWRING_HASH_AP(mac, fifo, ifidx);
    hash_idx &= (WHD_FLOWRING_HASHSIZE - 1);
    found = 0;
    hash = flow->hash;
    for (i = 0; i < WHD_FLOWRING_HASHSIZE; i++)
    {
        if ( (sta || (memcmp(hash[hash_idx].mac, mac, ETHER_ADDR_LEN) == 0) ) &&
             (hash[hash_idx].fifo == fifo) &&
             (hash[hash_idx].ifidx == ifidx) )
        {
            found = 1;
            break;
        }
        hash_idx++;
        hash_idx &= (WHD_FLOWRING_HASHSIZE - 1);
    }
    if (found)
        return hash[hash_idx].flowid;

    return WHD_FLOWRING_INVALID_ID;
}

uint32_t whd_flowring_create(struct whd_flowring *flow, uint8_t da[ETHER_ADDR_LEN],
                             uint8_t prio, uint8_t ifidx)
{
    struct whd_flowring_ring *ring;
    struct whd_flowring_hash *hash;
    uint16_t hash_idx;
    uint32_t i;
    uint8_t found;
    uint8_t fifo;
    uint8_t sta;
    uint8_t *mac;

    fifo = whd_flowring_prio2fifo[prio];
    sta = (flow->addr_mode[ifidx] == ADDR_INDIRECT);
    mac = da;
    if ( (!sta) && (is_multicast_ether_addr(da) ) )
    {
        mac = (uint8_t *)ALLFFMAC;
        fifo = 0;
    }

    if ( (sta) && (flow->tdls_active) &&
         (whd_flowring_is_tdls_mac(flow, da) ) )
    {
        sta = 0;
    }

    hash_idx =  sta ? WHD_FLOWRING_HASH_STA(fifo, ifidx) :
               WHD_FLOWRING_HASH_AP(mac, fifo, ifidx);
    hash_idx &= (WHD_FLOWRING_HASHSIZE - 1);
    found = 0;
    hash = flow->hash;

    for (i = 0; i < WHD_FLOWRING_HASHSIZE; i++)
    {
        if (hash[hash_idx].ifidx == WHD_FLOWRING_INVALID_IFIDX)
        {
            found = 1;
            break;
        }
        hash_idx++;
        hash_idx &= (WHD_FLOWRING_HASHSIZE - 1);
    }

    if (found)
    {

        for (i = 0; i < flow->nrofrings; i++)
        {
            if (flow->rings[i] == NULL)
                break;
        }

        if (i == flow->nrofrings)
            return -1;

        ring = whd_mem_malloc(sizeof(*ring) );
        if (!ring)
            return -1;

        memcpy(hash[hash_idx].mac, mac, ETHER_ADDR_LEN);
        hash[hash_idx].fifo = fifo;
        hash[hash_idx].ifidx = ifidx;
        hash[hash_idx].flowid = i;

        ring->hash_id = hash_idx;
        ring->status = RING_CLOSED;
        whd_msgbuf_txflow_init(&ring->txflow_queue);
        flow->rings[i] = ring;

        return i;
    }

    return WHD_FLOWRING_INVALID_ID;
}

void whd_flowring_delete(struct whd_flowring *flow, uint16_t flowid)
{
    whd_result_t result;
    whd_driver_t whd_driver = flow->dev;
    struct whd_flowring_ring *ring;
    uint16_t hash_idx;
    whd_buffer_t skb;

    ring = flow->rings[flowid];
    if (!ring)
        return;

    hash_idx = ring->hash_id;
    flow->hash[hash_idx].ifidx = WHD_FLOWRING_INVALID_IFIDX;
    memset(flow->hash[hash_idx].mac, 0, WHD_ETHER_ADDR_LEN);

    (void)whd_msgbuf_txflow_dequeue(whd_driver, &skb, flowid);
    while (skb)
    {
        result = whd_buffer_release(whd_driver, skb, WHD_NETWORK_TX);
        if (result != WHD_SUCCESS)
            WPRINT_WHD_ERROR( ("buffer release failed in %s at %d \n", __func__, __LINE__) );
        (void)whd_msgbuf_txflow_dequeue(whd_driver, &skb, flowid);
    }

    whd_mem_free(ring);
    flow->rings[flowid] = NULL;
    return;
}

uint8_t whd_flowring_tid(struct whd_flowring *flow, uint16_t flowid)
{
    struct whd_flowring_ring *ring;

    ring = flow->rings[flowid];

    return flow->hash[ring->hash_id].fifo;
}

uint32_t whd_flowring_qlen(struct whd_flowring *flow, uint16_t flowid)
{
    struct whd_flowring_ring *ring;

    ring = flow->rings[flowid];
    if (!ring)
        return 0;

    if (ring->status != RING_OPEN)
        return 0;

    return ring->txflow_queue.npkt_in_q;
}

uint8_t whd_flowring_ifidx_get(struct whd_flowring *flow, uint16_t flowid)
{
    struct whd_flowring_ring *ring;
    uint16_t hash_idx;

    ring = flow->rings[flowid];
    hash_idx = ring->hash_id;

    return flow->hash[hash_idx].ifidx;
}

void whd_flowring_detach(struct whd_flowring *flow)
{
    whd_driver_t whd_driver = flow->dev;
    struct whd_flowring_tdls_entry *search;
    struct whd_flowring_tdls_entry *remove;
    uint16_t flowid;

    for (flowid = 0; flowid < flow->nrofrings; flowid++)
    {
        if (flow->rings[flowid])
            whd_msgbuf_delete_flowring(whd_driver, flowid);
    }

    search = flow->tdls_entry;
    while (search)
    {
        remove = search;
        search = search->next;
        whd_mem_free(remove);
    }
    whd_mem_free(flow->rings);
    whd_mem_free(flow);
}

struct whd_flowring *whd_flowring_attach(struct whd_driver *dev, uint16_t nrofrings)
{
    struct whd_flowring *flow;
    uint32_t i;

    flow = whd_mem_malloc(sizeof(*flow) );

    if (flow)
    {
        memset(flow, 0, sizeof(*flow));
        flow->dev = dev;
        flow->nrofrings = nrofrings;

        for (i = 0; i < WHD_MAX_IFS; i++)
            flow->addr_mode[i] = ADDR_INDIRECT;
        for (i = 0; i < WHD_FLOWRING_HASHSIZE; i++)
            flow->hash[i].ifidx = WHD_FLOWRING_INVALID_IFIDX;

        flow->rings = whd_mem_calloc(nrofrings, sizeof(*flow->rings) );

        if (!flow->rings)
        {
            whd_mem_free(flow);
            flow = NULL;
        }

    }

    return flow;
}

void whd_flowring_open(struct whd_flowring *flow, uint16_t flowid)
{
    struct whd_flowring_ring *ring;

    ring = flow->rings[flowid];
    if (!ring)
    {
        WPRINT_WHD_DEBUG( ("Ring NULL, for flowid %d\n", flowid) );
        return;
    }

    ring->status = RING_OPEN;
}

#endif /* PROTO_MSGBUF */
