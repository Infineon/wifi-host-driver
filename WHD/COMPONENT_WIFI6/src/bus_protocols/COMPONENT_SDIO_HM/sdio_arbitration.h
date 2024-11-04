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

#ifndef _SDIO_ARBITRATION_H_
#define _SDIO_ARBITRATION_H_

#include "cy_pdl.h"
#include "cyhal.h"

#define ETH_HWADDR_LEN    6
#define ETH_PAD_SIZE      0
/*
 * A list of often ethtypes (although lwIP does not use all of them).
 */
enum lwip_ieee_eth_type {
    /** Internet protocol v4 */
    ETHTYPE_IP        = 0x0800U,
    /** Address resolution protocol */
    ETHTYPE_ARP       = 0x0806U,
    /** Wake on lan */
    ETHTYPE_WOL       = 0x0842U,
    /** RARP */
    ETHTYPE_RARP      = 0x8035U,
    /** Virtual local area network */
    ETHTYPE_VLAN      = 0x8100U,
    /** Internet protocol v6 */
    ETHTYPE_IPV6      = 0x86DDU,
    /** PPP Over Ethernet Discovery Stage */
    ETHTYPE_PPPOEDISC = 0x8863U,
    /** PPP Over Ethernet Session Stage */
    ETHTYPE_PPPOE     = 0x8864U,
    /** Jumbo Frames */
    ETHTYPE_JUMBO     = 0x8870U,
    /** Process field network */
    ETHTYPE_PROFINET  = 0x8892U,
    /** Ethernet for control automation technology */
    ETHTYPE_ETHERCAT  = 0x88A4U,
    /** Link layer discovery protocol */
    ETHTYPE_LLDP      = 0x88CCU,
    /** Serial real-time communication system */
    ETHTYPE_SERCOS    = 0x88CDU,
    /** Media redundancy protocol */
    ETHTYPE_MRP       = 0x88E3U,
    /** Precision time protocol */
    ETHTYPE_PTP       = 0x88F7U,
    /** Q-in-Q, 802.1ad */
    ETHTYPE_QINQ      = 0x9100U
};

/** @ingroup ipaddr
 * IP address types for use in ip_addr_t.type member.
 * @see tcp_new_ip_type(), udp_new_ip_type(), raw_new_ip_type().
 */
enum lwip_ip_addr_type {
    /** IPv4 */
    IPADDR_TYPE_V4 =   0U,
    /** IPv6 */
    IPADDR_TYPE_V6 =   6U,
    /** IPv4+IPv6 ("dual-stack") */
    IPADDR_TYPE_ANY = 46U
};

/** This is the aligned version of ip4_addr_t,
 *  used as local variable, on the stack, etc.
 */
struct ip4_addr {
    uint32_t addr;
};

/** ip4_addr_t uses a struct for convenience only, so that the same defines can
 *  operate both on ip4_addr_t as well as on ip4_addr_p_t.
 */
typedef struct ip4_addr ip4_addr_t;

typedef struct ip_addr {
    union {
    ip4_addr_t ip4;
    } u_addr;
    /** @ref lwip_ip_addr_type */
    uint8_t type;
} ip_addr_t;

#define ip4_addr_get_u32(src_ipaddr) ((src_ipaddr)->addr)

#define PP_NTOHS(x)   ((uint16_t)(x))
#define ip4_addr_set(dest, src) ((dest)->addr = \
        ((src) == NULL ? 0 : \
        (src)->addr))

/** An Ethernet MAC address */
struct eth_addr {
    uint8_t addr[ETH_HWADDR_LEN];
};

/** Ethernet header */
struct eth_hdr {
#if ETH_PAD_SIZE
    uint8_t padding[ETH_PAD_SIZE];
#endif
    struct eth_addr dest;
    struct eth_addr src;
    uint16_t type;
};

struct ip4_addr_wordaligned {
    uint16_t addrw[2];
};

/** the ARP message, see RFC 826 ("Packet format") */
typedef struct etharp_hdr {
    uint16_t hwtype;
    uint16_t proto;
    uint8_t  hwlen;
    uint8_t  protolen;
    uint16_t opcode;
    struct eth_addr shwaddr;
    struct ip4_addr_wordaligned sipaddr;
    struct eth_addr dhwaddr;
    struct ip4_addr_wordaligned dipaddr;
} etharp_hdr_t;

/* ARP message types (opcodes) */
enum etharp_opcode {
    ARP_REQUEST = 1,
    ARP_REPLY   = 2
};

#define NUM_BUFFERS_MQTT			(2)
#define ETHERNET_HEADER_LEN			(14)
#define IPV4_HEADER_FIXED_LEN		(20)
#define TCP_HEADER_FIXED_LEN		(20)

struct ip_hdr {
    uint8_t    _v_hl;
    uint8_t   _tos;
    uint16_t  _len;
    uint16_t  _id;
    uint16_t  _offset;
#define IP_RF 0x8000U        /* reserved fragment flag */
#define IP_DF 0x4000U        /* don't fragment flag */
#define IP_MF 0x2000U        /* more fragments flag */
#define IP_OFFMASK 0x1fffU   /* mask for fragmenting bits */
    uint8_t   _ttl;
    uint8_t   _proto;
    uint16_t  _chksum;
    ip4_addr_t src;
    ip4_addr_t dest;
};

struct udp_hdr {
    uint16_t src;
    uint16_t dest;  /* src/dest UDP ports */
    uint16_t len;
    uint16_t chksum;
};

struct tcp_hdr {
    uint16_t src;
    uint16_t dest;
    uint32_t seqno;
    uint32_t ackno;
    uint16_t _hdrlen_rsvd_flags;
    uint16_t wnd;
    uint16_t chksum;
    uint16_t urgp;
} ;

struct icmp_echo_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t chksum;
    uint16_t id;
    uint16_t seqno;
} ;

#define IPH_V(hdr)  ((hdr)->_v_hl >> 4)
#define IPH_HL(hdr) ((hdr)->_v_hl & 0x0f)
#define IPH_HL_BYTES(hdr) ((u8_t)(IPH_HL(hdr) * 4))
#define IPH_TOS(hdr) ((hdr)->_tos)
#define IPH_LEN(hdr) ((hdr)->_len)
#define IPH_ID(hdr) ((hdr)->_id)
#define IPH_OFFSET(hdr) ((hdr)->_offset)
#define IPH_OFFSET_BYTES(hdr) ((u16_t)((hton16(IPH_OFFSET(hdr)) & IP_OFFMASK) * 8U))
#define IPH_TTL(hdr) ((hdr)->_ttl)
#define IPH_PROTO(hdr) ((hdr)->_proto)
#define IPH_CHKSUM(hdr) ((hdr)->_chksum)

#define IP_PROTO_ICMP    1
#define IP_PROTO_IGMP    2
#define IP_PROTO_UDP     17
#define IP_PROTO_UDPLITE 136
#define IP_PROTO_TCP     6

#define ip4_addr_cmp(addr1, addr2) ((addr1)->addr == (addr2)->addr)

#define IP_SET_TYPE_VAL(ipaddr, iptype) do { (ipaddr).type = (iptype); }while(0)
#define IP_SET_TYPE(ipaddr, iptype)     do { if((ipaddr) != NULL) { IP_SET_TYPE_VAL(*(ipaddr), iptype); }}while(0)
#define IP_GET_TYPE(ipaddr)             ((ipaddr)->type)

/**
 * Init, malloc resoures.
 *
 * @return None
 */
void sdio_arb_network_init();

/**
 * Deinit, free resoures.
 *
 * @return None.
 */
void sdio_arb_network_deinit();

#ifdef SDIO_HM_TRACK_SESSION
/**
 * Initialize arb timer for tracking ip sessions.
 *
 * @return None
 */
void sdio_arb_network_timer_start();
/**
 * De-initialize arb timer for tracking ip sessions.
 *
 * @return None
 */
void sdio_arb_network_timer_stop();
#endif /* SDIO_HM_TRACK_SESSION */

/**
 * Process received packets from WiFi chip and determine where the packet is forwarded.
 *
 * @param[in]   eth_packet  : the pointer address of the ethernet packet buffer
 *
 * @return 0 - forward to host side; 1 - forward to local network.
 */
bool sdio_arb_eth_packet_recv_handle(uint8_t *eth_packet);

/**
 * Process received packets from Host side and determine whether the packet is forwarded.
 *
 * @param[in]   eth_packet  : Pointer address of the ethernet packet buffer
 * @param[out]  packet_len  : the length of the packet.
 *
 * @return 0  if the packet can forward to whd. failure code otherwise.
 */
int sdio_arb_eth_packet_send_handle(uint8_t *eth_packet, int *packet_len);

/**
 * Add the whitelist port. If the TCP/UDP session destination port is in the whitelist port,
 * the session data will be transmitted to Host. Up to 16 can be set.
 *
 * @param[in]   port  : the port need to add whitelist
 *
 * @return 0 success. failure code otherwise.
 */
int sdio_arb_add_whitelist_port(uint16_t port);

/**
 * Del the whitelist port. If the port is 0, will clear
 * all whitelist port.
 *
 * @param[in]   port  : the port need to del whitelist
 *
 * @return 0 success. failure code otherwise.
*/
int sdio_arb_del_whitelist_port(uint16_t port);

/**
 * Get current whitelist port number.
 *
 * @param[in]   None
 *
 * @return the nubmer of whitelist port.
*/
int sdio_arb_get_whitelist_port_num(void);

/**
 * Get index whitelist port.
 *
 * @param[in]   index: the index of whitelist port.
 *
 * @return the port of whitelist port[index].
*/
uint16_t sdio_arb_get_whitelist_port(int index);

#endif /* _SDIO_ARBITRATION_H_ */
