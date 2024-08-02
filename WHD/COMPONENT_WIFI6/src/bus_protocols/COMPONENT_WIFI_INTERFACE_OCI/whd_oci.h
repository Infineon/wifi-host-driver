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

#ifndef INCLUDED_WHD_OCI_H_
#define INCLUDED_WHD_OCI_H_

#include "whd_bus_oci_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
*             OCI Constants
******************************************************/
//GCI Registers
#define GCI_INT_MASK_REG            0x4fe40018    /* When the31st bit of this Reg is set, Doorbell Interrupts to the IP(BT/WLAN) are enabled */
#define GCI_INT_STATUS_REG          0x4fe40014    /* Read Only Reg - 31st bit indicates a DB Interrupt to the BT is pending.If it is set, Indicates a WL2BT Doorbell Interrupt is pending */
#define GCI_DB_INT_STATUS_REG       0x4fe4063c   /* To check which DB interrupt is triggerd by WLAN */
#define GCI_DB_INT_MASK_REG         0x4fe40640   /*  This register is writable by BT only and readable by both WLAN and BT. This register resets with BT reset */

#ifndef GCI_SECURE_ACCESS
#define GCI_DB_INT_STS_BIT          (1 << 31)      /* 31st bit of  GCI_INT_MASK_REG */
#endif

/* DB Bits for Doorbell Interrupt Mask Register(GCI_DB_INT_MASK_REG) */
#define GCI_H2D_SET_BIT_DB0         0x0001
#define GCI_H2D_SET_BIT_DB1         0x0002
#define GCI_H2D_SET_BIT_DB2         0x0004
#define GCI_H2D_SET_BIT_DB3         0x0008
#define GCI_H2D_SET_BIT_DB4         0x0010
#define GCI_H2D_SET_BIT_DB5         0x0020
#define GCI_H2D_SET_BIT_DB6         0x0040
#define GCI_H2D_SET_BIT_DB7         0x0080
#define GCI_H2D_SET_ALL_DB          (GCI_H2D_SET_BIT_DB0 | GCI_H2D_SET_BIT_DB1 | GCI_H2D_SET_BIT_DB2 | \
                                     GCI_H2D_SET_BIT_DB3 | \
                                     GCI_H2D_SET_BIT_DB4 | GCI_H2D_SET_BIT_DB5 | GCI_H2D_SET_BIT_DB6 | \
                                     GCI_H2D_SET_BIT_DB7)

/* These registers are writable by BT only and readable by both WLAN and BT. These registers reset with BT reset */
#define GCI_BT2WL_DB0_REG          0x4fe4066c
#define GCI_BT2WL_DB1_REG          0x4fe40670
#define GCI_BT2WL_DB2_REG          0x4fe40674
#define GCI_BT2WL_DB3_REG          0x4fe40678
#define GCI_BT2WL_DB4_REG          0x4fe4067c
#define GCI_BT2WL_DB5_REG          0x4fe40680
#define GCI_BT2WL_DB6_REG          0x4fe40684
#define GCI_BT2WL_DB7_REG          0x4fe40688

/* These registers are writable by WLAN only and readable by both WLAN and BT. These registers reset with WLAN reset */
#define GCI_WL2BT_DB0_REG          0x4fe40644
#define GCI_WL2BT_DB1_REG          0x4fe40648
#define GCI_WL2BT_DB2_REG          0x4fe4064c
#define GCI_WL2BT_DB3_REG          0x4fe40650
#define GCI_WL2BT_DB4_REG          0x4fe40654
#define GCI_WL2BT_DB5_REG          0x4fe40658
#define GCI_WL2BT_DB6_REG          0x4fe4065c
#define GCI_WL2BT_DB7_REG          0x4fe40660

/* These register readable by both WLAN and BT. These registers reset with WLAN reset */
#define GCI_WL2BT_CLOCK_CSR        0x4fe406A0

/* These register readable by both WLAN and BT. These registers reset with BT reset. */
#define GCI_BT2WL_CLOCK_CSR        0x4fe406A4

/* The final control value is OR of individual values from each IP's register */
#define GCI_CTSS_CLOCK_CSR         0x4fe40700

/* GCI_CHIP_CLOCK_CSR Bits */
#define GCI_FORCE_ALP           ( (uint32_t)0x01 )     /* Force ALP request to backplane */
#define GCI_FORCE_HT            ( (uint32_t)0x02 )     /* Force HT request to backplane */
#define GCI_FORCE_ILP           ( (uint32_t)0x04 )     /* Force ILP request to backplane */
#define GCI_ALP_AVAIL_REQ       ( (uint32_t)0x08 )     /* Make ALP ready (power up xtal) */
#define GCI_HT_AVAIL_REQ        ( (uint32_t)0x10 )     /* Make HT ready (power up PLL) */
#define GCI_FORCE_HW_CLKREQ_OFF ( (uint32_t)0x20 )     /* Squelch clock requests from HW */
#define GCI_ALP_AVAIL           ( (uint32_t)0x10000 )  /* Status: ALP is ready */
#define GCI_HT_AVAIL            ( (uint32_t)0x20000 )  /* Status: HT is ready */
#define GCI_BT_ON_ALP           ( (uint32_t)0x40000 )
#define GCI_BT_ON_HT            ( (uint32_t)0x80000 )

/* Hatchet1-CP(through GCI) Security Handshake Register */
#define OCI_REG_DAR_H2D_MSG_0      0x4fe40634
#define OCI_REG_DAR_D2H_MSG_0      0x4fe4062c
#define OCI_REG_DAR_SC0_MSG_0      0x4fe40628

/* Bootloader handshake flags - dongle to host */
#define OCI_BLHS_D2H_START                    (1 << 0)
#define OCI_BLHS_D2H_READY                    (1 << 1)
#define OCI_BLHS_D2H_STEADY                   (1 << 2)
#define OCI_BLHS_D2H_TRXHDR_PARSE_DONE        (1 << 3)
#define OCI_BLHS_D2H_VALDN_START              (1 << 4)
#define OCI_BLHS_D2H_VALDN_RESULT             (1 << 5)
#define OCI_BLHS_D2H_VALDN_DONE               (1 << 6)

/* Bootloader handshake flags - host to dongle */
#define OCI_BLHS_H2D_BL_INIT                   0
#define OCI_BLHS_H2D_DL_FW_START              (1 << 0)
#define OCI_BLHS_H2D_DL_FW_DONE               (1 << 1)
#define OCI_BLHS_H2D_DL_NVRAM_DONE            (1 << 2)
#define OCI_BLHS_H2D_BL_RESET_ON_ERROR        (1 << 3)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef INCLUDED_WHD_OCI_H_ */
