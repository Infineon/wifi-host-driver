/*
 * (c) 2025, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG.  SPDX-License-Identifier: Apache-2.0
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

/** @file whd_hal_port.h
 *  Provides the abstract layer for WHD to use HAL driver.
 *
 *  This hearder file defines the common names for Infineon HAL driver which incl. the header files / type / macro / APIs.
 *  Depends on the macro either COMPONENT_MTB_HAL or CYHAL_API_VERSION whether defined,
 *  to map the common HAL definitions to the corresponding version of multiple Infineon HAL drivers.
 */

#ifndef INCLUDED_WHD_HAL_PORT_H
#define INCLUDED_WHD_HAL_PORT_H

#include "whd.h"
#include "whd_int.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(WHD_USE_CUSTOM_HAL_IMPL) && ((CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) || (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)) && !defined(COMPONENT_WIFI_INTERFACE_OCI)



#if defined(COMPONENT_MTB_HAL)
/*============================== HAL Next ==============================*/

/******************************************************
*             Header files
******************************************************/
#define HAL_SYSPM_H "mtb_hal_syspm.h"
#define HAL_HW_TYPES_H "mtb_hal_hw_types.h"
#define HAL_GPIO_H "mtb_hal_gpio.h"
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
#define HAL_SDIO_H "mtb_hal_sdio.h"
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
#define HAL_SPI_H "mtb_hal_spi.h"
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
#define HAL_DMA_H "mtb_hal_dma.h"

/******************************************************
*             Type definition
******************************************************/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
typedef mtb_hal_syspm_callback_state_t whd_hal_syspm_callback_state_t;
typedef mtb_hal_syspm_callback_mode_t whd_hal_syspm_callback_mode_t;
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
typedef	mtb_hal_gpio_event_t whd_hal_gpio_event_t;
typedef mtb_hal_gpio_event_callback_t whd_hal_gpio_event_callback_t;
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
typedef mtb_hal_sdio_event_t whd_hal_sdio_event_t;
typedef mtb_hal_sdio_host_transfer_type_t whd_hal_sdio_host_transfer_type_t;
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

/******************************************************
 *            Macro definition
******************************************************/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
/* The enum of mtb_hal_syspm_callback_state_t */
#define WHD_HAL_SYSPM_CB_CPU_DEEPSLEEP MTB_HAL_SYSPM_CB_CPU_DEEPSLEEP
/* The enum of mtb_hal_syspm_callback_mode_t */
#define WHD_HAL_SYSPM_CHECK_READY MTB_HAL_SYSPM_CHECK_READY
#define WHD_HAL_SYSPM_CHECK_FAIL MTB_HAL_SYSPM_CHECK_FAIL
#define WHD_HAL_SYSPM_BEFORE_TRANSITION MTB_HAL_SYSPM_BEFORE_TRANSITION
#define WHD_HAL_SYSPM_AFTER_TRANSITION MTB_HAL_SYSPM_AFTER_TRANSITION
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
/* Integer representation of no connect gpio */
#define WHD_NC_PIN_VALUE MTB_HAL_GPIO_NC_VALUE
/* The enum of mtb_hal_gpio_event_t */
#define WHD_HAL_GPIO_IRQ_RISE MTB_HAL_GPIO_IRQ_RISE
#define WHD_HAL_GPIO_IRQ_FALL MTB_HAL_GPIO_IRQ_FALL
/* The enum of mtb_hal_sdio_event_t */
#define WHD_HAL_SDIO_CARD_INTERRUPT MTB_HAL_SDIO_CARD_INTERRUPT
#define WHD_HAL_SDIO_CMD_IO_RW_DIRECT MTB_HAL_SDIO_CMD_IO_RW_DIRECT

/******************************************************
*             HAL APIs
******************************************************/
// system
#define whd_hal_system_delay_ms mtb_hal_system_delay_ms
// syspm
#define whd_hal_syspm_lock_deepsleep mtb_hal_syspm_lock_deepsleep
#define whd_hal_syspm_unlock_deepsleep mtb_hal_syspm_unlock_deepsleep
#define whd_hal_syspm_register_callback mtb_hal_syspm_register_callback
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
// sdio
#define whd_hal_sdio_host_bulk_transfer mtb_hal_sdio_host_bulk_transfer
#define whd_hal_sdio_host_send_cmd mtb_hal_sdio_host_send_cmd
#define whd_hal_sdio_register_callback mtb_hal_sdio_register_callback
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
// spi
#define whd_hal_spi_transfer mtb_hal_spi_transfer
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */



#elif defined(CYHAL_API_VERSION) && (CYHAL_API_VERSION == 2)
/*============================== Legacy HAL 2.X ==============================*/

/******************************************************
*             Header files
******************************************************/
#define HAL_SYSPM_H "cyhal_syspm.h"
#define HAL_HW_TYPES_H "cyhal_hw_types.h"
#define HAL_GPIO_H "cyhal_gpio.h"
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
#define HAL_SDIO_H "cyhal_sdio.h"
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
#define HAL_SPI_H "cyhal_spi.h"
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
#define HAL_DMA_H "cyhal_dma.h"

/******************************************************
*             Type definition
******************************************************/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
typedef cyhal_syspm_callback_state_t whd_hal_syspm_callback_state_t;
typedef cyhal_syspm_callback_mode_t whd_hal_syspm_callback_mode_t;
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
typedef cyhal_gpio_event_t whd_hal_gpio_event_t;
typedef cyhal_gpio_event_callback_t whd_hal_gpio_event_callback_t;
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
typedef cyhal_sdio_event_t whd_hal_sdio_event_t;
typedef cyhal_sdio_transfer_type_t whd_hal_sdio_host_transfer_type_t;
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

/******************************************************
 *            Macro definition
******************************************************/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
/* The enum of cyhal_syspm_callback_state_t */
#define WHD_HAL_SYSPM_CB_CPU_DEEPSLEEP CYHAL_SYSPM_CB_CPU_DEEPSLEEP
/* The enum of whd_hal_syspm_callback_mode_t */
#define WHD_HAL_SYSPM_CHECK_READY CYHAL_SYSPM_CHECK_READY
#define WHD_HAL_SYSPM_CHECK_FAIL CYHAL_SYSPM_CHECK_FAIL
#define WHD_HAL_SYSPM_BEFORE_TRANSITION CYHAL_SYSPM_BEFORE_TRANSITION
#define WHD_HAL_SYSPM_AFTER_TRANSITION CYHAL_SYSPM_AFTER_TRANSITION
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
/* Integer representation of no connect gpio */
#define WHD_NC_PIN_VALUE (0xFF)
/* The enum of cyhal_gpio_event_t */
#define WHD_HAL_GPIO_IRQ_RISE CYHAL_GPIO_IRQ_RISE
#define WHD_HAL_GPIO_IRQ_FALL CYHAL_GPIO_IRQ_FALL
/* The enum of cyhal_sdio_event_t */
#define WHD_HAL_SDIO_CARD_INTERRUPT CYHAL_SDIO_CARD_INTERRUPT
#define WHD_HAL_SDIO_CMD_IO_RW_DIRECT CYHAL_SDIO_CMD_IO_RW_DIRECT

/******************************************************
*             HAL APIs
******************************************************/
// system
#define whd_hal_system_delay_ms cyhal_system_delay_ms
// syspm
#define whd_hal_syspm_lock_deepsleep cyhal_syspm_lock_deepsleep
#define whd_hal_syspm_unlock_deepsleep cyhal_syspm_unlock_deepsleep
#define whd_hal_syspm_register_callback cyhal_syspm_register_callback
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
// sdio
#define whd_hal_sdio_host_bulk_transfer cyhal_sdio_bulk_transfer
#define whd_hal_sdio_host_send_cmd cyhal_sdio_send_cmd
#define whd_hal_sdio_register_callback cyhal_sdio_register_callback
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
// spi
#define whd_hal_spi_transfer cyhal_spi_transfer
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */



#else
/*============================== Legacy HAL 1.X ==============================*/

/******************************************************
*             Header files
******************************************************/
#define HAL_SYSPM_H "cyhal_syspm.h"
#define HAL_HW_TYPES_H "cyhal_hw_types.h"
#define HAL_GPIO_H "cyhal_gpio.h"
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
#define HAL_SDIO_H "cyhal_sdio.h"
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
#define HAL_SPI_H "cyhal_spi.h"
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
#define HAL_DMA_H "cyhal_dma.h"

/******************************************************
*             Type definition
******************************************************/
typedef cyhal_gpio_irq_event_t whd_hal_gpio_event_t;
typedef cyhal_gpio_irq_handler_t whd_hal_gpio_event_callback_t;
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
typedef cyhal_sdio_irq_event_t whd_hal_sdio_event_t;
typedef cyhal_transfer_t whd_hal_sdio_host_transfer_type_t;
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

/******************************************************
 *            Macro definition
******************************************************/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
/* The enum of cyhal_syspm_callback_state_t */
#define WHD_HAL_SYSPM_CB_CPU_DEEPSLEEP CYHAL_SYSPM_CB_CPU_DEEPSLEEP
/* The enum of whd_hal_syspm_callback_mode_t */
#define WHD_HAL_SYSPM_CHECK_READY CYHAL_SYSPM_CHECK_READY
#define WHD_HAL_SYSPM_CHECK_FAIL CYHAL_SYSPM_CHECK_FAIL
#define WHD_HAL_SYSPM_BEFORE_TRANSITION CYHAL_SYSPM_BEFORE_TRANSITION
#define WHD_HAL_SYSPM_AFTER_TRANSITION CYHAL_SYSPM_AFTER_TRANSITION
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */
/* Integer representation of no connect gpio */
#define WHD_NC_PIN_VALUE (0xFF)
/* The enum of cyhal_gpio_event_t */
#define WHD_HAL_GPIO_IRQ_RISE CYHAL_GPIO_IRQ_RISE
#define WHD_HAL_GPIO_IRQ_FALL CYHAL_GPIO_IRQ_FALL
/* The enum of cyhal_sdio_event_t */
#define WHD_HAL_SDIO_CARD_INTERRUPT CYHAL_SDIO_CARD_INTERRUPT
#define WHD_HAL_SDIO_CMD_IO_RW_DIRECT CYHAL_SDIO_CMD_IO_RW_DIRECT

/******************************************************
*             HAL APIs
******************************************************/
// system
#define whd_hal_system_delay_ms cyhal_system_delay_ms
// syspm
#define whd_hal_syspm_lock_deepsleep cyhal_syspm_lock_deepsleep
#define whd_hal_syspm_unlock_deepsleep cyhal_syspm_unlock_deepsleep
#define whd_hal_syspm_register_callback cyhal_syspm_register_callback
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
// sdio
#define whd_hal_sdio_host_bulk_transfer cyhal_sdio_bulk_transfer
#define whd_hal_sdio_host_send_cmd cyhal_sdio_send_cmd
#define whd_hal_sdio_register_callback cyhal_sdio_register_irq
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
// spi
#define whd_hal_spi_transfer cyhal_spi_transfer
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

#endif /* defined (COMPONENT_MTB_HAL) */



/*============================== Common Definitions ==============================*/
#if defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS)
#include HAL_SYSPM_H
#endif /* defined(COMPONENT_CAT5) && !defined(WHD_DISABLE_PDS) */

extern whd_bool_t whd_hal_is_oob_pin_avaliable(const whd_oob_config_t* oob_config);
extern whd_result_t whd_hal_gpio_register_callback(whd_oob_config_t* oob_config, whd_bool_t register_action, whd_hal_gpio_event_callback_t callback, void* callback_arg);
extern void whd_hal_gpio_enable_event(whd_oob_config_t* oob_config, whd_bool_t enable);
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
extern void whd_hal_sdio_enable_event(whd_sdio_t* sdio_obj, whd_bool_t enable);
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */


#endif /* !defined(WHD_USE_CUSTOM_HAL_IMPL) && ((CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) || (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)) */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* INCLUDED_WHD_HAL_PORT_H */
