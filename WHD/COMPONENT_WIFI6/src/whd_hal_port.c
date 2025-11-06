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
 *  This source file implements the common HAL APIs which call the supported Infineon HAL functions
 *  based on the macro either COMPONENT_MTB_HAL or CYHAL_API_VERSION whether defined.
 */

#include "whd_hal_port.h"

#if !defined(WHD_USE_CUSTOM_HAL_IMPL) && ((CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) || (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)) && !defined(COMPONENT_WIFI_INTERFACE_OCI)

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)
#define WLAN_INTR_PRIORITY 1 /* XXX FIXME */
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */


#if defined (COMPONENT_MTB_HAL)
/*============================== HAL Next ==============================*/

whd_bool_t whd_hal_is_oob_pin_avaliable(const whd_oob_config_t* oob_config)
{
    return ((oob_config->host_oob_pin->pin_num == WHD_NC_PIN_VALUE) ? WHD_FALSE : WHD_TRUE);
}

whd_result_t whd_hal_gpio_register_callback(whd_oob_config_t* oob_config, whd_bool_t register_action, whd_hal_gpio_event_callback_t callback, void* callback_arg)
{
    if (register_action == WHD_TRUE) /* Register */
    {
        mtb_hal_gpio_register_callback(oob_config->host_oob_pin, callback, callback_arg);
    }
    else /* Unregister */
    {
        mtb_hal_gpio_register_callback(oob_config->host_oob_pin, NULL, NULL);
    }

    return WHD_SUCCESS;
}

void whd_hal_gpio_enable_event(whd_oob_config_t* oob_config, whd_bool_t enable)
{
    const mtb_hal_gpio_event_t event =
        (oob_config->is_falling_edge == WHD_TRUE) ? MTB_HAL_GPIO_IRQ_FALL : MTB_HAL_GPIO_IRQ_RISE;
    mtb_hal_gpio_enable_event(oob_config->host_oob_pin, event, (enable == WHD_TRUE) ? true : false);
}

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
void whd_hal_sdio_enable_event(whd_sdio_t* sdio_obj, whd_bool_t enable)
{
    mtb_hal_sdio_enable_event(sdio_obj, MTB_HAL_SDIO_CARD_INTERRUPT, enable);
}
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */



/*============================== Legacy HAL 2.X ==============================*/
#elif defined(CYHAL_API_VERSION) && (CYHAL_API_VERSION == 2)
whd_bool_t whd_hal_is_oob_pin_avaliable(const whd_oob_config_t* oob_config)
{
    return ((oob_config->host_oob_pin == WHD_NC_PIN_VALUE) ? WHD_FALSE : WHD_TRUE);
}

whd_result_t whd_hal_gpio_register_callback(whd_oob_config_t* oob_config, whd_bool_t register_action, whd_hal_gpio_event_callback_t callback, void* callback_arg)
{
    const whd_oob_config_t *const_oob_config = oob_config;

    if (register_action == WHD_TRUE) /* Register */
    {
    #if !defined(CYCFG_WIFI_HOST_WAKE_ENABLED)
    #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
        CHECK_RETURN(cyhal_gpio_init(const_oob_config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, const_oob_config->drive_mode, const_oob_config->init_drive_state));
    #else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
        CHECK_RETURN(cyhal_gpio_init(const_oob_config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_ANALOG, (const_oob_config->is_falling_edge == WHD_TRUE) ? 1 : 0));
    #endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
    #endif /* !defined(CYCFG_WIFI_HOST_WAKE_ENABLED) */

        static cyhal_gpio_callback_data_t cbdata;
        cbdata.callback = callback;
        cbdata.callback_arg = callback_arg;
        cyhal_gpio_register_callback(const_oob_config->host_oob_pin, &cbdata);
    }
    else /* Unregister */
    {
        cyhal_gpio_register_callback(const_oob_config->host_oob_pin, NULL);
        cyhal_gpio_free(const_oob_config->host_oob_pin);
    }

    return WHD_SUCCESS;
}

void whd_hal_gpio_enable_event(whd_oob_config_t* oob_config, whd_bool_t enable)
{
    const whd_oob_config_t *const_oob_config = oob_config;
    const cyhal_gpio_event_t event =
        (const_oob_config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;
#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
    cyhal_gpio_enable_event(const_oob_config->host_oob_pin, event, const_oob_config->intr_priority, (enable == WHD_TRUE) ? true : false);
#else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
    cyhal_gpio_enable_event(const_oob_config->host_oob_pin, event, WLAN_INTR_PRIORITY, (enable == WHD_TRUE) ? true : false);
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
}

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
void whd_hal_sdio_enable_event(whd_sdio_t* sdio_obj, whd_bool_t enable)
{
    cyhal_sdio_enable_event(sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, CYHAL_ISR_PRIORITY_DEFAULT, enable);
}
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */



#else
/*============================== Legacy HAL 1.X (To be removed) ==============================*/

whd_bool_t whd_hal_is_oob_pin_avaliable(const whd_oob_config_t* oob_config)
{
    return ((oob_config->host_oob_pin == WHD_NC_PIN_VALUE) ? WHD_FALSE : WHD_TRUE);
}

whd_result_t whd_hal_gpio_register_callback(whd_oob_config_t* oob_config, whd_bool_t register_action, whd_hal_gpio_event_callback_t callback, void* callback_arg)
{
    const whd_oob_config_t *const_oob_config = oob_config;

    if (register_action == WHD_TRUE) /* Register */
    {
    #if !defined(CYCFG_WIFI_HOST_WAKE_ENABLED)
    #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
        CHECK_RETURN(cyhal_gpio_init(const_oob_config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, const_oob_config->drive_mode, const_oob_config->init_drive_state));
    #else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
        CHECK_RETURN(cyhal_gpio_init(const_oob_config->host_oob_pin, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_ANALOG, (const_oob_config->is_falling_edge == WHD_TRUE) ? 1 : 0));
    #endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
    #endif /* !defined(CYCFG_WIFI_HOST_WAKE_ENABLED) */
    #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
        cyhal_gpio_register_irq(const_oob_config->host_oob_pin, const_oob_config->intr_priority, callback, callback_arg);
    #else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
        cyhal_gpio_register_irq(const_oob_config->host_oob_pin, WLAN_INTR_PRIORITY, callback, callback_arg);
    #endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
    }
    else /* Unregister */
    {
    #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
        cyhal_gpio_register_irq(const_oob_config->host_oob_pin, const_oob_config->intr_priority, NULL, NULL);
    #else /* (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE) */
        cyhal_gpio_register_irq(const_oob_config->host_oob_pin, WLAN_INTR_PRIORITY, NULL, NULL);
    #endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */
        cyhal_gpio_free(const_oob_config->host_oob_pin);
    }

    return WHD_SUCCESS;
}

void whd_hal_gpio_enable_event(whd_oob_config_t* oob_config, whd_bool_t enable)
{
    const whd_oob_config_t *const_oob_config = oob_config;
    const cyhal_gpio_irq_event_t event =
        (const_oob_config->is_falling_edge == WHD_TRUE) ? CYHAL_GPIO_IRQ_FALL : CYHAL_GPIO_IRQ_RISE;
    cyhal_gpio_irq_enable(const_oob_config->host_oob_pin, event, (enable == WHD_TRUE) ? true : false);
}

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE)
void whd_hal_sdio_enable_event(whd_sdio_t* sdio_obj, whd_bool_t enable)
{
    cyhal_sdio_irq_enable(sdio_obj, CYHAL_SDIO_CARD_INTERRUPT, enable);
}
#endif /* #if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) */

#endif /* defined (COMPONENT_MTB_HAL) */


#endif /* !defined(WHD_USE_CUSTOM_HAL_IMPL) && ((CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) || (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)) */
