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

#ifndef INCLUDED_WHD_TRXHDR_H_
#define INCLUDED_WHD_TRXHDR_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TRXV5

#else
/* Bootloader makes special use of trx header "offsets" array */
enum
{
    TRX_V4_OFFS_SIGN_INFO_IDX                   = 0,
    TRX_V4_OFFS_DATA_FOR_SIGN1_IDX              = 1,
    TRX_V4_OFFS_DATA_FOR_SIGN2_IDX              = 2,
    TRX_V4_OFFS_ROOT_MODULUS_IDX                = 3,
    TRX_V4_OFFS_ROOT_EXPONENT_IDX               = 67,
    TRX_V4_OFFS_CONT_MODULUS_IDX                = 68,
    TRX_V4_OFFS_CONT_EXPONENT_IDX               = 132,
    TRX_V4_OFFS_HASH_FW_IDX                     = 133,
    TRX_V4_OFFS_FW_LEN_IDX                      = 149,
    TRX_V4_OFFS_TR_RST_IDX                      = 150,
    TRX_V4_OFFS_FW_VER_FOR_ANTIROOLBACK_IDX     = 151,
    TRX_V4_OFFS_IV_IDX                          = 152,
    TRX_V4_OFFS_NONCE_IDX                       = 160,
    TRX_V4_OFFS_SIGN_INFO2_IDX                  = 168,
    TRX_V4_OFFS_MAX_IDX
};
#endif
/******************************************************
*			TRX header Constants
******************************************************/

#define TRX_MAGIC       0x30524448  /* "HDR0" */

#ifdef TRXV5
#define TRX_VERSION   5           /* Version trxv5 */
#else
#define TRX_VERSION   4           /* Version trxv4 */
#define TRX_MAX_OFFSET  TRX_V4_OFFS_MAX_IDX         /* Max number of file offsets for trxv4 */
#endif
/******************************************************
*			Structures
******************************************************/
#ifdef TRXV5
typedef struct
{
    uint32_t magic;                     /* "HDR0" */
    uint32_t len;                       /* Length of file including header */
    uint32_t crc32;                     /* CRC from flag_version to end of file */
    uint32_t flag_version;              /* 0:15 flags, 16:31 version */
    uint32_t offsets[4];                /* Offsets of partitions */
} trx_header_t;
#else
typedef struct
{
    uint32_t magic;                     /* "HDR0" */
    uint32_t len;                       /* Length of file including header */
    uint32_t crc32;                     /* CRC from flag_version to end of file */
    uint32_t flag_version;              /* 0:15 flags, 16:31 version */
    uint32_t offsets[TRX_MAX_OFFSET];   /* Offsets of partitions */
} trx_header_t;
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef INCLUDED_WHD_TRXHDR_H_ */
