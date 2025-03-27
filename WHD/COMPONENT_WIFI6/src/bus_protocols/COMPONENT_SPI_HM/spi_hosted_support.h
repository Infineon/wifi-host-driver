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

#ifndef _SPI_HOSTED_SUPPORT_H_
#define _SPI_HOSTED_SUPPORT_H_

/* Header file includes */
#include "cyabs_rtos.h"
#include "cyhal.h"
#include "cyhal_spi.h"
#include "cyhal_gpio.h"

#include "whd_types.h"
#include "whd_utils.h"

/* Enabling Info and Error msgs by default */
#define ENABLE_SPI_HM_INFO
#define ENABLE_SPI_HM_ERROR
/* #define ENABLE_SPI_HM_DEBUG */
/* #define ENABLE_SPI_HM_HEX */

/* DEBUG related Macros */
#define PRINT_MACRO(args) do { printf args; } while (0 == 1)

#ifdef ENABLE_SPI_HM_INFO
#define PRINT_HM_INFO(args) PRINT_MACRO(args)
#else
#define PRINT_HM_INFO(args)
#endif

#ifdef ENABLE_SPI_HM_DEBUG
#define PRINT_HM_DEBUG(args) PRINT_MACRO(args)
#else
#define PRINT_HM_DEBUG(args)
#endif

#ifdef ENABLE_SPI_HM_ERROR
#define PRINT_HM_ERROR(args) PRINT_MACRO(args)
#else
#define PRINT_HM_ERROR(args)
#endif

#ifdef ENABLE_SPI_HM_HEX
#define PRINT_HM_HEX(args, data, data_len) PRINT_HM_HEX_DUMP(args, data, data_len)
#else
#define PRINT_HM_HEX(args, data, data_len)
#endif

#define PRINT_HM_HEX_DUMP(args, data, data_len) { \
        uint16_t i; \
        PRINT_MACRO((args)); \
        PRINT_MACRO((":\n")); \
        for (i = 0; i < data_len; i++) { \
            PRINT_MACRO(("%c", data[i])); \
        } \
        PRINT_MACRO(("\n")); \
} \

#define CHK_RET(expr)  { \
        cy_rslt_t check_res = (expr); \
        if (check_res != CY_RSLT_SUCCESS) \
        { \
            PRINT_HM_ERROR(("Function %s failed at line %d type 0x%lx module 0x%lx submodule 0x%lx code 0x%lx\n", \
                __func__, __LINE__, \
                (check_res >> CY_RSLT_TYPE_POSITION) & CY_RSLT_TYPE_MASK, \
                (check_res >> CY_RSLT_MODULE_POSITION) & CY_RSLT_MODULE_MASK, \
                (check_res >> CY_RSLT_SUBMODULE_POSITION) & CY_RSLT_SUBMODULE_MASK, \
                (check_res >> CY_RSLT_CODE_POSITION) & CY_RSLT_CODE_MASK)); \
            return check_res; \
        } \
}

#ifdef COMPONENT_CAT5                  /* SPI Master mode AT-Command bringup is verified on H1-CP */
#define SPI_HM_MOSI    BT_GPIO_3
#define SPI_HM_MISO    BT_GPIO_2
#define SPI_HM_SCLK    BT_GPIO_17
#define SPI_HM_SSEL    BT_GPIO_16
#define SPI_IN_GPIO    BT_GPIO_0
#define SPI_OP_GPIO    BT_GPIO_5
#define SPI_HM_MODE    CYHAL_SPI_MODE_00_MSB
#define SPI_HM_WIDTH   8
#else
#define SPI_HM_MISO
#define SPI_HM_MOSI
#define SPI_HM_SCLK
#define SPI_HM_SSEL
#define SPI_IN_GPIO
#define SPI_OP_GPIO
#endif

/**
 * Enumeration of Infineon command
 */
enum {
    INF_AT_CMD         = 1,
    INF_AT_EVT         = 2
};

typedef struct
{
    uint8_t type;
    uint8_t flags;
    uint8_t length;
    uint8_t reserved[4];
} spi_hm_sw_hdr_t;

struct spi_hm_handler
{
    cyhal_spi_t spi_hm_obj;
    cy_thread_t spi_task;
    cy_semaphore_t spi_task_sema;
    uint8_t rx_payload[200];    /* will be changed to dynamic allocation later */
    uint8_t *tx_payload;
    bool is_at_cmd_avl;
    bool is_at_read;
    uint8_t at_cmd_buf[200];
};

typedef struct spi_hm_handler *spi_hm_handler_t;

cy_rslt_t spi_hm_init (void);
cy_rslt_t spi_hm_deinit (void);
spi_hm_handler_t spi_hm_get_main_handler (void);
uint32_t spi_hm_proto_send (spi_hm_handler_t spi_hm_send, spi_hm_sw_hdr_t *spi_proto_hdr);
uint32_t spi_hm_proto_recv (spi_hm_handler_t spi_hm_recv, spi_hm_sw_hdr_t *spi_proto_hdr);
bool spi_hm_atcmd_is_data_ready (void);
uint32_t spi_hm_atcmd_write_data(uint8_t *buffer, uint32_t length);
uint32_t spi_hm_atcmd_read_data(uint8_t *buffer, uint32_t length);
#endif /* _SPI_HOSTED_SUPPORT_H_ */
