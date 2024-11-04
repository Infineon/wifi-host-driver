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

#if defined(COMPONENT_SDIO_HM)

#include <stdio.h>
#include "sdio_hosted_support.h"
#include "sdio_arbitration.h"

#include "cyabs_rtos.h"
#include "cy_wcm.h"
#include "whd_debug.h"
#include "whd_endian.h"

#define SESSION_TIMEOUT             (16000)
#define SESSION_NUMBER_MAX          (64)
#define MAC_ADDR_LEN                (6)
#define ARP_TABLE_LIST_NUM          (16)
#define IP6_NEIGHBOR_SESSION_NUM    (16)
#define IP6_NEIGHBOR_TIMEOUT        (5000)

#define ETHERNET_HEADER_LEN         (14)
#define IPV4_HEADER_FIXED_LEN       (20)
#define IPV6_HEADER_FIXED_LEN       (40)
#define TCP_HEADER_FIXED_LEN        (20)
#define ICMP6_NS_TARGET_ADDR_OFFSET (8)
#define IPV6_ADDRESS_LEN            (16)
#define ARP_PACKET_SIZE             (28)

#define IP4ADDR_STRLEN_MAX          (16)

#define WHITE_LIST_PORT_MAX         (16)

struct ip_session {
    struct ip_session *next;
    ip_addr_t local_ip;         /* local device ip */
    ip_addr_t remote_ip;        /* remote server ip */
    uint8_t tos;                /* type of service */
    uint16_t sport;             /* source port */
    uint16_t dport;             /* destination port */
    uint32_t time;              /* the latest time of recive the packet */
    uint32_t hash_key;          /* hask key of 5 tuple */
};
typedef struct ip_session ip_session_t;

static bool sdio_arb_query_ip_session(ip_addr_t *local_ip, ip_addr_t *remote_ip,
                         uint16_t sport, uint16_t dport, uint16_t icmp_hash, uint8_t tos, bool update_time);

struct arp_request {
    uint8_t shwaddr[MAC_ADDR_LEN];
    ip4_addr_t sipaddr;
    uint8_t dhwaddr[MAC_ADDR_LEN];
    ip4_addr_t dipaddr;
    uint32_t time;
};
typedef struct arp_request arp_request_t;

cy_timer_t sdio_arb_timer;
cy_mutex_t sdio_arb_mutex;

ip_session_t *g_ip_session = NULL;
int ip_session_number = 0;
arp_request_t g_arp_rq[ARP_TABLE_LIST_NUM] = {0};
struct eth_addr local_eth_addr;
int g_whitelist_port[WHITE_LIST_PORT_MAX] = {0};
int g_whitelist_port_num = 0;

int sdio_arb_inited = 0;

static bool is_eth_multicast(uint8_t *dest)
{
    if((dest[0] & 0x01) == 0x01) {
        return true;
    }
    return false;
}

cy_rslt_t sdio_arb_mutex_init()
{
    return cy_rtos_mutex_init(&sdio_arb_mutex, 1);
}

cy_rslt_t sdio_arb_mutex_deinit()
{
    return cy_rtos_mutex_deinit(&sdio_arb_mutex);
}

static cy_rslt_t sdio_arb_mutex_lock()
{
    return cy_rtos_mutex_get(&sdio_arb_mutex, CY_RTOS_NEVER_TIMEOUT);
}

static cy_rslt_t sdio_arb_mutex_unlock()
{
    return cy_rtos_mutex_set(&sdio_arb_mutex);
}

char *ip4addr_ntoa_r(const ip4_addr_t *addr, char *buf, int buflen)
{
    uint32_t s_addr;
    char inv[3];
    char *rp;
    uint8_t *ap;
    uint8_t rem;
    uint8_t n;
    uint8_t i;
    int len = 0;

    s_addr = ip4_addr_get_u32(addr);

    rp = buf;
    ap = (uint8_t *)&s_addr;
    for (n = 0; n < 4; n++) {
        i = 0;
        do {
            rem = *ap % (uint8_t)10;
            *ap /= (uint8_t)10;
            inv[i++] = (char)('0' + rem);
        } while (*ap);
        while (i--) {
            if (len++ >= buflen) {
                return NULL;
            }
            *rp++ = inv[i];
        }
        if (len++ >= buflen) {
            return NULL;
        }
        *rp++ = '.';
        ap++;
    }
    *--rp = 0;
    return buf;
}

/**
 * Convert numeric IP address into decimal dotted ASCII representation.
 * returns ptr to static buffer; not reentrant!
 *
 * @param addr ip address in network order to convert
 * @return pointer to a global static (!) buffer that holds the ASCII
 *         representation of addr
 */
char *ip4addr_ntoa(const ip4_addr_t *addr)
{
    static char str[IP4ADDR_STRLEN_MAX];
    return ip4addr_ntoa_r(addr, str, IP4ADDR_STRLEN_MAX);
}

static uint32_t ghash_key(ip_addr_t *sip, ip_addr_t *dip, uint16_t sport, uint16_t dport, uint16_t icmp_hash, uint8_t tos)
{
    uint32_t hash_key = 0;
    uint32_t _sip, _dip;

    _sip = ip4_addr_get_u32(&sip->u_addr.ip4);
    _dip = ip4_addr_get_u32(&dip->u_addr.ip4);

    hash_key = _sip ^ (123 * sport)^ _dip ^ (456 * dport)^ (123456 * tos)^ (78 * icmp_hash);
    return hash_key;
}

int sdio_arb_add_ip_session(ip_addr_t *local_ip, ip_addr_t *remote_ip,
                        uint16_t sport, uint16_t dport, uint16_t icmp_hash, uint8_t tos)
{
    ip_session_t *ip_ses;
    cy_time_t cur_time;

    if (sdio_arb_query_ip_session(local_ip, remote_ip, sport, dport, icmp_hash, tos, 1)) {
        return 0;
    }
    if (ip_session_number > SESSION_NUMBER_MAX) {
        return -1;
    }
    ip_ses = whd_mem_malloc(sizeof(ip_session_t));
    if (!ip_ses) {
        PRINT_HM_ERROR(("[%s:%d] malloc buffer failed\n", __func__, __LINE__));
        return -1;
    }
    whd_mem_memcpy(&ip_ses->local_ip, local_ip, sizeof(ip_addr_t));
    whd_mem_memcpy(&ip_ses->remote_ip, remote_ip, sizeof(ip_addr_t));
    ip_ses->sport = sport;
    ip_ses->dport = dport;
    ip_ses->tos = tos;
    cy_rtos_get_time(&cur_time);
    ip_ses->time = cur_time;
    ip_ses->hash_key = ghash_key(local_ip, remote_ip, sport, dport, icmp_hash, tos);
    ip_ses->next = NULL;
    sdio_arb_mutex_lock();

    if (g_ip_session == NULL) {
        g_ip_session = ip_ses;
    } else {
        ip_ses->next = g_ip_session;
        g_ip_session = ip_ses;
    }
    ip_session_number += 1;

    PRINT_HM_DEBUG(("snet session number:%d.\n", ip_session_number));
    sdio_arb_mutex_unlock();
    return 0;
}

int sdio_arb_del_ip_session(ip_session_t *del_ip_ses)
{
    ip_session_t *ip_ses, *pre_ip_ses;
    bool find_session = false;

    sdio_arb_mutex_lock();
    pre_ip_ses = ip_ses = g_ip_session;

    while(ip_ses) {
        if (ip_ses->hash_key == del_ip_ses->hash_key)	{
            find_session = true;
            break;
        }
        pre_ip_ses = ip_ses;
        ip_ses = ip_ses->next;
    }
    if (find_session) {
        if (ip_ses == g_ip_session) {
            g_ip_session = ip_ses->next;
        } else {
            pre_ip_ses->next = ip_ses->next;
        }

        ip_ses->next = NULL;
        free(ip_ses);
        ip_session_number -= 1;
    }
    sdio_arb_mutex_unlock();
    return 0;
}

static bool sdio_arb_query_ip_session(ip_addr_t *local_ip, ip_addr_t *remote_ip,
                         uint16_t sport, uint16_t dport, uint16_t icmp_hash, uint8_t tos, bool update_time)
{
    ip_session_t *ip_ses;
    bool is_present = 0;
    cy_time_t cur_time;
    uint32_t hash_key = 0;

    hash_key = ghash_key(local_ip, remote_ip, sport, dport, icmp_hash, tos);
    sdio_arb_mutex_lock();
    ip_ses = g_ip_session;

    while(ip_ses) {
        if (ip_ses->hash_key == hash_key) {
            if (update_time) {
                cy_rtos_get_time(&cur_time);
                ip_ses->time = cur_time;
                PRINT_HM_DEBUG(("[%s:%d] update ip session time:%lu\n", __func__, __LINE__, ip_ses->time));
            }
            is_present = 1;
            if(icmp_hash) {
             /* For ICMP packet, remove the IP session immediately, otherwise it will affect ping from the device,
                as the session is cached until the session got deleted,
                ICMP will be forwarded to Linux hosts(though ICMP intiated by Device). So delete ICMP session immedaitely */
                sdio_arb_del_ip_session(ip_ses);
            }
            break;
        }
        ip_ses = ip_ses->next;
    }
    sdio_arb_mutex_unlock();
    return is_present;
}

void sdio_arb_print_ip_session()
{
    PRINT_HM_DEBUG(("=== current ip session number:%d ===\n", ip_session_number));
    ip_session_t *ip_ses;
    sdio_arb_mutex_lock();
    ip_ses = g_ip_session;
    while(ip_ses) {
        ip_ses = ip_ses->next;
    }

    sdio_arb_mutex_unlock();
}

void sdio_arb_update_arp_table(void)
{
    cy_time_t cur_time;
    int i;

    cy_rtos_get_time(&cur_time);

    for (i = 0; i < ARP_TABLE_LIST_NUM; i ++)
    {
        if (g_arp_rq[i].time && cur_time - g_arp_rq[i].time > SESSION_TIMEOUT) {
            PRINT_HM_DEBUG(("[%s:%d] Clear ARP request dipaddr:%s\n", __func__, __LINE__, ip4addr_ntoa(&g_arp_rq[i].dipaddr)));
            whd_mem_memset(&g_arp_rq[i], 0x0, sizeof(arp_request_t));
        }
    }

}

int sdio_arb_query_arp_request(struct etharp_hdr *arp)
{
    int i;
    bool is_find = false;

    PRINT_HM_DEBUG(("[%s:%d] arp opcode:%d.\n", __func__, __LINE__,  hton16(arp->opcode)));
    PRINT_HM_DEBUG(("arp->shwaddr:%02x:%02x:%02x:%02x:%02x:%02x\n",
        arp->shwaddr.addr[0],arp->shwaddr.addr[1],arp->shwaddr.addr[2],arp->shwaddr.addr[3],arp->shwaddr.addr[4],arp->shwaddr.addr[5]));
    PRINT_HM_DEBUG(("arp->sipaddr:%s\n", ip4addr_ntoa((ip4_addr_t *)&arp->sipaddr.addrw)));
    PRINT_HM_DEBUG(("arp->dhwaddr:%02x:%02x:%02x:%02x:%02x:%02x\n",
        arp->dhwaddr.addr[0],arp->dhwaddr.addr[1],arp->dhwaddr.addr[2],arp->dhwaddr.addr[3],arp->dhwaddr.addr[4],arp->dhwaddr.addr[5]));
    PRINT_HM_DEBUG(("arp->dipaddr:%s\n", ip4addr_ntoa((ip4_addr_t *)&arp->dipaddr.addrw)));
    sdio_arb_mutex_lock();
    if (hton16(arp->opcode) == ARP_REQUEST) {
        for (i = 0; i < ARP_TABLE_LIST_NUM; i ++) {
            if ((g_arp_rq[i].time) &&
                !memcmp(&g_arp_rq[i].shwaddr, (uint8_t *)&arp->shwaddr.addr, MAC_ADDR_LEN) &&
                ip4_addr_cmp(&g_arp_rq[i].sipaddr, (ip4_addr_t *)&arp->sipaddr.addrw) &&
                !memcmp(&g_arp_rq[i].dhwaddr, (uint8_t *)&arp->dhwaddr.addr, MAC_ADDR_LEN) &&
                ip4_addr_cmp(&g_arp_rq[i].dipaddr, (ip4_addr_t *)&arp->dipaddr.addrw)) {
                is_find = true;
                break;
            }
        }
    } else if (hton16(arp->opcode) == ARP_REPLY) {

        for (i = 0; i < ARP_TABLE_LIST_NUM; i ++) {
            if (g_arp_rq[i].time &&
                ip4_addr_cmp(&g_arp_rq[i].dipaddr, (ip4_addr_t *)&arp->sipaddr.addrw) &&
                ip4_addr_cmp(&g_arp_rq[i].sipaddr, (ip4_addr_t *)&arp->dipaddr.addrw) &&
                !memcmp(&g_arp_rq[i].shwaddr, (uint8_t *)&arp->dhwaddr.addr, MAC_ADDR_LEN)) {
                is_find = true;
                /* arp have replay, del it */
                PRINT_HM_DEBUG(("[%s:%d] Find arp quest: dipaddr:%s opcode:%d\n", __func__, __LINE__, ip4addr_ntoa(&g_arp_rq[i].dipaddr), hton16(arp->opcode)));
                whd_mem_memset(&g_arp_rq[i], 0x0, sizeof(arp_request_t));
                break;
            }
        }

#ifndef SDIO_HM_TRACK_SESSION
        /* Clear the timedout ARP entries, if the ARP arb table entries reached max
           if SDIO_HM_TRACK_SESSION is defined, then arb timer take care of clear the table */
        if (i == ARP_TABLE_LIST_NUM) {
            sdio_arb_update_arp_table();
        }
#endif /* SDIO_HM_TRACK_SESSION */

    }
    sdio_arb_mutex_unlock();

    return is_find;
}

int sdio_arb_add_arp_request(struct etharp_hdr *arp)
{
    int i = 0;
    bool is_add = false;
    cy_time_t cur_time;

    if (hton16(arp->opcode) != ARP_REQUEST) {
        return -1;
    }
    if (sdio_arb_query_arp_request(arp)) {
        return 0;
    }
    sdio_arb_mutex_lock();

    for (i = 0; i < ARP_TABLE_LIST_NUM; i ++) {
        if (g_arp_rq[i].time == 0) {
            is_add = true;
            break;
        }
    }
    if (is_add) {
        whd_mem_memcpy(&g_arp_rq[i].shwaddr, (uint8_t *)&arp->shwaddr, MAC_ADDR_LEN);
        ip4_addr_set(&g_arp_rq[i].sipaddr, (ip4_addr_t *)arp->sipaddr.addrw);
        whd_mem_memcpy(&g_arp_rq[i].dhwaddr, (uint8_t *)&arp->dhwaddr, MAC_ADDR_LEN);
        ip4_addr_set(&g_arp_rq[i].dipaddr, (ip4_addr_t *)arp->dipaddr.addrw);
        cy_rtos_get_time(&cur_time);
        g_arp_rq[i].time = cur_time;
        PRINT_HM_DEBUG(("[%s:%d] Add ARP request[%d] target ip:%s time:%u\n", __func__, __LINE__, i, ip4addr_ntoa(&g_arp_rq[i].dipaddr),
            g_arp_rq[i].time));
    } else {
        PRINT_HM_INFO(("[%s:%d] ARP request list is full\n", __func__, __LINE__));
    }
    sdio_arb_mutex_unlock();
    return 0;
}

void sdio_arb_network_init()
{
    if (sdio_arb_inited) {
        PRINT_HM_INFO(("WARING: SNET have inited\n"));
        return ;
    }
    sdio_arb_mutex_init();
#ifdef SDIO_HM_TRACK_SESSION
    sdio_arb_network_timer_init();
#endif
    sdio_arb_inited = 1;
}

void sdio_arb_network_deinit()
{
    if (sdio_arb_inited) {
#ifdef SDIO_HM_TRACK_SESSION
        sdio_arb_network_timer_stop();
        cy_rtos_deinit_timer(&sdio_arb_timer);
#endif
        sdio_arb_mutex_deinit();
    }
    sdio_arb_inited = 0;
}

#ifdef SDIO_HM_TRACK_SESSION
static void *sdio_arb_timer_callback(void* arg)
{
    ip_session_t *ip_ses, *pre_ip_ses;
    cy_time_t cur_time;
    bool del_session;

    cy_rtos_get_time(&cur_time);

    sdio_arb_mutex_lock();
    ip_ses = pre_ip_ses = g_ip_session;

    while (ip_ses) {
        del_session = 0;
        if (cur_time - ip_ses->time > SESSION_TIMEOUT) {

            del_session = 1;
        }
        if (del_session) {
            if (g_ip_session == ip_ses) {
                g_ip_session = ip_ses->next;
                pre_ip_ses = g_ip_session;
                free(ip_ses);
                ip_ses = g_ip_session;
            } else {
                pre_ip_ses->next = ip_ses->next;
                free(ip_ses);
                ip_ses = pre_ip_ses->next;
            }

            ip_session_number -= 1;
        } else {
            pre_ip_ses = ip_ses;
            ip_ses = ip_ses->next;
        }
    }
    sdio_arb_update_arp_table();
    sdio_arb_mutex_unlock();

    return NULL;
}

cy_rslt_t sdio_arb_network_timer_init()
{
    return cy_rtos_init_timer(&sdio_arb_timer, CY_TIMER_TYPE_PERIODIC,
                        (cy_timer_callback_t)sdio_arb_timer_callback, (cy_timer_callback_arg_t)NULL);
}

void sdio_arb_network_timer_start()
{
    bool arb_timer_state;
    (void)cy_rtos_is_running_timer(&sdio_arb_timer, &arb_timer_state);

    if(!arb_timer_state)
        cy_rtos_start_timer(&sdio_arb_timer, 5000);
}

void sdio_arb_network_timer_stop()
{
    bool arb_timer_running;
    (void)cy_rtos_is_running_timer(&sdio_arb_timer, &arb_timer_running);

    if(arb_timer_running)
        cy_rtos_stop_timer(&sdio_arb_timer);
}
#endif /* SDIO_HM_TRACK_SESSION */

static int is_in_whitelist_port(uint16_t port)
{
    int i;
    if (g_whitelist_port_num == 0) {
        return 0;
    }
    for (i = 0; i < g_whitelist_port_num; i ++) {
        if (g_whitelist_port[i] == port) {
            return 1;
        }
    }
    return 0;
}

int sdio_arb_add_whitelist_port(uint16_t port)
{
    int i = 0;
    if (g_whitelist_port_num >= WHITE_LIST_PORT_MAX) {
        PRINT_HM_ERROR(("Add failed, up to %d can be set.\n", WHITE_LIST_PORT_MAX));
        return -1;
    }
    for (i = 0; i < g_whitelist_port_num; i ++) {
        if (g_whitelist_port[i] == port) {
            PRINT_HM_INFO(("Port %d has add, return\n", port));
            return 0;
        }
    }
    g_whitelist_port[g_whitelist_port_num++] = port;
    PRINT_HM_INFO(("Added white list port %d for Hosted Mode, list number:%d\n", port, g_whitelist_port_num));
    return 0;
}

/* if port == 0, clear all white port */
int sdio_arb_del_whitelist_port(uint16_t port)
{
    int i = 0;
    if (port == 0) {
        PRINT_HM_INFO(("Clear White list port\n"));
        whd_mem_memset(g_whitelist_port, 0, sizeof(g_whitelist_port));
        g_whitelist_port_num = 0;
        return 0;
    }
    for (i = 0; i < g_whitelist_port_num; i ++) {
        if (g_whitelist_port[i] == port) {
            if (i != g_whitelist_port_num - 1) {
                g_whitelist_port[i] = g_whitelist_port[g_whitelist_port_num - 1];
                g_whitelist_port[g_whitelist_port_num - 1] = 0;
            } else {
                g_whitelist_port[i] = 0;
            }
            g_whitelist_port_num -= 1;
            PRINT_HM_INFO(("Del white list port %d, list number:%d\n", port, g_whitelist_port_num));
            return 0;
        }
    }
    PRINT_HM_INFO(("Not found port %d in port white list.\n", port));
    return -1;
}

int sdio_arb_get_whitelist_port_num(void)
{
    return g_whitelist_port_num;
}

uint16_t sdio_arb_get_whitelist_port(int index)
{
    if (index >= g_whitelist_port_num) {
        return 0;
    }
    return g_whitelist_port[index];
}

// return 0 send to bus, other drop it.
int sdio_arb_eth_packet_send_handle(uint8_t *eth_packet, int *packet_len)
{
    uint8_t *pdata;
    struct eth_hdr *ethhdr;
    struct ip_hdr  *iphdr;

    struct udp_hdr *udphdr;
    struct tcp_hdr *tcphdr;
    //struct icmp_echo_hdr *icmphdr;

    struct etharp_hdr *arphdr;
    ip_addr_t local_ip, remote_ip;
    int result = -1;

    uint16_t ether_type;
    bool is_frag_more = 0;
    bool is_unsupport = false;
    uint16_t sport = 0, dport = 0, icmp_hash = 0;
    uint8_t tos;

    pdata = eth_packet;
    ethhdr = (struct eth_hdr *)pdata;

    ether_type = ntoh16(ethhdr->type);
    *packet_len = ETHERNET_HEADER_LEN;
    PRINT_HM_DEBUG(("%s Enter...\n", __func__));

    if (!sdio_arb_inited) {
        PRINT_HM_ERROR(("SNET Module not initialized, error.\n"));
        return -1;
    }
    PRINT_HM_DEBUG(("%s SRC Eth:%02x:%02x:%02x:%02x:%02x:%02x ", __func__, ethhdr->src.addr[0],ethhdr->src.addr[1],ethhdr->src.addr[2],ethhdr->src.addr[3],ethhdr->src.addr[4],ethhdr->src.addr[5]));
    PRINT_HM_DEBUG(("%s DEST Eth:%02x:%02x:%02x:%02x:%02x:%02x\n", __func__,ethhdr->dest.addr[0],ethhdr->dest.addr[1],ethhdr->dest.addr[2],ethhdr->dest.addr[3],ethhdr->dest.addr[4],ethhdr->dest.addr[5]));
    PRINT_HM_DEBUG(("%s ether_type:0x%x.\n", __func__, ether_type));

    if (ether_type == ETHTYPE_IP) {
        //struct icmp_echo_hdr *icmphdr;
        iphdr = (struct ip_hdr *)(pdata + ETHERNET_HEADER_LEN);
        is_frag_more = (IPH_OFFSET(iphdr) & PP_NTOHS(IP_MF)) != 0;
        tos = IPH_PROTO(iphdr);
        *packet_len += hton16(IPH_LEN(iphdr));

        ip4_addr_set(&local_ip.u_addr.ip4, (ip4_addr_t *)&iphdr->src.addr);
        IP_SET_TYPE(&local_ip, IPADDR_TYPE_V4);
        ip4_addr_set(&remote_ip.u_addr.ip4, (ip4_addr_t *)&iphdr->dest.addr);
        IP_SET_TYPE(&remote_ip, IPADDR_TYPE_V4);
        PRINT_HM_DEBUG(("[%s:%d] IPv4 request Remote IP:%s PROTO:%d\n", __func__, __LINE__, ip4addr_ntoa((const ip4_addr_t *) &iphdr->dest.addr), tos));
        PRINT_HM_DEBUG(("ip option more fragment:%d.\n", is_frag_more));
        if (IPH_PROTO(iphdr) == IP_PROTO_UDP) {
            udphdr = (struct udp_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);

            if (is_frag_more == 0)
            {
                sport = hton16(udphdr->src);
                dport = hton16(udphdr->dest);
            }
        } else if(IPH_PROTO(iphdr) == IP_PROTO_ICMP) {
            //icmphdr = (struct icmp_echo_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);
            sport = dport = 0;
            icmp_hash = 0x99;
            //PRINT_HM_DEBUG(("icmp request seq number:%d\n", hton16(icmphdr->seqno)));
        } else if (IPH_PROTO(iphdr) == IP_PROTO_TCP) {
            tcphdr = (struct tcp_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);

            sport = hton16(tcphdr->src);
            dport = hton16(tcphdr->dest);
        } else {
            PRINT_HM_DEBUG(("[%s:%d] unsupport ipv4 protocol:%x\n", __func__, __LINE__, tos));
            is_unsupport = true;
        }
    }
    else if (ether_type == ETHTYPE_ARP)
    {
        arphdr = (struct etharp_hdr *)(pdata + ETHERNET_HEADER_LEN);
        *packet_len += ARP_PACKET_SIZE;
        sdio_arb_add_arp_request(arphdr);
        result = 0;
        is_unsupport = true;
    } else {
        PRINT_HM_DEBUG(("[%s:%d] unsupport ether type:%x\n", __func__, __LINE__, ether_type));
        is_unsupport = true;
    }

    if (!is_unsupport) {
        result = sdio_arb_add_ip_session(&local_ip, &remote_ip, sport, dport, icmp_hash, tos);
    }
    PRINT_HM_DEBUG(("%s Exit...\n", __func__));

    return result;
}

// return 1 - transfer to device, 0 -transfer to host
bool sdio_arb_eth_packet_recv_handle(uint8_t *eth_packet)
{
    uint8_t *pdata;
    struct eth_hdr *ethhdr;
    struct ip_hdr *iphdr;
    struct udp_hdr *udphdr;
    struct tcp_hdr *tcphdr;
    struct etharp_hdr *arphdr;
    //struct icmp_echo_hdr *icmphdr;
    uint16_t ether_type;

    uint16_t sport = 0, dport = 0, icmp_hash = 0;
    uint8_t tos;
    ip_addr_t local_ip, remote_ip;

    bool is_frag_more = 0;
    bool is_find_session = 0;

    pdata  = eth_packet;
    ethhdr = (struct eth_hdr *)pdata;
    ether_type = ntoh16(ethhdr->type);
    PRINT_HM_DEBUG(("%s Enter...\n", __func__));

    if (!sdio_arb_inited) {
        PRINT_HM_ERROR(("SNET Module not initialized, error.\n"));
        return -1;
    }

    if (is_eth_multicast((uint8_t *)&ethhdr->dest.addr))
    {
        return 1;
    }
    if (ether_type == ETHTYPE_IP)
    {
        iphdr = (struct ip_hdr *)(pdata + ETHERNET_HEADER_LEN);
        tos = IPH_PROTO(iphdr);
        is_frag_more = (IPH_OFFSET(iphdr) & PP_NTOHS(IP_MF)) != 0;

        ip4_addr_set(&local_ip.u_addr.ip4, (ip4_addr_t *)&iphdr->src.addr);
        IP_SET_TYPE(&local_ip, IPADDR_TYPE_V4);
        ip4_addr_set(&remote_ip.u_addr.ip4, (ip4_addr_t *)&iphdr->dest.addr);
        IP_SET_TYPE(&remote_ip, IPADDR_TYPE_V4);

        if (IPH_PROTO(iphdr) == IP_PROTO_UDP)
        {
            udphdr = (struct udp_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);
            if (is_frag_more == 0)
            {
                sport = hton16(udphdr->src);
                dport = hton16(udphdr->dest);
            }
        }
        else if (IPH_PROTO(iphdr) == IP_PROTO_TCP)
        {
            tcphdr = (struct tcp_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);
            sport = hton16(tcphdr->src);
            dport = hton16(tcphdr->dest);
        }
        else if (IPH_PROTO(iphdr) == IP_PROTO_ICMP)
        {
            //icmphdr = (struct icmp_echo_hdr *)(pdata + ETHERNET_HEADER_LEN + IPV4_HEADER_FIXED_LEN);
            sport = dport = 0;
            icmp_hash = 0x99;
            //PRINT_HM_DEBUG(("icmp reply seq number:%d\n", hton16(icmphdr->seqno)));
        }

        PRINT_HM_DEBUG(("[%s:%d] recieve IPv4 packet Local IP:%s ", __func__, __LINE__, ip4addr_ntoa((const ip4_addr_t *) &iphdr->src.addr)));
        PRINT_HM_DEBUG(("Remote IP:%s.\n", ip4addr_ntoa((const ip4_addr_t *) &iphdr->dest.addr)));
        PRINT_HM_DEBUG(("Source port %d, Dest Port %d, tos:%d is_frag_more:%d\n", sport, dport, tos, is_frag_more));
        if (dport && is_in_whitelist_port(dport)) {
            is_find_session = 1;
            PRINT_HM_DEBUG(("Dest port %d is in port white list, transfer the data\n"));
        } else {
            is_find_session = sdio_arb_query_ip_session(&remote_ip, &local_ip, dport, sport, icmp_hash, tos, 1);
        }
    }
    else if (ether_type == ETHTYPE_ARP)
    {
        arphdr = (struct etharp_hdr *)(pdata + ETHERNET_HEADER_LEN);
        if (hton16(arphdr->opcode) == ARP_REPLY)
        {
            is_find_session = sdio_arb_query_arp_request(arphdr);
        }
    }
    else
    {
        PRINT_HM_INFO(("Unsupport ether type:0x%x\n", ether_type));
    }
    PRINT_HM_DEBUG(("%s Exit...\n", __func__));

    if (is_find_session) {
        return 0;
    }
    return 1;
}

#endif /* COMPONENT_SDIO_HM */
