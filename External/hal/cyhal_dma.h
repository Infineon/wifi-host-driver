/***************************************************************************//**
* \file cyhal_dma.h
*
* \brief
* Provides a high level interface for interacting with the Infineon DMA.
* This interface abstracts out the chip specific details. If any chip specific
* functionality is necessary, or performance is critical the low level functions
* can be used directly.
*
********************************************************************************
* \copyright
* Copyright 2018-2021 Cypress Semiconductor Corporation
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
*******************************************************************************/

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cy_result.h"
#include "cyhal_hw_types.h"

#if defined(__cplusplus)
extern "C" {
#endif


/**
 * \}
 */

/** Direction for DMA transfers. */
typedef enum
{
    CYHAL_DMA_DIRECTION_MEM2MEM,       //!< Memory to memory
    CYHAL_DMA_DIRECTION_MEM2PERIPH,    //!< Memory to peripheral
    CYHAL_DMA_DIRECTION_PERIPH2MEM,    //!< Peripheral to memory
    CYHAL_DMA_DIRECTION_PERIPH2PERIPH, //!< Peripheral to peripheral
} cyhal_dma_direction_t;

/** Flags enum of DMA events. Multiple events can be enabled via \ref cyhal_m2m_enable_event and
 * the callback from \ref cyhal_m2m_register_callback will be run to notify. */
typedef enum
{
    CYHAL_DMA_NO_INTR             = 0,      //!< No interrupt
    CYHAL_DMA_TRANSFER_COMPLETE   = 1 << 0, /**< Indicates that an individual transfer (burst or
                                                 full) has completed based on the specified \ref
                                                 cyhal_dma_transfer_action_t */
    CYHAL_DMA_DESCRIPTOR_COMPLETE = 1 << 1, //!< Indicates that the full transfer has completed
    CYHAL_DMA_SRC_BUS_ERROR       = 1 << 2, //!< Indicates that there is a source bus error
    CYHAL_DMA_DST_BUS_ERROR       = 1 << 3, //!< Indicates that there is a destination bus error
    CYHAL_DMA_SRC_MISAL           = 1 << 4, //!< Indicates that the source address is not aligned
    CYHAL_DMA_DST_MISAL           = 1 << 5, //!< Indicates that the destination address is not aligned
    CYHAL_DMA_CURR_PTR_NULL       = 1 << 6, //!< Indicates that the current descriptor pointer is null
    CYHAL_DMA_ACTIVE_CH_DISABLED  = 1 << 7, //!< Indicates that the active channel is disabled
    CYHAL_DMA_DESCR_BUS_ERROR     = 1 << 8, //!< Indicates that there has been a descriptor bus error
} cyhal_m2m_event_t;

/** Specifies the transfer type to trigger when an input signal is received. */
typedef enum
{
    CYHAL_DMA_INPUT_TRIGGER_SINGLE_ELEMENT, //!< Transfer a single element when an input signal is received
    CYHAL_DMA_INPUT_TRIGGER_SINGLE_BURST,   //!< Transfer a single burst when an input signal is received
    CYHAL_DMA_INPUT_TRIGGER_ALL_ELEMENTS,   //!< Transfer all elements when an input signal is received
} cyhal_dma_input_t;

/** Specifies the transfer completion event that triggers a signal output. */
typedef enum
{
    CYHAL_DMA_OUTPUT_TRIGGER_SINGLE_ELEMENT, //!< Trigger an output when a single element is transferred
    CYHAL_DMA_OUTPUT_TRIGGER_SINGLE_BURST,   //!< Trigger an output when a single burst is transferred
    CYHAL_DMA_OUTPUT_TRIGGER_ALL_ELEMENTS,   //!< Trigger an output when all elements are transferred
} cyhal_dma_output_t;

typedef enum
{
    /** A single burst is triggered and a \ref CYHAL_DMA_TRANSFER_COMPLETE will occur after
     * each burst. The channel will be left enabled and can continue to be triggered. */
    CYHAL_DMA_TRANSFER_BURST,
    /** All bursts are triggered and a single \ref CYHAL_DMA_TRANSFER_COMPLETE will occur at
     * the end. The channel will be left enabled and can continue to be triggered. */
    CYHAL_DMA_TRANSFER_FULL,
    /** A single burst is triggered and a \ref CYHAL_DMA_TRANSFER_COMPLETE will occur after
     * each burst. When all bursts are complete, the channel will be disabled. */
    CYHAL_DMA_TRANSFER_BURST_DISABLE,
    /** All bursts are triggered and a single \ref CYHAL_DMA_TRANSFER_COMPLETE will occur at
     * the end. When complete, the channel will be disabled. */
    CYHAL_DMA_TRANSFER_FULL_DISABLE,
} cyhal_dma_transfer_action_t;

/** \brief Configuration of a DMA channel. When configuring address,
 * increments, and transfer width keep in mind your hardware may have more
 * stringent address and data alignment requirements. */
typedef struct
{
    uint32_t src_addr;                  //!< Source address
    int16_t src_increment;              //!< Source address auto increment amount in multiples of transfer_width
    uint32_t dst_addr;                  //!< Destination address
    int16_t dst_increment;              //!< Destination address auto increment amount in multiples of transfer_width
    uint8_t transfer_width;             //!< Transfer width in bits. Valid values are: 8, 16, or 32
    uint32_t length;                    //!< Number of elements to be transferred in total
    uint32_t burst_size;                //!< Number of elements to be transferred per trigger. If set to 0 every element is transferred, otherwise burst_size must evenly divide length.
    cyhal_dma_transfer_action_t action; //!< Sets the behavior of the channel when triggered (using start_transfer). Ignored if burst_size is not configured.
} cyhal_dma_cfg_t;

/** Event handler for DMA interrupts */
typedef void (*cyhal_dma_event_callback_t)(void *callback_arg, cyhal_m2m_event_t event);

/** Generic trigger source defined for devices that do not support trigger mux. */
typedef uint32_t cyhal_source_t;

/** Generic trigger destination defined for devices that do not support trigger mux. */
typedef uint32_t cyhal_dest_t;

/** DMA input connection information to setup while initializing the driver. */
typedef struct
{
    cyhal_source_t source;      //!< Source of signal to DMA; obtained from another driver's cyhal_<PERIPH>_enable_output
    cyhal_dma_input_t input;    //!< DMA input signal to be driven
} cyhal_dma_src_t;

/** DMA output connection information to setup while initializing the driver. */
typedef struct
{
    cyhal_dma_output_t output;  //!< Output signal of DMA
    cyhal_dest_t dest;          //!< Destination of DMA signal
} cyhal_dma_dest_t;

/** Initialize the DMA peripheral.
 *
 * If a source signal is provided for \p src, this will connect the provided signal to the DMA
 * just as would be done by calling \ref cyhal_dma_connect_digital. Similarly, if a destination
 * target is provided for \p dest this will enable the specified output just as would be done
 * by calling \ref cyhal_dma_enable_output.
 * @param[out] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @param[in]  src          An optional source signal to connect to the DMA
 * @param[in]  dest         An optional destination singal to drive from the DMA
 * @param[out] dest_source  An optional pointer to user-allocated source signal object which
 * will be initialized by enable_output. If \p dest is non-null, this must also be non-null.
 * \p dest_source should be passed to (dis)connect_digital functions to (dis)connect the
 * associated endpoints.
 * @param[in]  priority     The priority of this DMA operation relative to others. The number of
 * priority levels which are supported is hardware dependent. All implementations define a
 * #CYHAL_DMA_PRIORITY_DEFAULT constant which is always valid. If supported, implementations will
 * also define #CYHAL_DMA_PRIORITY_HIGH, #CYHAL_DMA_PRIORITY_MEDIUM, and #CYHAL_DMA_PRIORITY_LOW.
 * The behavior of any other value is implementation defined. See the implementation-specific DMA
 * documentation for more details.
 * @param[in]  direction    The direction memory is copied
 * @return The status of the init request
 */
cy_rslt_t cyhal_dma_init_adv(cyhal_dma_t *obj, cyhal_dma_src_t *src, cyhal_dma_dest_t *dest,
                             cyhal_source_t *dest_source, uint8_t priority, cyhal_dma_direction_t direction);

/** Initialize the DMA peripheral.
 *
 * @param[out] obj          Pointer to a DMA object. The caller must allocate the memory for this
 * object but the init function will initialize its contents.
 * @param[in]  priority     The priority of this DMA operation relative to others. The number of
 * priority levels which are supported is hardware dependent. All implementations define a
 * #CYHAL_DMA_PRIORITY_DEFAULT constant which is always valid. If supported, implementations will
 * also define #CYHAL_DMA_PRIORITY_HIGH, #CYHAL_DMA_PRIORITY_MEDIUM, and #CYHAL_DMA_PRIORITY_LOW.
 * The behavior of any other value is implementation defined. See the implementation-specific DMA
 * documentation for more details.
 * @param[in]  direction    The direction memory is copied
 * @return The status of the init request
 */
#define cyhal_m2m_init(obj, priority, direction)    (cyhal_dma_init_adv(obj, NULL, NULL, NULL, priority, direction) )

/** Free the DMA object. Freeing a DMA object while a transfer is in progress
 * (\ref cyhal_m2m_is_busy) is invalid.
 *
 * @param[in,out] obj The DMA object
 */
void cyhal_m2m_free(cyhal_dma_t *obj);

/** Setup the DMA channel behavior. This will also enable the channel to allow it to be triggered.
 * The transfer can be software triggered by calling \ref cyhal_dma_start_transfer or by hardware.
 * A hardware input signal is setup by \ref cyhal_dma_connect_digital or \ref cyhal_dma_init_adv.
 * \note If hardware triggers are used, any necessary event callback setup (\ref
 * cyhal_m2m_register_callback and \ref cyhal_m2m_enable_event) should be done before calling
 * this function to ensure the handlers are in place before the transfer can happen.
 * \note The automatic enablement of the channel as part of this function is expected to change
 * in a future update. This would only happen on a new major release (eg: 1.0 -> 2.0).
 *
 * @param[in] obj    The DMA object
 * @param[in] cfg    Configuration parameters for the transfer
 * @return The status of the configure request
 */
cy_rslt_t cyhal_dma_configure(cyhal_dma_t *obj, const cyhal_dma_cfg_t *cfg);

/** Enable the DMA transfer so that it can start transferring data when triggered. A trigger is
 * caused either by calling \ref cyhal_dma_start_transfer or by hardware as a result of a connection
 * made in either \ref cyhal_dma_connect_digital or \ref cyhal_dma_init_adv. The DMA can be disabled
 * by calling \ref cyhal_dma_disable or by setting the \ref cyhal_dma_cfg_t action to \ref
 * CYHAL_DMA_TRANSFER_BURST_DISABLE, or \ref CYHAL_DMA_TRANSFER_FULL_DISABLE.
 *
 * @param[in] obj    The DMA object
 * @return The status of the enable request
 */
cy_rslt_t cyhal_dma_enable(cyhal_dma_t *obj);

/** Disable the DMA transfer so that it does not continue to trigger. It can be reenabled by calling
 * \ref cyhal_dma_enable or \ref cyhal_dma_configure.
 *
 * @param[in] obj    The DMA object
 * @return The status of the enable request
 */
cy_rslt_t cyhal_dma_disable(cyhal_dma_t *obj);

/** Initiates DMA channel transfer for specified DMA object. This should only be done after the
 * channel has been configured (\ref cyhal_dma_configure) and any necessary event callbacks setup
 * (\ref cyhal_m2m_register_callback \ref cyhal_m2m_enable_event)
 *
 * @param[in] obj    The DMA object
 * @return The status of the start_transfer request
 */
cy_rslt_t cyhal_dma_start_transfer(cyhal_dma_t *obj);

/** Checks if the transfer has been triggered, but not yet complete (eg: is pending, blocked or running)
 *
 * @param[in] obj    The DMA object
 * @return True if DMA channel is busy
 */
bool cyhal_m2m_is_busy(cyhal_dma_t *obj);

/** Register a DMA callback handler.
 *
 * This function will be called when one of the events enabled by \ref cyhal_m2m_enable_event occurs.
 *
 * @param[in] obj          The DMA object
 * @param[in] callback     The callback handler which will be invoked when an event triggers
 * @param[in] callback_arg Generic argument that will be provided to the callback when called
 */
void cyhal_m2m_register_callback(cyhal_dma_t *obj, cyhal_dma_event_callback_t callback, void *callback_arg);

/** Configure DMA event enablement.
 *
 * When an enabled event occurs, the function specified by \ref cyhal_m2m_register_callback will be called.
 *
 * @param[in] obj            The DMA object
 * @param[in] event          The DMA event type
 * @param[in] intr_priority  The priority for NVIC interrupt events. The priority from the most
 * recent call will take precedence, i.e all events will have the same priority.
 * @param[in] enable         True to turn on interrupts, False to turn off
 */
void cyhal_m2m_enable_event(cyhal_dma_t *obj, cyhal_m2m_event_t event, uint8_t intr_priority, bool enable);

/** Connects a source signal and enables the specified input to the DMA channel. This connection
 * can also be setup automatically on initialization via \ref cyhal_dma_init_adv. If the signal
 * needs to be disconnected later, \ref cyhal_dma_disconnect_digital can be used.
 *
 * @param[in] obj         The DMA object
 * @param[in] source      Source signal obtained from another driver's cyhal_<PERIPH>_enable_output
 * @param[in] input       Which input to enable
 * @return The status of the connection
 */
cy_rslt_t cyhal_dma_connect_digital(cyhal_dma_t *obj, cyhal_source_t source, cyhal_dma_input_t input);

/** Enables the specified output signal from a DMA channel that is triggered when a transfer is
 * completed. This can also be setup automatically on initialization via \ref cyhal_dma_init_adv.
 * If the output is not needed in the future, \ref cyhal_dma_disable_output can be used.
 *
 * @param[in]  obj         The DMA object
 * @param[in]  output      Which event triggers the output
 * @param[out] source      Pointer to user-allocated source signal object which
 * will be initialized by enable_output. \p source should be passed to
 * (dis)connect_digital functions to (dis)connect the associated endpoints.
 * @return The status of the output enable
 */
cy_rslt_t cyhal_dma_enable_output(cyhal_dma_t *obj, cyhal_dma_output_t output, cyhal_source_t *source);

/** Disconnects a source signal and disables the specified input to the DMA channel. This removes
 * the connection that was established by either \ref cyhal_dma_init_adv or \ref
 * cyhal_dma_connect_digital.
 *
 * @param[in] obj         The DMA object
 * @param[in] source      Source signal from cyhal_<PERIPH>_enable_output to disable
 * @param[in] input       Which input to disable
 * @return The status of the disconnect
 */
cy_rslt_t cyhal_dma_disconnect_digital(cyhal_dma_t *obj, cyhal_source_t source, cyhal_dma_input_t input);

/** Disables the specified output signal from a DMA channel. This turns off the signal that was
 * enabled by either \ref cyhal_dma_init_adv or \ref cyhal_dma_enable_output. It is recommended
 * that the signal is disconnected (cyhal_<PERIPH>_disconnect_digital) from anything it might be
 * driving before being disabled.
 *
 * @param[in]  obj         The DMA object
 * @param[in]  output      Which output to disable
 * @return The status of the disablement
 * */
cy_rslt_t cyhal_dma_disable_output(cyhal_dma_t *obj, cyhal_dma_output_t output);

//cyhal_dma_impl.h ++
/** M2M DMA group */
typedef enum
{
    _CYHAL_M2M_GRP_WWD,     //!< Group used for WWD
    _CYHAL_M2M_GRP_USR_1,   //!< User group 1
    _CYHAL_M2M_GRP_USR_2    //!< User group 2
} _cyhal_m2m_group_t;


/** WHD M2M reinitialization for setting RX_buffer_size.
 *
 * Function for WHD to reinitialize the DMA with a different RX buffer size.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @param[in]  rx_buffer_size Size of the RX buffer.
 */
void _cyhal_m2m_reinit_dma(cyhal_dma_t *obj, uint32_t rx_buffer_size);

/** WHD M2M packet-based DMA TX data.
 *
 * Port of m2m_dma_tx_data() referenced by wwd_bus_protocol.c in WICED.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @param[in]  buffer Data buffer to be used in the packet DMA transaction.
 * @return The status of the tx operation.
 */
int cyhal_m2m_tx_send(cyhal_dma_t *obj, void *buffer);

/** WHD M2M packet-based DMA reclaim next completed TXD
 *
 * Port of m2m_dma_tx_reclaim() referenced by wwd_bus_protocol.c in WICED.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 */
void cyhal_m2m_tx_release(cyhal_dma_t *obj);

/** WHD M2M packet-based DMA read DMA packet
 *
 * Port of m2m_read_dma_packet() referenced by wwd_bus_protocol.c in WICED.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @param[out] packet packet pointer
 * @param[out] hwtag  Hardware tag
 * @return DMA packet pointer
 */
void *cyhal_m2m_rx_receive(cyhal_dma_t *obj, void *packet, uint16_t **hwtag);

/** WHD M2M packet-based DMA refill RX
 *
 * Port of m2m_refill_dma() referenced by wwd_bus_protocol.c in WICED.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @return DMA refilled (true) or failed (false)
 */
bool cyhal_m2m_rx_prepare(cyhal_dma_t *obj);

/** WHD M2M packet-based DMA RX active status
 *
 * Port of m2m_rxactive_dma() referenced by wwd_bus_protocol.c in WICED.
 * @param[in] obj  Pointer to a DMA object. The caller must allocate the memory
 *  for this object but the init function will initialize its contents.
 * @return RX active status
 */
int cyhal_m2m_rx_status(cyhal_dma_t *obj);

uint32_t cyhal_m2m_intr_status(cyhal_dma_t *obj, bool *signal_txdone);

//cyhal_dma_impl.h ++


//cyhal_system_impl.h ++
void _cyhal_system_timer_enable_irq(void);

/* Disable external interrupts from M2M_ExtIRQn to APPS Core */
void _cyhal_system_m2m_disable_irq(void);

/* Enable external interrupts from M2M_ExtIRQn to APPS Core */
void _cyhal_system_m2m_enable_irq(void);

/* Disable external interrupts from SW0_ExtIRQn to APPS Core */
void _cyhal_system_sw0_disable_irq(void);

/* Enable external interrupts from SW0_ExtIRQn to APPS Core */
void _cyhal_system_sw0_enable_irq(void);

//cyhal_system_impl.h ++

#if defined(__cplusplus)
}
#endif

/** \} group_hal_dma */
