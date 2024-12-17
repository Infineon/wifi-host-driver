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
 * @file WHD utilities
 *
 * Utilities to help do specialized (not general purpose) WHD specific things
 */
#include "whd_chip.h"
#include "whd_events_int.h"
#include "whd_types_int.h"

#ifdef PROTO_MSGBUF
#include "whd_hw.h"
#endif

#ifndef INCLUDED_WHD_UTILS_H_
#define INCLUDED_WHD_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define  NBBY  8
#define  setbit(a, i)   ( ( (uint8_t *)a )[(int)(i) / (int)(NBBY)] |= (uint8_t)(1 << ( (i) % NBBY ) ) )
#define  clrbit(a, i)   ( ( (uint8_t *)a )[(int)(i) / (int)(NBBY)] &= (uint8_t) ~(1 << ( (i) % NBBY ) ) )
#define  isset(a, i)    ( ( (const uint8_t *)a )[(int)(i) / (int)(NBBY)]& (1 << ( (i) % NBBY ) ) )
#define  isclr(a, i)    ( ( ( (const uint8_t *)a )[(int)(i) / (int)(NBBY)]& (1 << ( (i) % NBBY ) ) ) == 0 )

#define  CEIL(x, y)     ( ( (x) + ( (y) - 1 ) ) / (y) )
#define  ROUNDUP(x, y)      ( ( ( (x) + ( (y) - 1 ) ) / (y) ) * (y) )
#define  ROUNDDN(p, align)  ( (p)& ~( (align) - 1 ) )

/**
 * Get the offset (in bytes) of a member within a structure
 */
#define OFFSET(type, member)                          ( (uint32_t)&( (type *)0 )->member )

/**
 * determine size (number of elements) in an array
 */
#define ARRAY_SIZE(a)                                 (sizeof(a) / sizeof(a[0]) )

#ifdef PROTO_MSGBUF
uint32_t whd_dmapool_init(uint32_t memory_size);
void* whd_dmapool_alloc( int size);
#endif

/** Searches for a specific WiFi Information Element in a byte array
 *
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 *
 * @note : This function has been copied directly from the standard Broadcom host driver file wl/exe/wlu.c
 *
 *
 * @param tlv_buf : The byte array containing the Information Elements (IEs)
 * @param buflen  : The length of the tlv_buf byte array
 * @param key     : The Information Element tag to search for
 *
 * @return    NULL : if no matching Information Element was found
 *            Non-Null : Pointer to the start of the matching Information Element
 */

whd_tlv8_header_t *whd_parse_tlvs(const whd_tlv8_header_t *tlv_buf, uint32_t buflen, dot11_ie_id_t key);

/** Checks if a WiFi Information Element is a WPA entry
 *
 * Is this body of this tlvs entry a WPA entry? If
 * not update the tlvs buffer pointer/length
 *
 * @note : This function has been copied directly from the standard Broadcom host driver file wl/exe/wlu.c
 *
 * @param wpaie    : The byte array containing the Information Element (IE)
 * @param tlvs     : The larger IE array to be updated if not a WPA IE
 * @param tlvs_len : The current length of larger IE array
 *
 * @return    WHD_TRUE  : if IE matches the WPA OUI (Organizationally Unique Identifier) and its type = 1
 *            WHD_FALSE : otherwise
 */
whd_bool_t whd_is_wpa_ie(vendor_specific_ie_header_t *wpaie, whd_tlv8_header_t **tlvs, uint32_t *tlvs_len);

/** Searches for a specific WiFi Information Element in a byte array
 *
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 *
 * @note : This function has been copied directly from the standard Broadcom host driver file wl/exe/wlu.c
 *
 *
 * @param tlv_buf : The byte array containing the Information Elements (IEs)
 * @param buflen  : The length of the tlv_buf byte array
 * @param key     : The Information Element tag to search for
 *
 * @return    NULL : if no matching Information Element was found
 *            Non-Null : Pointer to the start of the matching Information Element
 */
whd_tlv8_header_t *whd_parse_dot11_tlvs(const whd_tlv8_header_t *tlv_buf, uint32_t buflen, dot11_ie_id_t key);

/******************************************************
*             Debug helper functionality
******************************************************/
#ifdef WPRINT_ENABLE_WHD_DEBUG
const char *whd_event_to_string(whd_event_num_t var);
char *whd_ssid_to_string(uint8_t *value, uint8_t length, char *ssid_buf, uint8_t ssid_buf_len);
const char *whd_status_to_string(whd_event_status_t status);
const char *whd_reason_to_string(whd_event_reason_t reason);
char *whd_ether_ntoa(const uint8_t *ea, char *buf, uint8_t buf_len);
const char *whd_ioctl_to_string(uint32_t ioctl);
#endif /* ifdef WPRINT_ENABLE_WHD_DEBUG */

/**
 ******************************************************************************
 * Prints partial details of a scan result on a single line
 * @param[in] record :  A pointer to the whd_scan_result_t record
 *
 */
extern void whd_print_scan_result(whd_scan_result_t *record);

/**
 ******************************************************************************
 * Convert a security bitmap to string
 * @param[in] security :  security of type whd_security_t
 * @param[in] out_str :  a character array to store output
 * @param[in] out_str_len :  length of out_str char array
 *
 */
extern void whd_convert_security_type_to_string(whd_security_t security, char *out_str, uint16_t out_str_len);

/*!
 ******************************************************************************
 * Convert an IOCTL to a string.
 *
 * @param[in] cmd  The ioct_log value.
 * @param[out] ioctl_str The string value after conversion.
 * @param[out] ioctl_str_len The string length of the IOCTL string.
 *
 * @result
 */
void whd_ioctl_info_to_string(uint32_t cmd, char *ioctl_str, uint16_t ioctl_str_len);

/*!
 ******************************************************************************
 * Convert event, status and reason value coming from firmware to string.
 *
 * @param[in] cmd  The event value in numeric form.
 * @param[in] flag  The status value in numeric form.
 * @param[in] reason  The reson value in numeric form.
 * @param[out] ioctl_str  The string representation of event, status and reason.
 * @param[out] ioctl_str_len  The str_len of ioctl_str.
 *
 * @result
 */
void whd_event_info_to_string(uint32_t cmd, uint16_t flag, uint32_t reason, char *ioctl_str, uint16_t ioctl_str_len);

/*!
 ******************************************************************************
 * Prints Hexdump and ASCII dump for data passed as argument.
 *
 * @param[in] data  The data which has to be converted into hex and ascii format.
 * @param[in] data_len The length of data.
 *
 * @result
 */
void whd_hexdump(uint8_t *data, uint32_t data_len);

extern wl_chanspec_t whd_channel_to_wl_band(whd_driver_t whd_driver, uint32_t channel);

/*!
 ******************************************************************************
 * Convert an ipv4 string to a uint32_t.
 *
 * @param[in] ip4addr   : IP address in string format
 * @param[in] len       : length of the ip address string
 * @param[out] dest     : IP address format in uint32
 *
 * @return
 */
bool whd_str_to_ip(const char *ip4addr, size_t len, void *dest);

/*!
 ******************************************************************************
 * Print binary IPv4 address to a string.
 * String must contain enough room for full address, 16 bytes exact.
 * @param[in] ip4addr     : IPv4 address
 * @param[out] p          : ipv4 address in string format
 *
 * @return
 */
uint8_t whd_ip4_to_string(const void *ip4addr, char *p);


/*!
 ******************************************************************************
 * The wrapper function for memory allocation.
 * It allocates the requested memory and returns a pointer to it.
 * In default implementation it uses The C library function malloc().
 *
 * Use macro WHD_USE_CUSTOM_MALLOC_IMPL (-D) for custom whd_mem_malloc/
 * whd_mem_calloc/whd_mem_free inplemetation.
 *
 * @param[in] size     :  This is the size of the memory block, in bytes.
 *
 * @return
 *  This function returns a pointer to the allocated memory, or NULL if the
 *  request fails.
 */
void *whd_mem_malloc(size_t size);

/*!
 ******************************************************************************
 * The wrapper function for memory allocation.
 * It allocates the requested memory and sets allocated memory to zero.
 * In default implementation it uses The C library function calloc().
 *
 * Use macro WHD_USE_CUSTOM_MALLOC_IMPL (-D) for custom whd_mem_malloc/
 * whd_mem_calloc/whd_mem_free inplemetation.
 *
 * @param[in] nitems   :  This is the number of elements to be allocated.
 * @param[in] size     :  This is the size of elements.
 *
 * @return
 *  This function returns a pointer to the allocated memory, or NULL if the
 *  request fails.
 */
void *whd_mem_calloc(size_t nitems, size_t size);

/*!
 ******************************************************************************
 * The wrapper function for free allocated memory.
 * In default implementation it uses The C library function free().
 *
 * Use macro WHD_USE_CUSTOM_MALLOC_IMPL (-D) for custom whd_mem_malloc/
 * whd_mem_calloc/whd_mem_free inplemetation.
 *
 * @param[in] ptr     :  pointer to a memory block previously allocated
 *                       with whd_mem_malloc, whd_mem_calloc
 * @return
 */
void whd_mem_free(void *ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
