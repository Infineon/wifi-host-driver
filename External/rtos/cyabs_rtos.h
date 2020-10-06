/***************************************************************************//**
* \file cyabs_rtos.h
*
* \brief
* Defines the Cypress RTOS Interface. Provides prototypes for functions that
* allow Cypress libraries to use RTOS resources such as threads, mutexes & timing
* functions in an abstract way. The APIs are implemented
* in the Port Layer RTOS interface which is specific to the RTOS in use.
*
********************************************************************************
* \copyright
* Copyright 2018-2019 Cypress Semiconductor Corporation
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

#ifndef INCLUDED_CY_RTOS_INTERFACE_H_
#define INCLUDED_CY_RTOS_INTERFACE_H_

#include "cyabs_rtos_impl.h"
#include <cy_result.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Note, cyabs_rtos_impl.h above is included and is the implementation of some basic
 * types for the abstraction layer.  The types expected to be defined are.
 *
 * cy_thread_t              : typedef from underlying RTOS thread type
 * cy_thread_arg_t          : typedef from the RTOS type that is passed to the
 *                            entry function of a thread.
 * cy_time_t                : count of time in milliseconds
 * cy_rtos_error_t          : typedef from the underlying RTOS error type *
 *
 */


/**
 * \addtogroup group_abstraction_rtos RTOS abstraction
 * \ingroup group_abstraction
 * \{
 * Basic abstraction layer for dealing with RTOSes.
 *
 * \defgroup group_abstraction_rtos_macros Macros
 * \defgroup group_abstraction_rtos_enums Enums
 * \defgroup group_abstraction_rtos_data_structures Data Structures
 * \defgroup group_abstraction_rtos_functions Functions
 */

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************** CONSTANTS **********************************************/

/**
 * \addtogroup group_abstraction_rtos_macros
 * \{
 */

/** Used with RTOS calls that require a timeout.  This implies the call will never timeout. */
#define CY_RTOS_NEVER_TIMEOUT ( (uint32_t)0xffffffffUL )

//
// Note on error strategy.  If the error is a normal part of operation (timeouts, full queues, empty
// queues), the these errors are listed here and the abstraction layer implementation must map from the
// underlying errors to these.  If the errors are special cases, the the error CY_RTOS_GENERAL_ERROR can be
// returns and cy_rtos_last_error() used to retrieve the RTOS specific error message.
//
/** Requested operationd did not complete in the specified time */
#define CY_RTOS_TIMEOUT                     CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 0)
/** The RTOS could not allocate memory for the specified operation */
#define CY_RTOS_NO_MEMORY                   CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 1)
/** An error occured in the RTOS */
#define CY_RTOS_GENERAL_ERROR               CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 2)
/** The Queue is already full and can't accept any more items at this time */
#define CY_RTOS_QUEUE_FULL                  CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 3)
/** The Queue is empty and has nothing to remove */
#define CY_RTOS_QUEUE_EMPTY                 CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 4)
/** A bad argument was passed into the APIs */
#define CY_RTOS_BAD_PARAM                   CY_RSLT_CREATE(CY_RSLT_TYPE_ERROR, CY_RSLT_MODULE_ABSTRACTION_OS, 5)

/** \} group_abstraction_rtos_macros */


/*********************************************** TYPES **********************************************/

/**
 * \addtogroup group_abstraction_rtos_data_structures
 * \{
 */

/**
 * The type of a function that is the entry point for a thread
 *
 * @param[in] arg the argument passed from the thread create call to the entry function
 */
typedef void (*cy_thread_entry_fn_t)(cy_thread_arg_t arg);

/** \} group_abstraction_rtos_data_structures */


/**
 * \addtogroup group_abstraction_rtos_functions
 * \{
 */

/*********************************************** Threads **********************************************/


/** Create a thread with specific thread argument.
 *
 * This function is called to startup a new thread. If the thread can exit, it must call
 * cy_rtos_finish_thread() just before doing so. All created threds that can terminate, either
 * by themselves or forcefully by another thread MUST be joined in order to cleanup any resources
 * that might have been allocated for them.
 *
 * @param[out] thread         Pointer to a variable which will receive the new thread handle
 * @param[in]  entry_function Function pointer which points to the main function for the new thread
 * @param[in]  name           String thread name used for a debugger
 * @param[in]  stack          The buffer to use for the thread stack
 * @param[in]  stack_size     The size of the thread stack in bytes
 * @param[in]  priority       The priority of the thread. Values are operating system specific, but some
 *                            common priority levels are defined:
 *                                CY_THREAD_PRIORITY_LOW
 *                                CY_THREAD_PRIORITY_NORMAL
 *                                CY_THREAD_PRIORITY_HIGH
 * @param[in]  arg            The argument to pass to the new thread
 *
 * @return The status of thread create request. [CY_RSLT_SUCCESS, CY_RTOS_NO_MEMORY, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_create_thread(cy_thread_t *thread, cy_thread_entry_fn_t entry_function,
                                       const char *name, void *stack, uint32_t stack_size,
                                       cy_thread_priority_t priority, cy_thread_arg_t arg);


/** Exit the current thread.
 *
 * This function is called just before a thread exits.  In some cases it is sufficient
 * for a thread to just return to exit, but in other cases, the RTOS must be explicitly
 * signaled. In cases where a return is sufficient, this should be a null funcition.
 * where the RTOS must be signaled, this function should perform that In cases operation.
 * In code using RTOS services, this function should be placed at any at any location
 * where the main thread function will return, exiting the thread. Threads that can
 * exit must still be joined (cy_rtos_join_thread) to ensure their resources are fully
 * cleaned up.
 *
 * @return The status of thread exit request. [CY_RSLT_SUCCESS, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_exit_thread(void);

/** Terminates another thread.
 *
 * This function is called to terminate another thread and reap the resoruces claimed
 * by it thread. This should be called both when forcibly terminating another thread
 * as well as any time a thread can exit on its own. For some RTOS implementations
 * this is not required as the thread resoruces are claimed as soon as it exits. In
 * other cases, this must be called to reclaim resources. Threads that are terminated
 * must still be joined (cy_rtos_join_thread) to ensure their resources are fully
 * cleaned up.
 *
 * @param[in] thread Handle of the thread to terminate
 *
 * @returns The status of the thread terminate. [CY_RSLT_SUCCESS, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_terminate_thread(cy_thread_t *thread);

/** Checks if the thread is running
 *
 * This function is called to determine if a thread is running or not.
 *
 * @param[in] thread     handle of the terminated thread to delete
 * @param[out] state     returns true if the thread is running, otherwise false
 *
 * @returns The status of the thread check. [CY_RSLT_SUCCESS, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_is_thread_running(cy_thread_t *thread, bool *state);

/** Waits for a thread to complete.
 *
 * This must be called on any thread that can complete to ensure that any resources that
 * were allocated for it are cleaned up.
 *
 * @param[in] thread Handle of the thread to wait for
 *
 * @returns The status of thread join request. [CY_RSLT_SUCCESS, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_join_thread(cy_thread_t *thread);

/*********************************************** Semaphores **********************************************/

/**
 * Create a semaphore
 *
 * This is basically a counting semaphore.
 *
 * @param[in,out] semaphore  Pointer to the semaphore handle to be initialized
 * @param[in] maxcount       The maximum count for this semaphore
 * @param[in] initcount      The initial count for this sempahore
 *
 * @return The status of the sempahore creation. [CY_RSLT_SUCCESS, CY_RTOS_NO_MEMORY, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_init_semaphore(cy_semaphore_t *semaphore, uint32_t maxcount, uint32_t initcount);

/**
 * Get/Acquire a semaphore
 *
 * If the semaphore count is zero, waits until the semaphore count is greater than zero.
 * Once the semaphore count is greater than zero, this function decrements
 * the count and return.  It may also return if the timeout is exceeded.
 *
 * @param[in] semaphore   Pointer to the semaphore handle
 * @param[in] timeout_ms  Maximum number of milliseconds to wait while attempting to get
 *                        the semaphore. Use the NEVER_TIMEOUT constant to wait forever. Must
 *                        be zero is in_isr is true
 * @param[in] in_isr      true if we are trying to get the semaphore from with an ISR
 * @return The status of get semaphore operation [CY_RSLT_SUCCESS, CY_RTOS_NO_MEMORY, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_get_semaphore(cy_semaphore_t *semaphore, cy_time_t timeout_ms, bool in_isr);

/**
 * Set/Release a semaphore
 *
 * Increments the semaphore count, up to the maximum count for this semaphore.
 *
 * @param[in] semaphore   Pointer to the semaphore handle
 * @param[in] in_isr      Value of true indicates calling from interrupt context
 *                        Value of false indicates calling from normal thread context
 * @return The status of set semaphore operation [CY_RSLT_SUCCESS, CY_RTOS_NO_MEMORY, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_set_semaphore(cy_semaphore_t *semaphore, bool in_isr);

/**
 * Deletes a sempahore
 *
 * This function frees the resources associated with a sempahore.
 *
 * @param[in] semaphore   Pointer to the sempahore handle
 *
 * @return The status of semaphore deletion [CY_RSLT_SUCCESS, CY_RTOS_NO_MEMORY, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_deinit_semaphore(cy_semaphore_t *semaphore);


/*********************************************** Time **********************************************/

/** Gets time in milliseconds since RTOS start.
 *
 * @note Since this is only 32 bits, it will roll over every 49 days, 17 hours, 2 mins, 47.296 seconds
 *
 * @param[out] tval Pointer to the struct to populate with the RTOS time
 *
 * @returns Time in milliseconds since the RTOS started.
 */
extern cy_rslt_t cy_rtos_get_time(cy_time_t *tval);

/** Delay for a number of milliseconds.
 *
 * Processing of this function depends on the minimum sleep
 * time resolution of the RTOS. The current thread should sleep for
 * the longest period possible which is less than the delay required,
 * then makes up the difference with a tight loop.
 *
 * @param[in] num_ms The number of miliseconds to delay for
 *
 * @return The status of the creation request. [CY_RSLT_SUCCESS, CY_RTOS_GENERAL_ERROR]
 */
extern cy_rslt_t cy_rtos_delay_milliseconds(cy_time_t num_ms);

/** \} group_abstraction_rtos_functions */

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ifndef INCLUDED_CY_RTOS_INTERFACE_H_ */
