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

#ifndef INCLUDED_WHD_TRXHDR_H_
#define INCLUDED_WHD_TRXHDR_H_

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
*			TRX header Constants
******************************************************/

#define TRX_MAGIC       0x30524448  /* "HDR0" */
#define TRXV4_SIZE             692
#define TRXV5_SIZE              32

/******************************************************
*			Structures
******************************************************/
typedef struct
{
    uint32_t magic;                     /* "HDR0" */
    uint32_t len;                       /* Length of file including header */
    uint32_t crc32;                     /* CRC from flag_version to end of file */
    uint32_t flag_version;              /* 0:15 flags, 16:31 version */
    uint32_t offsets[4];                /* Offsets of partitions */
} trx_header_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef INCLUDED_WHD_TRXHDR_H_ */
