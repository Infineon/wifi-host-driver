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

#if defined(COMPONENT_SPI_HM)

#include "spi_hosted_support.h"

/******************************************************************************
* MACROS/Preprocessors
******************************************************************************/
#define SPI_TSK_STACK_SIZE     (1024 * 4)
#define SPI_TASK_PRIORITY      (CY_RTOS_PRIORITY_HIGH)
#define SPI_BAUD_RATE          (7500000)
#define SPI_HM_RW_TMOUT        (3000)
#define SPI_HM_NEVER_TMOUT     (CY_RTOS_NEVER_TIMEOUT)

#define TX_FIFO_SIZE           (100)
#define RX_FIFO_SIZE           (100)

/******************************************************************************
* Global variables
******************************************************************************/
static spi_hm_handler_t spi_hm = NULL;
__attribute__((aligned(8))) uint8_t spi_thread_stack[SPI_TSK_STACK_SIZE] = {0};

/******************************************************************************
* Function Definitions
******************************************************************************/
static void spi_hm_trigger_intr (bool enable)
{
    cyhal_gpio_write(SPI_OP_GPIO, enable);
}

void spi_hm_irq_handler (void *arg, cyhal_gpio_event_t event)
{
    spi_hm_handler_t spi_irq_handler = (spi_hm_handler_t)arg;
    cy_rtos_set_semaphore(&spi_irq_handler->spi_task_sema, true);
}

static void spi_hm_task_func (cy_thread_arg_t arg)
{
    spi_hm_handler_t spi_task_handler = (spi_hm_handler_t)arg;
    spi_hm_sw_hdr_t spi_hdr_rx;

    if (spi_task_handler == NULL)
    {
        PRINT_HM_ERROR(("Task handler is NULL, can't start spi_hm_task_func \n"));
        return;
    }

    while(1)
    {
        whd_mem_memset(&spi_hdr_rx, 0, sizeof(spi_hm_sw_hdr_t));
        whd_mem_memset(spi_task_handler->at_cmd_buf, 0, sizeof(spi_task_handler->at_cmd_buf));

        PRINT_HM_DEBUG("WFI() \n");
        cy_rtos_get_semaphore(&spi_task_handler->spi_task_sema, SPI_HM_NEVER_TMOUT, true);

        if (spi_hm_proto_recv(spi_task_handler, &spi_hdr_rx) == WHD_SUCCESS)
        {
            if (spi_hdr_rx.type == INF_AT_CMD)
            {
               spi_task_handler->is_at_read = WHD_TRUE;
               whd_mem_memcpy(spi_task_handler->at_cmd_buf, spi_task_handler->rx_payload, spi_hdr_rx.length);
               spi_task_handler->is_at_cmd_avl = WHD_TRUE;
            }

            PRINT_HM_HEX_DUMP("SPI-RX", spi_task_handler->at_cmd_buf, spi_hdr_rx.length);

            if (spi_hdr_rx.type == INF_AT_CMD)
            {
                while (spi_task_handler->is_at_read == WHD_TRUE)
                {
                    cy_rtos_delay_milliseconds(100);
                }
            }
        }
        else
        {
            PRINT_HM_ERROR(("spi_hm_task_func: spi_hm_proto_recv failed!!! \n"));
        }
    }

}

spi_hm_handler_t spi_hm_get_main_handler (void)
{
    return spi_hm;
}

cy_rslt_t spi_hm_init (void)
{
    spi_hm_handler_t spi_handler = NULL;
    static cyhal_gpio_callback_data_t cbdata;

    spi_handler = (spi_hm_handler_t)whd_mem_malloc(sizeof(struct spi_hm_handler));
    whd_mem_memset(spi_handler, 0, sizeof(struct spi_hm_handler) );
    spi_hm = spi_handler;

    /* Configuring the SPI as master */
    CHK_RET(cyhal_spi_init(&spi_handler->spi_hm_obj, SPI_HM_MOSI, SPI_HM_MISO, SPI_HM_SCLK, SPI_HM_SSEL,
                                                   NULL, SPI_HM_WIDTH, SPI_HM_MODE, false));

    /* Set baud rate for the SPI master, make sure in sync with slave baud rate */
    CHK_RET(cyhal_spi_set_frequency(&spi_handler->spi_hm_obj, SPI_BAUD_RATE));

    /* Register GPIO as interrupt handler for SPI sync transmission */
    cy_rtos_init_semaphore(&spi_handler->spi_task_sema, 1, 0);
    cyhal_gpio_init(SPI_IN_GPIO, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_NONE, false);
    cyhal_gpio_init(SPI_OP_GPIO, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    cbdata.callback = spi_hm_irq_handler;
    cbdata.callback_arg = spi_handler;
    cyhal_gpio_register_callback(BT_GPIO_0, &cbdata);
    cyhal_gpio_enable_event(BT_GPIO_0, CYHAL_GPIO_IRQ_RISE, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Create SPI thread to handle incoming packets over SPI bus */
    CHK_RET(cy_rtos_thread_create(&spi_handler->spi_task, spi_hm_task_func, "spi-hm-task",
        (void *)spi_thread_stack, SPI_TSK_STACK_SIZE, SPI_TASK_PRIORITY, (cy_thread_arg_t)spi_handler));

    return WHD_SUCCESS;
}

cy_rslt_t spi_hm_deinit (void)
{
    spi_hm_handler_t spi_handler = spi_hm_get_main_handler();

    cyhal_spi_free(&spi_handler->spi_hm_obj);

    cyhal_gpio_enable_event(BT_GPIO_0, CYHAL_GPIO_IRQ_RISE, CYHAL_ISR_PRIORITY_DEFAULT, false);
    CHK_RET(cy_rtos_deinit_semaphore(&spi_handler->spi_task_sema));

    CHK_RET(cy_rtos_terminate_thread(&spi_handler->spi_task));

    whd_mem_free(spi_handler);
    return WHD_SUCCESS;
}

static uint32_t spi_hm_pyld_send (spi_hm_handler_t spi_hm_send, const uint8_t *tx_buffer, size_t tx_buffer_length)
{
    cyhal_spi_t *obj = &spi_hm_send->spi_hm_obj;
    uint8_t *trans_buffer;
    size_t trans_length = TX_FIFO_SIZE;
    uint32_t block_count = 0, i =0, result = 0;
    block_count = tx_buffer_length/TX_FIFO_SIZE;
    trans_buffer = (uint8_t *)tx_buffer;

    if (block_count == 0)
    {
       cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_TX, tx_buffer_length);

       spi_hm_trigger_intr(WHD_TRUE);
       cy_rtos_delay_milliseconds (10);
       result = cyhal_spi_transfer(obj, tx_buffer, tx_buffer_length, NULL, 0, 0);
       spi_hm_trigger_intr(WHD_FALSE);

       while (Cy_SCB_SPI_IsTxComplete(obj->base) == WHD_FALSE) { }
       return result;
    }

    cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_TX, TX_FIFO_SIZE);

    if (tx_buffer_length%TX_FIFO_SIZE)
        block_count += 1;

    for (i = 0; i < block_count; i++)
    {
        if(i == (block_count - 1))
        {
            if(tx_buffer_length%TX_FIFO_SIZE)
                cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_TX, (tx_buffer_length%TX_FIFO_SIZE));
        }

        spi_hm_trigger_intr(WHD_TRUE);
        cy_rtos_delay_milliseconds (10);
        result = cyhal_spi_transfer(obj, trans_buffer, trans_length, NULL, 0, 0);
        spi_hm_trigger_intr(WHD_FALSE);

        while (Cy_SCB_SPI_IsTxComplete(obj->base) == WHD_FALSE) { }
        trans_buffer += trans_length;
    }

    return result;
}

uint32_t spi_hm_proto_send (spi_hm_handler_t spi_hm_send, spi_hm_sw_hdr_t *spi_proto_hdr)
{
    uint32_t result = 0;
    cyhal_spi_t *obj = &spi_hm_send->spi_hm_obj;

    cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_TX, sizeof(spi_hm_sw_hdr_t));

    spi_hm_trigger_intr(WHD_TRUE);
    cy_rtos_delay_milliseconds (90);
    result = cyhal_spi_transfer(obj, (const uint8_t*)spi_proto_hdr, sizeof(spi_hm_sw_hdr_t), NULL, 0, 0);
    spi_hm_trigger_intr(WHD_FALSE);

    while (Cy_SCB_SPI_IsTxComplete(obj->base) == WHD_FALSE) { }
    result = spi_hm_pyld_send (spi_hm_send, spi_hm_send->tx_payload, spi_proto_hdr->length);
    return result;
}

static uint32_t spi_hm_pyld_recv (spi_hm_handler_t spi_hm_recv, uint8_t *rx_buffer, size_t rx_buffer_length)
{
    cyhal_spi_t *obj = &spi_hm_recv->spi_hm_obj;
    uint8_t *recv_buffer;
    size_t recv_length = RX_FIFO_SIZE;
    uint32_t block_count = 0, i = 0, result = 0;
    recv_buffer = (uint8_t *)rx_buffer;

    block_count = rx_buffer_length/RX_FIFO_SIZE;

    if (block_count == 0)
    {
        cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_RX, rx_buffer_length);
        cy_rtos_get_semaphore(&spi_hm_recv->spi_task_sema, SPI_HM_RW_TMOUT, true);
        return cyhal_spi_transfer(obj, NULL, 0, rx_buffer, rx_buffer_length, 0);
    }
    cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_RX, RX_FIFO_SIZE);

    if (rx_buffer_length%RX_FIFO_SIZE)
         block_count += 1;

    for (i = 0; i < block_count; i++)
    {
       if (i == (block_count - 1))
       {
           if (rx_buffer_length%RX_FIFO_SIZE)
               cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_RX, (rx_buffer_length%RX_FIFO_SIZE));
       }

       cy_rtos_get_semaphore(&spi_hm_recv->spi_task_sema, SPI_HM_RW_TMOUT, WHD_TRUE);
       result = cyhal_spi_transfer(obj, NULL, 0, recv_buffer, recv_length, 0);
       recv_buffer += recv_length;
    }

    return result;
}

uint32_t spi_hm_proto_recv (spi_hm_handler_t spi_hm_recv, spi_hm_sw_hdr_t *spi_proto_hdr)
{
    cyhal_spi_t *obj = &spi_hm_recv->spi_hm_obj;
    uint32_t result = 0;

    cyhal_spi_set_fifo_level(obj, CYHAL_SPI_FIFO_RX, sizeof(spi_hm_sw_hdr_t));

    result = cyhal_spi_transfer(obj, NULL, 0, (uint8_t*)spi_proto_hdr, sizeof(spi_hm_sw_hdr_t), 0);
    result = spi_hm_pyld_recv(spi_hm_recv, spi_hm_recv->rx_payload, spi_proto_hdr->length);
    return result;
}

bool spi_hm_atcmd_is_data_ready (void)
{
    spi_hm_handler_t spi_hm = spi_hm_get_main_handler();

    if (spi_hm->is_at_cmd_avl)
    {
        spi_hm->is_at_cmd_avl = WHD_FALSE;
        return WHD_TRUE;
    }
    else
    {
        return WHD_FALSE;
    }
}

uint32_t spi_hm_atcmd_write_data(uint8_t *buffer, uint32_t length)
{
    spi_hm_sw_hdr_t spi_hdr_tx;
    spi_hm_handler_t spi_hm_write = spi_hm_get_main_handler();

    whd_mem_memset(&spi_hdr_tx, 0, sizeof(spi_hdr_tx));
    spi_hdr_tx.type = INF_AT_EVT;
    spi_hdr_tx.flags = 0x00;
    spi_hdr_tx.length = length;
    spi_hm_write->tx_payload = whd_mem_malloc((spi_hdr_tx.length)*sizeof(uint8_t));
    whd_mem_memset(spi_hm_write->tx_payload, 0, spi_hdr_tx.length);

    if(spi_hm_write->tx_payload == NULL)
    {
        PRINT_HM_ERROR(("spi_cmd_at_write_data: TX payload alloc failed \n"));
        return WHD_MALLOC_FAILURE;
    }
    memcpy(spi_hm_write->tx_payload, buffer, spi_hdr_tx.length);

    PRINT_HM_DEBUG(("spi_cmd_at_write_data ----> \n"));

    PRINT_HM_HEX_DUMP("SPI-TX", spi_hm_write->tx_payload, spi_hdr_tx.length);

    if (spi_hm_proto_send(spi_hm_write, &spi_hdr_tx) == WHD_SUCCESS)
    {
        whd_mem_free(spi_hm_write->tx_payload);
        return WHD_SUCCESS;
    }
    else
    {
        whd_mem_free(spi_hm_write->tx_payload);
        PRINT_HM_ERROR(("spi_hm_atcmd_write_data: TX failure \n"));
        return WHD_UNFINISHED;
    }
}

uint32_t spi_hm_atcmd_read_data(uint8_t *buffer, uint32_t length)
{
    uint32_t at_cmd_len = 0;
    spi_hm_handler_t spi_hm_write = spi_hm_get_main_handler();

    if(buffer == NULL)
    {
        PRINT_HM_ERROR(("spi_cmd_at_write_data: TX payload alloc failed \n"));
        return WHD_MALLOC_FAILURE;
    }

    if (length < strlen((const char*)spi_hm_write->at_cmd_buf)) {
        PRINT_HM_INFO(("buffer size %ld isn't enough to get AT command %d\n", length, strlen((const char*)spi_hm_write->at_cmd_buf)));
        return WHD_BADARG;
    }

    PRINT_HM_DEBUG(("spi_cmd_at_read_data <---- \n"));
    PRINT_HM_DEBUG(("AT CMD read data: %s\n", spi_hm_write->at_cmd_buf));

    at_cmd_len = strlen((const char*)spi_hm_write->at_cmd_buf);
    if (at_cmd_len > 0)
    {
        memcpy(buffer, spi_hm_write->at_cmd_buf, at_cmd_len);
    }

    spi_hm_write->is_at_read = WHD_FALSE;
    return at_cmd_len;
}
#endif /* COMPONENT_SPI_HM */
