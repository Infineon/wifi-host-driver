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

/** @file
 *  Prototypes of functions for controlling the Wi-Fi system
 *
 *  This file provides prototypes for end-user functions which allow
 *  actions such as scanning for Wi-Fi networks, joining Wi-Fi
 *  networks, getting the MAC address, etc
 *
 */

#include "cybsp.h"
#include "whd.h"
#include "whd_types.h"

#ifndef INCLUDED_WHD_WIFI_API_H
#define INCLUDED_WHD_WIFI_API_H

#ifdef __cplusplus
extern "C"
{
#endif

#define REMAINING_LEN(ptr, type, member) \
	(sizeof(*(ptr)) - offsetof(type, member) - sizeof((ptr)->member))

/******************************************************
*             Function declarations
******************************************************/

/** @addtogroup wifi WHD Wi-Fi API
 *  APIs for controlling the Wi-Fi system
 *  @{
 */

/** @addtogroup wifimanagement WHD Wi-Fi Management API
 *  @ingroup wifi
 *  Initialisation and other management functions for WHD system
 *  @{
 */

/** Initialize an instance of the WHD driver
 *
 *  @param whd_driver_ptr       Pointer to Pointer to handle instance of the driver
 *  @param whd_init_config      Pointer to configuration data that controls how the driver is initialized
 *  @param resource_ops         Pointer to resource interface to provide resources to the driver initialization process
 *  @param buffer_ops           Pointer to a buffer interface to provide buffer related services to the driver instance
 *  @param network_ops          Pointer to a whd_netif_funcs_t to provide network stack services to the driver instance
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_init(whd_driver_t *whd_driver_ptr, whd_init_config_t *whd_init_config,
                         whd_resource_source_t *resource_ops, whd_buffer_funcs_t *buffer_ops,
                         whd_netif_funcs_t *network_ops);
/* @} */
/* @} */

/** @addtogroup busapi WHD Bus API
 * Allows WHD to operate with specific SDIO/SPI bus
 *  @{
 */

#if (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SDIO_INTERFACE) && !defined(COMPONENT_WIFI_INTERFACE_OCI)
/** Attach the WLAN Device to a specific SDIO bus
 *
 *  @param  whd_driver         Pointer to handle instance of the driver
 *  @param  whd_config         Configuration for SDIO bus
 *  @param  sdio_obj           The SDHC hardware interface, from the Level 3 CY HW APIs
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t  whd_bus_sdio_attach(whd_driver_t whd_driver, whd_sdio_config_t *whd_config, whd_sdio_t *sdio_obj);

/** Detach the WLAN Device to a specific SDIO bus
 *
 *  @param  whd_driver         Pointer to handle instance of the driver
 */
extern void whd_bus_sdio_detach(whd_driver_t whd_driver);

#elif (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_SPI_INTERFACE)
/** Attach the WLAN Device to a specific SPI bus
 *
 *  @param  whd_driver        Pointer to handle instance of the driver
 *  @param  whd_config        Configuration for SPI bus
 *  @param  spi_obj           The SPI hardware interface, from the Level 3 CY HW APIs
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t  whd_bus_spi_attach(whd_driver_t whd_driver, whd_spi_config_t *whd_config, whd_spi_t *spi_obj);

/** Detach the WLAN Device to a specific SPI bus
 *
 *  @param  whd_driver         Pointer to handle instance of the driver
 */
extern void whd_bus_spi_detach(whd_driver_t whd_driver);

#elif (CYBSP_WIFI_INTERFACE_TYPE == CYBSP_M2M_INTERFACE)
/** Attach the WLAN Device to M2M bus
 *
 *  @param  whd_driver        Pointer to handle instance of the driver
 *  @param  whd_config        Configuration for M2M bus
 *  @param  m2m_obj           The M2M hardware interface, from the Level 3 CY HW APIs
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t  whd_bus_m2m_attach(whd_driver_t whd_driver, whd_m2m_config_t *whd_config, whd_m2m_t *m2m_obj);

/** Detach the WLAN Device to a specific M2M bus
 *
 *  @param  whd_driver         Pointer to handle instance of the driver
 */
extern void whd_bus_m2m_detach(whd_driver_t whd_driver);

#elif defined(COMPONENT_WIFI_INTERFACE_OCI)
/** Attach the WLAN Device to AXI(OCI) bus
 *
 *  @param  whd_driver        Pointer to handle instance of the driver
 *  @param  whd_config        Configuration for OCI(AXI) bus
 *  @param  oci_obj           The OCI interface, from the Level 3 CY HW APIs
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_bus_oci_attach(whd_driver_t whd_driver, whd_oci_config_t *whd_config);

/** Detach the WLAN Device to a specific AXI(OCI) bus
 *
 *  @param  whd_driver         Pointer to handle instance of the driver
 */
extern void whd_bus_oci_detach(whd_driver_t whd_driver);
#else
error "CYBSP_WIFI_INTERFACE_TYPE or COMPONENT_WIFI_INTERFACE_OCI is not defined"
#endif

/*  @} */

/** @addtogroup wifi WHD Wi-Fi API
 *  APIs for controlling the Wi-Fi system
 *  @{
 */

/** @addtogroup wifimanagement WHD Wi-Fi Management API
 *  @ingroup wifi
 *  Initialisation and other management functions for WHD system
 *  @{
 */

/**
 * Turn on the Wi-Fi device
 *
 *  Initialise Wi-Fi platform
 *  Program various WiFi parameters and modes
 *
 *  @param  whd_driver        Pointer to handle instance of the driver
 *  @param   ifpp             Pointer to Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS if initialization is successful, error code otherwise
 */
extern whd_result_t whd_wifi_on(whd_driver_t whd_driver, whd_interface_t *ifpp);

/**
 * Turn off the Wi-Fi device
 *
 *  - De-Initialises the required parts of the hardware platform
 *    i.e. pins for SDIO/SPI, interrupt, reset, power etc.
 *
 *  - De-Initialises the whd thread which arbitrates access
 *    to the SDIO/SPI bus
 *
 *  @param   ifp              Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS if deinitialization is successful, Error code otherwise
 */
extern whd_result_t whd_wifi_off(whd_interface_t ifp);

/** Shutdown this instance of the wifi driver, freeing all used resources
 *
 *  @param   ifp              Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_deinit(whd_interface_t ifp);

/** Brings up the Wi-Fi core
 *
 *  @param   ifp              Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_up(whd_interface_t ifp);

/** Bring down the Wi-Fi core
 *
 *  WARNING / NOTE:
 *     This brings down the Wi-Fi core and existing network connections will be lost.
 *
 *  @param   ifp               Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_down(whd_interface_t ifp);

/** Creates a secondary interface
 *
 *  @param  whd_drv              Pointer to handle instance of the driver
 *  @param  mac_addr             MAC address for the interface
 *  @param  ifpp                 Pointer to the whd interface pointer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_add_secondary_interface(whd_driver_t whd_drv, whd_mac_t *mac_addr, whd_interface_t *ifpp);
/*  @} */

/** @addtogroup wifijoin   WHD Wi-Fi Join, Scan and Halt API
 *  @ingroup wifi
 *  Wi-Fi APIs for join, scan & leave
 *  @{
 */

/** Scan result callback function pointer type
 *
 * @param result_ptr   A pointer to the pointer that indicates where to put the next scan result
 * @param user_data    User provided data
 * @param status       Status of scan process
 */
typedef void (*whd_scan_result_callback_t)(whd_scan_result_t **result_ptr, void *user_data, whd_scan_status_t status);

/** Initiates a scan to search for 802.11 networks.
 *
 *  This functions returns the scan results with limited sets of parameter in a buffer provided by the caller.
 *  It is also a blocking call. It is an simplified version of the whd_wifi_scan().
 *
 *  @param   ifp                       Pointer to handle instance of whd interface
 *  @param   scan_result               Pointer to user requested records buffer.
 *  @param   count                     Pointer to the no of records user is interested in, and also to the no of record received.
 *
 *  @note    When scanning specific channels, devices with a strong signal strength on nearby channels may be detected
 *
 *  @return  WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_scan_synch(whd_interface_t ifp,
                                    whd_sync_scan_result_t *scan_result,
                                    uint32_t *count
                                    );

/** Initiates a scan to search for 802.11 networks.
 *
 *  The scan progressively accumulates results over time, and may take between 1 and 10 seconds to complete.
 *  The results of the scan will be individually provided to the callback function.
 *  Note: The callback function will be executed in the context of the WHD thread and so must not perform any
 *  actions that may cause a bus transaction.
 *
 *  @param   ifp                       Pointer to handle instance of whd interface
 *  @param   scan_type                 Specifies whether the scan should be Active, Passive or scan Prohibited channels
 *  @param   bss_type                  Specifies whether the scan should search for Infrastructure networks (those using
 *                                     an Access Point), Ad-hoc networks, or both types.
 *  @param   optional_ssid             If this is non-Null, then the scan will only search for networks using the specified SSID.
 *  @param   optional_mac              If this is non-Null, then the scan will only search for networks where
 *                                     the BSSID (MAC address of the Access Point) matches the specified MAC address.
 *  @param   optional_channel_list     If this is non-Null, then the scan will only search for networks on the
 *                                     specified channels - array of channel numbers to search, terminated with a zero
 *  @param   optional_extended_params  If this is non-Null, then the scan will obey the specifications about
 *                                     dwell times and number of probes.
 *  @param   callback                  The callback function which will receive and process the result data.
 *  @param   result_ptr                Pointer to a pointer to a result storage structure.
 *  @param   user_data                 user specific data that will be passed directly to the callback function
 *
 *  @note - When scanning specific channels, devices with a strong signal strength on nearby channels may be detected
 *        - Callback must not use blocking functions, nor use WHD functions, since it is called from the context of the
 *          WHD thread.
 *        - The callback, result_ptr and user_data variables will be referenced after the function returns.
 *          Those variables must remain valid until the scan is complete.
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_scan(whd_interface_t ifp,
                              whd_scan_type_t scan_type,
                              whd_bss_type_t bss_type,
                              const whd_ssid_t *optional_ssid,
                              const whd_mac_t *optional_mac,
                              const uint16_t *optional_channel_list,
                              const whd_scan_extended_params_t *optional_extended_params,
                              whd_scan_result_callback_t callback,
                              whd_scan_result_t *result_ptr,
                              void *user_data);

/** Abort a previously issued scan
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_stop_scan(whd_interface_t ifp);

/** Auth result callback function pointer type
 *
 * @param result_prt   A pointer to the pointer that indicates where to put the auth result
 * @param len          the size of result
 * @param status       Status of auth process
 * @param flag         flag of h2e will be indicated in auth request event, otherwise is NULL.
 * @param user_data    user specific data that will be passed directly to the callback function
 *
 */
typedef void (*whd_auth_result_callback_t)(void *result_ptr, uint32_t len, whd_auth_status_t status, uint8_t *flag,
                                           void *user_data);

/** Initiates SAE auth
 *
 *  The results of the auth will be individually provided to the callback function.
 *  Note: The callback function will be executed in the context of the WHD thread and so must not perform any
 *  actions that may cause a bus transaction.
 *
 *  @param   ifp                       Pointer to handle instance of whd interface
 *  @param   callback                  The callback function which will receive and process the result data.
 *  @param   data                      Pointer to a pointer to a result storage structure.
 *  @param   user_data                 user specific data that will be passed directly to the callback function
 *
 *  @note - Callback must not use blocking functions, nor use WHD functions, since it is called from the context of the
 *          WHD thread.
 *        - The callback, result_ptr and user_data variables will be referenced after the function returns.
 *          Those variables must remain valid until the scan is complete.
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_external_auth_request(whd_interface_t ifp,
                                               whd_auth_result_callback_t callback,
                                               void *result_ptr,
                                               void *user_data);
/** Abort authentication request
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_stop_external_auth_request(whd_interface_t ifp);

/** Joins a Wi-Fi network
 *
 *  Scans for, associates and authenticates with a Wi-Fi network.
 *  On successful return, the system is ready to send data packets.
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   ssid          A null terminated string containing the SSID name of the network to join
 *  @param   auth_type     Authentication type
 *  @param   security_key  A byte array containing either the cleartext security key for WPA/WPA2/WPA3 secured networks
 *  @param   key_length    The length of the security_key in bytes.
 *
 *  @note    In case of WPA3/WPA2 transition mode, the security_key value is WPA3 password.
 *
 *  @return  WHD_SUCCESS   when the system is joined and ready to send data packets
 *           Error code    if an error occurred
 */
extern whd_result_t whd_wifi_join(whd_interface_t ifp, const whd_ssid_t *ssid, whd_security_t auth_type,
                              const uint8_t *security_key, uint8_t key_length);

/** Joins a specific Wi-Fi network
 *
 *  Associates and authenticates with a specific Wi-Fi access point.
 *  On successful return, the system is ready to send data packets.
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   ap            A pointer to a whd_scan_result_t structure containing AP details and
 *                         set ap.channel to 0 for unspecificed channel
 *  @param   security_key  A byte array containing either the cleartext security key for WPA/WPA2
 *                         secured networks
 *  @param   key_length    The length of the security_key in bytes.
 *
 *  @return  WHD_SUCCESS   when the system is joined and ready to send data packets
 *           Error code    if an error occurred
 */
extern whd_result_t whd_wifi_join_specific(whd_interface_t ifp, const whd_scan_result_t *ap, const uint8_t *security_key,
                                       uint8_t key_length);

/** Check whether the platform supports triband or not
 *
 *  @param   ifp      Pointer to handle instance of whd interface
 *
 *  @return  true     if the platform supports triband(6GHz support)
 *           false    if the platform does not support triband
 */
extern whd_bool_t whd_wifi_platform_supports_triband(whd_interface_t ifp);

/** Set the current chanspec on the WLAN radio
 *
 *  @note  On most WLAN devices this will set the chanspec for both AP *AND* STA
 *        (since there is only one radio - it cannot be on two chanspec simulaneously)
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   chanspec      The desired chanspec
 *
 *  @return  WHD_SUCCESS   if the chanspec was successfully set
 *           Error code    if the chanspec was not successfully set
 */
extern whd_result_t whd_wifi_set_chanspec(whd_interface_t ifp, wl_chanspec_t chanspec);

/*  @} */

/** @addtogroup wifiutilities   WHD Wi-Fi Utility API
 *  @ingroup wifi
 *  Allows WHD to perform utility operations
 *  @{
 */

/** Set the current channel on the WLAN radio
 *
 *  @note  On most WLAN devices this will set the channel for both AP *AND* STA
 *        (since there is only one radio - it cannot be on two channels simulaneously)
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   channel       The desired channel
 *
 *  @return  WHD_SUCCESS   if the channel was successfully set
 *           Error code    if the channel was not successfully set
 */
extern whd_result_t whd_wifi_set_channel(whd_interface_t ifp, uint32_t channel);

/** Get the current channel on the WLAN radio
 *
 *  @note On most WLAN devices this will get the channel for both AP *AND* STA
 *       (since there is only one radio - it cannot be on two channels simulaneously)
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   channel        Pointer to receive the current channel
 *
 *  @return  WHD_SUCCESS    if the channel was successfully retrieved
 *           Error code     if the channel was not successfully retrieved
 */
extern whd_result_t whd_wifi_get_channel(whd_interface_t ifp, uint32_t *channel);

/** Gets the supported channels
 *
 *  @param   ifp                 Pointer to handle instance of whd interface
 *  @param   channel_list        Buffer to store list of the supported channels
 *                               and max channel is MAXCHANNEL
 *
 *  @return  WHD_SUCCESS         if the active connections was successfully read
 *           WHD_ERROR           if the active connections was not successfully read
 */
extern whd_result_t whd_wifi_get_channels(whd_interface_t ifp, whd_list_t *channel_list);


/** Set the passphrase
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   security_key   The security key (passphrase) which is to be set
 *  @param   key_length     length of the key
 *
 *  @return  WHD_SUCCESS    when the key is set
 *           Error code     if an error occurred
 */
extern whd_result_t whd_wifi_set_passphrase(whd_interface_t ifp, const uint8_t *security_key, uint8_t key_length);

/** Set the SAE password
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   security_key   The security key (password) which is to be set
 *  @param   key_length     length of the key
 *
 *  @return  WHD_SUCCESS    when the key is set
 *           Error code     if an error occurred
 */
extern whd_result_t whd_wifi_sae_password(whd_interface_t ifp, const uint8_t *security_key, uint8_t key_length);

/** Set the offload configuration
 *
 * @param   ifp            Pointer to handle instance of whd interface
 * @param   ol_feat        Offload Skip bitmap
 * @param   reset          reset or set configuration
 *
 * @return  WHD_SUCCESS    when the offload config is set/reset
 *          Error code     if an error occurred
 */
extern whd_result_t whd_wifi_offload_config(whd_interface_t ifp, uint32_t ol_feat, uint32_t reset);

/** Update IPV4 address
 *
 * @param  ifp            Pointer to handle instance of whd interface
 * @param  ol_feat        Offload Skip bitmap
 * @param  is_add         To add or delete IPV4 address
 *
 * @return WHD_SUCCESS    when the ipv4 address updated or not
 *         Error code     if an error occurred
 */
extern whd_result_t whd_wifi_offload_ipv4_update(whd_interface_t ifp, uint32_t ol_feat, uint32_t ipv4_addr, whd_bool_t is_add);

/** Update IPV6 address
 *
 * @param  ifp            Pointer to handle instance of whd interface
 * @param  ol_feat        Offload Skip bitmap
 * @param  is_add         To add or delete IPV6 address
 *
 * @return WHD_SUCCESS    when the ipv6 address updated or not
 *         Error code     if an error occurred
 */
extern whd_result_t whd_wifi_offload_ipv6_update(whd_interface_t ifp, uint32_t ol_feat, uint32_t *ipv6_addr, uint8_t type, whd_bool_t is_add);

/** Enable the offload module
 *
 * @param   ifp            Pointer to handle instance of whd interface
 * @param   ol_feat        Offload Skip bitmap
 * @param   enable         Enable/Disable offload module
 *
 * @return  WHD_SUCCESS    when offload module enabled or not
 * 	    Error code     if an error occurred
 */
extern whd_result_t whd_wifi_offload_enable(whd_interface_t ifp, uint32_t ol_feat, uint32_t enable);

/** Configure the offload module
 *
 * @param   ifp            Pointer to handle instance of whd interface
 * @param   set_wowl       value indicates, which are all wowl bits to be set
 *
 * @return  WHD_SUCCESS    when offload module enabled or not
 *          Error code     if an error occurred
 */
extern whd_result_t whd_configure_wowl(whd_interface_t ifp, uint32_t set_wowl);

/** Configure the Keep Alive offload module
 *
 * @param   ifp            Pointer to handle instance of whd interface
 * @param   packet         whd period,len_bytes & Data parameter structure
 * @param   flag           Flag to set NULL(0)/NAT(1) keepalive
 *
 * @return  WHD_SUCCESS    when offload module enabled or not
 *          Error code     if an error occurred
 */
extern whd_result_t whd_wifi_keepalive_config(whd_interface_t ifp, whd_keep_alive_t * packet, uint8_t flag);

/** Configure the TKO offload module
 *
 * @param   ifp            Pointer to handle instance of whd interface
 * @param   interval       How often to send (in seconds)
 * @param   retry_inerval  Max times to retry if original fails
 * @param   retry_count    Wait time between retries (in seconds)
 *
 * @return  WHD_SUCCESS    when offload module enabled or not
 *          Error code     if an error occurred
 */
extern whd_result_t whd_configure_tko_offload(whd_interface_t ifp, whd_bool_t enable);

/** Enable WHD internal supplicant and set WPA2 passphrase in case of WPA3/WPA2 transition mode
 *
 *  @param   ifp                Pointer to handle instance of whd interface
 *  @param   security_key_psk   The security key (passphrase) which is to be set
 *  @param   psk_length         length of the key
 *  @param   auth_type          Authentication type: @ref WHD_SECURITY_WPA3_WPA2_PSK
 *
 *  @return  WHD_SUCCESS        when the supplicant variable and wpa2 passphrase is set
 *           Error code         if an error occurred
 */
extern whd_result_t whd_wifi_enable_sup_set_passphrase(whd_interface_t ifp, const uint8_t *security_key_psk,
                                                   uint8_t psk_length, whd_security_t auth_type);

/** Set the PMK Key
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   security_key   The security key (PMK) which is to be set
 *  @param   key_length     length of the PMK(It must be 32 bytes)
 *
 *  @return  WHD_SUCCESS    when the key is set
 *           Error code     if an error occurred
 */
extern whd_result_t whd_wifi_set_pmk(whd_interface_t ifp, const uint8_t *security_key, uint8_t key_length);

/** Set the Roam time threshold
 *
 *  @param ifp                  Pointer to handle instance of whd interface
 *  @param roam_time_threshold  The maximum roam time threshold which is to be set
 *
 *  @return  WHD_SUCCESS    when the roam_time_threshold is set
 *           Error code     if an error occurred
 */
extern whd_result_t whd_wifi_set_roam_time_threshold(whd_interface_t ifp, uint32_t roam_time_threshold);

/** Enable WHD internal supplicant
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   auth_type      Authentication type
 *
 *  @return  WHD_SUCCESS    when the supplicant variable is set
 *           Error code     if an error occurred
 */
extern whd_result_t whd_wifi_enable_supplicant(whd_interface_t ifp, whd_security_t auth_type);

/** Set PMKID in Device (WLAN)
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *  @param   pmkid          Pointer to BSSID and PMKID(16 bytes)
 *
 *  @return whd_result_t
 */
extern whd_result_t whd_wifi_set_pmksa(whd_interface_t ifp, const pmkid_t *pmkid);

/** Clear all PMKIDs in Device (WLAN), especially the PMKIDs in Supplicant module
 *
 *  @param   ifp            Pointer to handle instance of whd interface
 *
 *  @return whd_result_t
 */
extern whd_result_t whd_wifi_pmkid_clear(whd_interface_t ifp);

/** Retrieve the latest RSSI value
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   rssi          The location where the RSSI value will be stored
 *
 *  @return  WHD_SUCCESS   if the RSSI was successfully retrieved
 *           Error code    if the RSSI was not retrieved
 */
extern whd_result_t whd_wifi_get_rssi(whd_interface_t ifp, int32_t *rssi);

/** Retrieve the latest Roam time threshold value
 *
 *  @param   ifp                  Pointer to handle instance of whd interface
 *  @param   roam_time_threshold  The location where the roam time threshold value will be stored
 *
 *  @return  WHD_SUCCESS   if the roam time threshold was successfully retrieved
 *           Error code    if the roam time threshold was not retrieved
 */
extern whd_result_t whd_wifi_get_roam_time_threshold(whd_interface_t ifp, uint32_t *roam_time_threshold);

/** Retrieve the associated STA's RSSI value
 *
 *  @param   ifp          : Pointer to handle instance of whd interface
 *  @param   rssi         : The location where the RSSI value will be stored
 *  @param   client_mac   : Pointer to associated client's MAC address
 *
 *  @return  WHD_SUCCESS  : if the RSSI was successfully retrieved
 *           Error code   : if the RSSI was not retrieved
 */
extern whd_result_t whd_wifi_get_ap_client_rssi(whd_interface_t ifp, int32_t *rssi, const whd_mac_t *client_mac);


/* @} */

/** @addtogroup wifijoin   WHD Wi-Fi Join, Scan and Halt API
 *  @ingroup wifi
 *  Wi-Fi APIs for join, scan & leave
 *  @{
 */
/** Disassociates from a Wi-Fi network.
 *  Applicable only for STA role
 *
 *  @param   ifp           Pointer to handle instance of whd interface.
 *
 *  @return  WHD_SUCCESS   On successful disassociation from the AP
 *           Error code    If an error occurred
 */
extern whd_result_t whd_wifi_leave(whd_interface_t ifp);
/* @} */

/** @addtogroup wifiutilities   WHD Wi-Fi Utility API
 *  @ingroup wifi
 *  Allows WHD to perform utility operations
 *  @{
 */

/** Retrieves the current Media Access Control (MAC) address
 *  (or Ethernet hardware address) of the 802.11 device
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   mac           Pointer to a variable that the current MAC address will be written to
 *
 *  @return  WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_mac_address(whd_interface_t ifp, whd_mac_t *mac);

/** Get the BSSID of the interface
 *
 *  @param  ifp           Pointer to the whd_interface_t
 *  @param  bssid         Returns the BSSID address (mac address) if associated
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_bssid(whd_interface_t ifp, whd_mac_t *bssid);
/* @} */

/** @addtogroup wifisoftap     WHD Wi-Fi SoftAP API
 *  @ingroup wifi
 *  Wi-Fi APIs to perform SoftAP related functionalities
 *  @{
 */

/** Initialises an infrastructure WiFi network (SoftAP)
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   ssid          A null terminated string containing the SSID name of the network to join
 *  @param   auth_type     Authentication type
 *  @param   security_key  A byte array containing the cleartext security key for the network
 *  @param   key_length    The length of the security_key in bytes.
 *  @param   chanspec      The desired chanspec
 *
 *  @return  WHD_SUCCESS   if successfully initialises an AP
 *           Error code    if an error occurred
 */
extern whd_result_t whd_wifi_init_ap(whd_interface_t ifp, whd_ssid_t *ssid, whd_security_t auth_type,
                                 const uint8_t *security_key, uint8_t key_length, uint16_t chanspec);

/** Start the infrastructure WiFi network (SoftAP)
 *  using the parameter set by whd_wifi_init_ap() and optionaly by whd_wifi_manage_custom_ie()
 *
 *  @return  WHD_SUCCESS   if successfully creates an AP
 *           Error code    if an error occurred
 */
extern whd_result_t whd_wifi_start_ap(whd_interface_t ifp);

/** Stops an existing infrastructure WiFi network
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *
 *  @return  WHD_SUCCESS   if the AP is successfully stopped or if the AP has not yet been brought up
 *           Error code    if an error occurred
 */
extern whd_result_t whd_wifi_stop_ap(whd_interface_t ifp);


/** Get the maximum number of associations supported by AP interfaces
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   max_assoc     The maximum number of associations supported by Soft AP interfaces.
 *
 *  @return  WHD_SUCCESS   if the maximum number of associated clients was successfully read
 *           WHD_ERROR     if the maximum number of associated clients was not successfully read
 */
extern whd_result_t whd_wifi_ap_get_max_assoc(whd_interface_t ifp, uint32_t *max_assoc);

/** Get the maximum number of associations supported by AP interfaces
 *
 *  @param   ifp           Pointer to handle instance of whd interface
 *  @param   max_assoc     The maximum number of clients supported by Soft AP interfaces.
 *
 *  @return  WHD_SUCCESS   if the maximum number of associated clients was successfully read
 *           WHD_ERROR     if the maximum number of associated clients was not successfully read
 */
extern whd_result_t whd_wifi_ap_set_max_assoc(whd_interface_t ifp, uint32_t max_assoc);

/** Gets the current number of active connections
 *
 *  @param   ifp                 Pointer to handle instance of whd interface
 *  @param   client_list_buffer  Buffer to store list of associated clients
 *  @param   buffer_length       Length of client list buffer
 *
 *  @return  WHD_SUCCESS         if the active connections was successfully read
 *           WHD_ERROR           if the active connections was not successfully read
 */
extern whd_result_t whd_wifi_get_associated_client_list(whd_interface_t ifp, void *client_list_buffer,
                                                    uint16_t buffer_length);

/** Deauthenticates a STA which may or may not be associated to SoftAP
 *
 * @param   ifp             Pointer to handle instance of whd interface
 * @param   mac             Pointer to a variable containing the MAC address to which the deauthentication will be sent
 *                          NULL mac address will deauthenticate all the associated STAs
 *
 * @param   reason          Deauthentication reason code
 *
 * @return  WHD_SUCCESS     On successful deauthentication of the other STA
 *          WHD_ERROR       If an error occurred
 */
extern whd_result_t whd_wifi_deauth_sta(whd_interface_t ifp, whd_mac_t *mac, whd_dot11_reason_code_t reason);

/** Retrieves AP information
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  ap_info        Returns a whd_bss_info_t structure containing AP details
 *  @param  security       Authentication type
 *
 *  @return WHD_SUCCESS    if the AP info was successfully retrieved
 *          Error code     if the AP info was not successfully retrieved
 */
extern whd_result_t whd_wifi_get_ap_info(whd_interface_t ifp, whd_bss_info_t *ap_info, whd_security_t *security);

/** Set the beacon interval.
 *
 *  Note that the value needs to be set before ap_start in order to beacon interval to take effect.
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  interval       Beacon interval in time units (Default: 100 time units = 102.4 ms)
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_ap_set_beacon_interval(whd_interface_t ifp, uint16_t interval);

/** Set the DTIM interval.
 *
 *  Note that the value needs to be set before ap_start in order to DTIM interval to take effect.
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  interval       DTIM interval, in unit of beacon interval
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_ap_set_dtim_interval(whd_interface_t ifp, uint16_t interval);
/*  @} */


/** @addtogroup wifipowersave   WHD Wi-Fi Power Save API
 *  @ingroup wifi
 *  Wi-Fi functions for WLAN low power modes
 *  @{
 */

/** Enables powersave mode on specified interface without regard for throughput reduction
 *
 *  This function enables (legacy) 802.11 PS-Poll mode and should be used
 *  to achieve the lowest power consumption possible when the Wi-Fi device
 *  is primarily passively listening to the network
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_enable_powersave(whd_interface_t ifp);

/** Enables powersave mode on specified interface while attempting to maximise throughput
 *
 *
 *  Network traffic is typically bursty. Reception of a packet often means that another
 *  packet will be received shortly afterwards (and vice versa for transmit).
 *
 *  In high throughput powersave mode, rather then entering powersave mode immediately
 *  after receiving or sending a packet, the WLAN chip waits for a timeout period before
 *  returning to sleep.
 *
 *  @param   ifp                    Pointer to handle instance of whd interface
 *  @param   return_to_sleep_delay  The variable to set return to sleep delay.
 *                                 return to sleep delay must be set to a multiple of 10 and not equal to zero.
 *
 *  @return  WHD_SUCCESS            if power save mode was successfully enabled
 *           Error code             if power save mode was not successfully enabled
 *
 */
extern whd_result_t whd_wifi_enable_powersave_with_throughput(whd_interface_t ifp, uint16_t return_to_sleep_delay);

/** Get powersave mode on specified interface
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  value          Value of the current powersave state
 *                          PM1_POWERSAVE_MODE, PM2_POWERSAVE_MODE, NO_POWERSAVE_MODE
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_powersave_mode(whd_interface_t ifp, uint32_t *value);

/** Disables 802.11 power save mode on specified interface
 *
 *  @param   ifp               Pointer to handle instance of whd interface
 *
 *  @return  WHD_SUCCESS       if power save mode was successfully disabled
 *           Error code        if power save mode was not successfully disabled
 *
 */
extern whd_result_t whd_wifi_disable_powersave(whd_interface_t ifp);

/** Configure ULP mode on specified interface
 *
 *  @param   ifp               Pointer to handle instance of whd interface
 *  @param   mode              mode to be set for ULP(DS0/DS1/DS2)
                               1 for DS1, 2 for DS2 and 0 indicates to disable(DS0)
 *  @param   wait_time         indicates ulp_wait in ms to be set
                               (if no network activity for this time, device will enter into DS2)
 *
 *  @return  WHD_SUCCESS       if ulp mode was successfully configured
 *           Error code        if ulp mode was not configured successfully
 *
 */
extern whd_result_t whd_wifi_config_ulp_mode(whd_interface_t ifp, uint32_t *mode, uint32_t *wait_time);

/* @} */

/** @addtogroup wifiutilities   WHD Wi-Fi Utility API
 *  @ingroup wifi
 *  Allows WHD to perform utility operations
 *  @{
 */
/** Registers interest in a multicast address
 *
 *  Once a multicast address has been registered, all packets detected on the
 *  medium destined for that address are forwarded to the host.
 *  Otherwise they are ignored.
 *
 *  @param  ifp              Pointer to handle instance of whd interface
 *  @param  mac              Ethernet MAC address
 *
 *  @return WHD_SUCCESS      if the address was registered successfully
 *          Error code       if the address was not registered
 */
extern whd_result_t whd_wifi_register_multicast_address(whd_interface_t ifp, const whd_mac_t *mac);

/** Unregisters interest in a multicast address
 *
 *  Once a multicast address has been unregistered, all packets detected on the
 *  medium destined for that address are ignored.
 *
 *  @param  ifp              Pointer to handle instance of whd interface
 *  @param  mac              Ethernet MAC address
 *
 *  @return WHD_SUCCESS      if the address was unregistered successfully
 *          Error code       if the address was not unregistered
 */
extern whd_result_t whd_wifi_unregister_multicast_address(whd_interface_t ifp, const whd_mac_t *mac);

/** Sets the 802.11 powersave listen interval for a Wi-Fi client, and communicates
 *  the listen interval to the Access Point. The listen interval will be set to
 *  (listen_interval x time_unit) seconds.
 *
 *  The default value for the listen interval is 0. With the default value of 0 set,
 *  the Wi-Fi device wakes to listen for AP beacons every DTIM period.
 *
 *  If the DTIM listen interval is non-zero, the DTIM listen interval will over ride
 *  the beacon listen interval value.
 *
 *  @param  ifp              Pointer to handle instance of whd interface
 *  @param  listen_interval  The desired beacon listen interval
 *  @param  time_unit        The listen interval time unit; options are beacon period or DTIM period.
 *
 *  @return WHD_SUCCESS      If the listen interval was successfully set.
 *          Error code       If the listen interval was not successfully set.
 */
extern whd_result_t whd_wifi_set_listen_interval(whd_interface_t ifp, uint8_t listen_interval,
                                             whd_listen_interval_time_unit_t time_unit);

/** Gets the current value of all beacon listen interval variables
 *
 *  @param  ifp                     Pointer to handle instance of whd interface
 *  @param  li                      Powersave listen interval values
 *                                     - listen_interval_beacon : The current value of the listen interval set as a multiple of the beacon period
 *                                     - listen_interval_dtim   : The current value of the listen interval set as a multiple of the DTIM period
 *                                     - listen_interval_assoc  : The current value of the listen interval sent to access points in an association request frame
 *
 *  @return WHD_SUCCESS             If all listen interval values are read successfully
 *          Error code              If at least one of the listen interval values are NOT read successfully
 */
extern whd_result_t whd_wifi_get_listen_interval(whd_interface_t ifp, whd_listen_interval_t *li);

/** Determines if a particular interface is ready to transceive ethernet packets
 *
 *  @param     ifp                    Pointer to handle instance of whd interface
 *
 *  @return    WHD_SUCCESS            if the interface is ready to transceive ethernet packets
 *             WHD_NOTFOUND           no AP with a matching SSID was found
 *             WHD_NOT_AUTHENTICATED  Matching AP was found but it won't let you authenticate.
 *                                    This can occur if this device is in the block list on the AP.
 *             WHD_NOT_KEYED          Device has authenticated and associated but has not completed the key exchange.
 *                                    This can occur if the passphrase is incorrect.
 *             Error code             if the interface is not ready to transceive ethernet packets
 */
extern whd_result_t whd_wifi_is_ready_to_transceive(whd_interface_t ifp);

/* Certification APIs */

/** Retrieve the latest STA EDCF AC parameters
 *
 *  Retrieve the latest Station (STA) interface EDCF (Enhanced Distributed
 *  Coordination Function) Access Category parameters
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  acp            The location where the array of AC parameters will be stored
 *
 *  @return  WHD_SUCCESS   if the AC Parameters were successfully retrieved
 *           Error code    if the AC Parameters were not retrieved
 */
extern whd_result_t whd_wifi_get_acparams(whd_interface_t ifp, whd_edcf_ac_param_t *acp);

/* Action Frames */

/** Manage the addition and removal of custom IEs
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  action         the action to take (add or remove IE)
 *  @param  oui            the oui of the custom IE
 *  @param  subtype        the IE sub-type
 *  @param  data           a pointer to the buffer that hold the custom IE
 *  @param  length         the length of the buffer pointed to by 'data'
 *  @param  which_packets  A mask to indicate in which all packets this IE should be included. See whd_ie_packet_flag_t.
 *
 *  @return WHD_SUCCESS    if the custom IE action was successful
 *          Error code     if the custom IE action failed
 */
extern whd_result_t whd_wifi_manage_custom_ie(whd_interface_t ifp, whd_custom_ie_action_t action,
                                          const uint8_t *oui, uint8_t subtype, const void *data,
                                          uint16_t length, uint16_t which_packets);

/** Send a pre-prepared action frame
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  af_params      A pointer to a pre-prepared action frame structure
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_send_action_frame(whd_interface_t ifp, whd_af_params_t *af_params);

/** Send a pre-prepared authentication frame
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  auth_params    pointer to a pre-prepared authentication frame structure
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_send_auth_frame(whd_interface_t ifp, whd_auth_params_t *auth_params);

/** Configure HE OM Control
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  he_omi_params  pointer to he_omi parameters
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_he_omi(whd_interface_t ifp, whd_he_omi_params_t *he_omi_params);

/** Configure BSS Max Idle Time
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  period         The bss max idle period time unit(seconds)
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_bss_max_idle(whd_interface_t ifp, uint16_t period);

/** Trigger individual TWT session(used by STA)
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  twt_params     pointer to itwt_setup parameters
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_itwt_setup(whd_interface_t ifp, whd_itwt_setup_params_t *twt_params);

/** Trigger Join broadcast TWT session(used by STA)
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  twt_params     pointer to btwt_join parameters
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_btwt_join(whd_interface_t ifp, whd_btwt_join_params_t *twt_params);

/** Trigger teardown all individual or broadcast TWT sessions
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  twt_params     pointer to twt_taerdown parameters
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_twt_teardown(whd_interface_t ifp, whd_twt_teardown_params_t *twt_params);

/** Trigger TWT information frame(used by STA)
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  twt_params     pointer to twt_information parameters
 *
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_twt_information_frame(whd_interface_t ifp, whd_twt_information_params_t *twt_params);

/** Configure TWT IE in beacon(used by AP)
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  twt_params     pointer to btwt_config parameters
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_btwt_config(whd_interface_t ifp, whd_btwt_config_params_t *twt_params);

/** Enable OCE Optimized Connectivity Experience
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  whd_bool_t     value to 'WHD_TRUE' or 'WHD_FALSE'
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_oce_enable(whd_interface_t ifp, whd_bool_t value);

/** Get HE Bss Color
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  value          get value of color
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_he_color(whd_interface_t ifp, uint8_t *value);

/** Set BSS Color
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  value          value from 0-255
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_he_color(whd_interface_t ifp, uint8_t *value);

/** Add MBO preffered/non-prefferd channel attributes
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  mbo_params     pointer to whd_mbo_add_chan_pref_params_t structure
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_mbo_add_chan_pref(whd_interface_t ifp, whd_mbo_add_chan_pref_params_t *mbo_params);

/** Delete MBO preffered/non-prefferd channel attributes
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  mbo_params     pointer to whd_mbo_del_chan_pref_params_t structure
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_mbo_del_chan_pref(whd_interface_t ifp, whd_mbo_del_chan_pref_params_t *mbo_params);

/** Send WNM Notification request sub-element type
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  sub_elem_type  sub-element type <2/3>
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_mbo_send_notif(whd_interface_t ifp, uint8_t sub_elem_type);

/** Set coex configuration
 *
 *  @param  ifp                  Pointer to handle instance of whd interface
 *  @param  coex_config          Pointer to the structure whd_coex_config_t
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_coex_config(whd_interface_t ifp, whd_coex_config_t *coex_config);

/** Set auth status used for External AUTH(SAE)
 *
 *  @param   ifp                    Pointer to handle instance of whd interface
 *  @param   whd_auth_req_status    Pointer to Auth_Status structure
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_auth_status(whd_interface_t ifp, whd_auth_req_status_t *params);

/** Get FW(chip) Capability
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  value          Enum value of the current FW capability, ex: sae or sae_ext or ...etc,
 *                         (enum value map to whd_fwcap_id_t)
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_fwcap(whd_interface_t ifp, uint32_t *value);

/** Get version of Device (WLAN) Firmware
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[out]   version        : pointer to store version #
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_version(whd_interface_t ifp, uint32_t *version);

/** Get ARP Offload Peer Age from Device (WLAN)
 *    Length of time in seconds before aging out an entry in the WLAN processor ARP table.
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[out]   seconds        : pointer to store value
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_peerage_get(whd_interface_t ifp, uint32_t *seconds);

/** Set ARP Offload Peer Age in Device (WLAN)
 *    Length of time in seconds before aging out an entry in the WLAN processor ARP table.
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[in]    seconds        : Seconds to age out IP
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_peerage_set(whd_interface_t ifp, uint32_t seconds);

/** Get ARP Offload Agent Enable from Device (WLAN)
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[out]   agent_enable   : pointer to store value
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_arpoe_get(whd_interface_t ifp, uint32_t *agent_enable);

/** Set ARP Offload Agent Enable in Device (WLAN)
 *    Set Enable/Disable of ARP Offload Agent
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[in]    agent_enable   : Enable=1 / Disable=0
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_arpoe_set(whd_interface_t ifp, uint32_t agent_enable);

/** Clear ARP Offload cache in Device (WLAN)
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_cache_clear(whd_interface_t ifp);

/** Get ARP Offload Feature Flags from Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[out]   features   : ptr to store currently set features - bit flags CY_ARP_OL_AGENT_ENABLE, etc.
 *                            ARL_OL_AGENT | ARL_OL_SNOOP | ARP_OL_HOST_AUTO_REPLY | ARP_OL_PEER_AUTO_REPLY
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_features_get(whd_interface_t ifp, uint32_t *features);

/** Set ARP Offload Feature Flags in Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    features   : features to set value (you can OR ('|') multiple flags) CY_ARP_OL_AGENT_ENABLE, etc.
 *                           ARL_OL_AGENT | ARL_OL_SNOOP | ARP_OL_HOST_AUTO_REPLY | ARP_OL_PEER_AUTO_REPLY
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_features_set(whd_interface_t ifp, uint32_t features);

/** Print ARP Offload Feature Flags in Human readable form to console
 *
 * @param[in]    features   : feature flags to set (you can OR '|' multiple flags) CY_ARP_OL_AGENT_ENABLE, etc.
 *                            ARL_OL_AGENT | ARL_OL_SNOOP | ARP_OL_HOST_AUTO_REPLY | ARP_OL_PEER_AUTO_REPLY
 * @param[in]    title      : Optional: Title for output (NULL == no title)
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_features_print(uint32_t features, const char *title);

/** Add ARP Offload Host IP Identifier(s) to HostIP List to Device (WLAN)
 *
 * @param[in]    ifp            : pointer to handle instance of whd interface
 * @param[in]    host_ipv4_list : pointer to host_ip data (IPv4, 1 uint32_t per ip addr)
 * @param[in]    count          : Number of array elements in host_ip
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_add(whd_interface_t ifp, uint32_t *host_ipv4_list, uint32_t count);

/** Add One ARP Offload Host IP Identifier to HostIP List (mbed-style IP string) to Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    ip_addr    : pointer to ip string (as returned from mbedos calls)
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_add_string(whd_interface_t ifp, const char *ip_addr);

/** Clear One ARP Offload Host IP Identifier from Host IP List in Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    ipv4_addr  : ip addr expressed as a uint32_t
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_clear_id(whd_interface_t ifp, uint32_t ipv4_addr);

/** Clear One ARP Offload Host IP Identifier from Host IP List (mbed-style IP string) in Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    ip_addr    : pointer to ip string (as returned from mbedos calls)
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_clear_id_string(whd_interface_t ifp, const char *ip_addr);

/** Clear all ARP Offload Host IP Identifier List
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_clear(whd_interface_t ifp);

/** Get ARP Offload Host IP Identifiers from Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    count          : Number of array elements in host_ip
 * @param[out]   host_ipv4_list : Pointer to structure array to store host_ip data
 * @param[out]   filled         : Number of array elements filled by this routine
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_hostip_list_get(whd_interface_t ifp, uint32_t count, uint32_t *host_ipv4_list, uint32_t *filled);

/** Clear ARP Offload statistics in Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_stats_clear(whd_interface_t ifp);

/** Get ARP Offload statistics from Device (WLAN)
 *
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[out]   stats      : Ptr to store statistics whd_arp_stats_t
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_stats_get(whd_interface_t ifp, whd_arp_stats_t *stats);

/** Print ARP Offload statistics
 *  NOTE: call whd_arp_stats_get(), then print them using this function.
 *
 * @param[in]    arp_stats  : Ptr to ARP statistics structure
 * @param[in]    title      : Optional: Title for output (NULL == no title)
 *
 * @return whd_result_t
 */
whd_result_t whd_arp_stats_print(whd_arp_stats_t *arp_stats, const char *title);

/** A filter must be added (e.g. created) before it can be enabled.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    settings   : Ptr to filter settings @ref whd_packet_filter_t
 * @return whd_result_t
 */
whd_result_t whd_pf_add_packet_filter(whd_interface_t ifp, const whd_packet_filter_t *settings);

/** Remove a previously added filter.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : filter to remove
 * @return whd_result_t
 */
whd_result_t whd_pf_remove_packet_filter(whd_interface_t ifp, uint8_t filter_id);

/** After a filter has been added it can be enabled or disabled as needed.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : filter to enable
 * @return whd_result_t
 */
whd_result_t whd_pf_enable_packet_filter(whd_interface_t ifp, uint8_t filter_id);

/** After a filter has been added it can be enabled or disabled as needed.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : filter to disable
 * @return whd_result_t
 */
whd_result_t whd_pf_disable_packet_filter(whd_interface_t ifp, uint8_t filter_id);

/** After a filter has been added it can be enabled or disabled as needed.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : filter to disable/enable
 * @param[in]    enable     : Enable/Disable Flag
 * @return whd_result_t
 */
whd_result_t whd_wifi_toggle_packet_filter(whd_interface_t ifp, uint8_t filter_id, whd_bool_t enable);

/** Filters are implemented in WLAN subsystem as a bit pattern and matching bit mask that
 *  are applied to incoming packets.  This API retrieves the pattern and mask.
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : which filter to retrieve
 * @param[in]    max_size   : size of both mask and pattern buffers
 * @param[out]   mask       : mask for this filter
 * @param[out]   pattern    : pattern for this filter
 * @param[out]   size_out   : length of both mask and pattern
 * @return whd_result_t
 */
whd_result_t whd_pf_get_packet_filter_mask_and_pattern(whd_interface_t ifp, uint8_t filter_id, uint32_t max_size,
                                                       uint8_t *mask,
                                                       uint8_t *pattern, uint32_t *size_out);

/** Clear the packet filter stats associated with a filter id
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : which filter
 * @return whd_result_t
 */
whd_result_t whd_wifi_clear_packet_filter_stats(whd_interface_t ifp, uint32_t filter_id);

/** Return the stats associated with a filter
 * @param[in]    ifp        : pointer to handle instance of whd interface
 * @param[in]    filter_id  : which filter
 * @param[out]   stats      : Ptr to store statistics wl_pkt_filter_stats_t
 * @return whd_result_t
 */
whd_result_t whd_pf_get_packet_filter_stats(whd_interface_t ifp, uint8_t filter_id, whd_pkt_filter_stats_t *stats);

/** Set/Get TKO retry & interval parameters
 * @param[in]    ifp            : Pointer to handle instance of whd interface
 * @param[in]    whd_tko_retry  : whd retry & interval parameters structure
 * @param[in]    set            : Set(1)/Get(0) Flag
 * @return whd_result_t
 */
whd_result_t whd_tko_param(whd_interface_t ifp, whd_tko_retry_t *whd_tko_retry, uint8_t set);

/** Return the tko status for all indexes
 * @param[in]    ifp        : Pointer to handle instance of whd interface
 * @param[out]   tko_status : Ptr to store tko_status
 * @return whd_result_t
 */
whd_result_t whd_tko_get_status(whd_interface_t ifp, whd_tko_status_t *tko_status);

/** Return the stats associated with a filter
 * @param[in]    ifp        : Pointer to handle instance of whd interface
 * @param[out]   max        : returns Max TCP connections supported by WLAN Firmware
 * @return whd_result_t
 */
whd_result_t whd_tko_max_assoc(whd_interface_t ifp, uint8_t *max);

/** Return the stats associated with a filter
 * @param[in]    ifp          : Pointer to handle instance of whd interface
 * @param[in]    index        : index for TCP offload connection
 * @param[out]   whd_connect  : tko_connect structure buffer from Firmware
 * @param[in]    buflen       : Buffer given for tko_connect
 * @return whd_result_t
 */
whd_result_t whd_tko_get_FW_connect(whd_interface_t ifp, uint8_t index, whd_tko_connect_t *whd_connect,
                                    uint16_t buflen);

/** Return the stats associated with a filter
 * @param[in]    ifp        : Pointer to handle instance of whd interface
 * @param[in]    enable     : Enable/Disable TCP Keepalive offload
 * @return whd_result_t
 */
whd_result_t whd_tko_toggle(whd_interface_t ifp, whd_bool_t enable);


/* @} */

/** @addtogroup wifiioctl   WHD Wi-Fi IOCTL Set/Get API
 *  @ingroup wifi
 *  Set and get IOCTL values
 *  @{
 */
/** Sends an IOCTL command - CDC_SET IOCTL value
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  ioctl          CDC_SET - To set the I/O control
 *  @param  value          Data value to be sent
 *
 *  @return WHD_SUCCESS or Error code
 */

/** Return the stats associated with a filter
 * @param        ifp          : Pointer to handle instance of whd interface
 * @param        whd_filter   : wl_filter structure buffer from Firmware
 * @param        set          : Set(1)/Get(0) Flag
 * @return whd_result_t
 */
extern whd_result_t whd_tko_filter(whd_interface_t ifp, whd_tko_auto_filter_t * whd_filter, uint8_t set);

extern whd_result_t whd_wifi_set_ioctl_value(whd_interface_t ifp, uint32_t ioctl, uint32_t value);

/** Sends an IOCTL command - CDC_GET IOCTL value
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  ioctl          CDC_GET - To get the I/O control
 *  @param  value          Pointer to receive the data value
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_ioctl_value(whd_interface_t ifp, uint32_t ioctl, uint32_t *value);

/** Sends an IOCTL command - CDC_SET IOCTL buffer
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  ioctl          CDC_SET - To set the I/O control
 *  @param  buffer         Handle for a packet buffer containing the data value to be sent.
 *  @param  buffer_length  Length of buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_ioctl_buffer(whd_interface_t ifp, uint32_t ioctl, void *buffer, uint16_t buffer_length);

/** Sends an IOCTL command - CDC_GET IOCTL buffer
 *
 *  @param  ifp           Pointer to handle instance of whd interface
 *  @param  ioctl         CDC_GET - To get the I/O control
 *  @param  out_buffer    Pointer to receive the handle for the packet buffer containing the response data value received
 *  @param  out_length    Length of out_buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_ioctl_buffer(whd_interface_t ifp, uint32_t ioctl, uint8_t *out_buffer,
                                          uint16_t out_length);

/** Sends an IOVAR command
 *
 *  @param  ifp           Pointer to handle instance of whd interface
 *  @param  iovar_name    SDPCM_GET - To get the I/O Variable
 *  @param  param         Paramater to be passed for the IOVAR
 *  @param  paramlen      Paramter length
 *  @param  out_buffer    Pointer to receive the handle for the packet buffer containing the response data value received
 *  @param  out_length    Length of out_buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_iovar_buffer_with_param(whd_interface_t ifp, const char *iovar_name, void *param,
                                                     uint32_t paramlen, uint8_t *out_buffer, uint32_t out_length);

/** Fetches ulp statistics and fills the buffer with that data and executes deepsleep
 *  indication callback if application registers for it
 *
 *  @param whd_driver              Instance of whd driver
 *  @param char *buf               Pointer to buffer to be filled with ulpstats data
 *  @param buflen                  Buffer length of the above buffer
 *                                 should be between 2048 and 4096
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_deepsleep_stats(whd_driver_t whd_driver, char *buf, uint32_t buflen);

/* Function pointer to be used callback registration */
typedef void* (*whd_ds_callback_t)(void*, char*, uint32_t);

/* Structure to store callback registration and parameters */
typedef struct deepsleep_cb_info {
    whd_ds_callback_t callback;
    void* ctx;
    char* buf;
    uint32_t buflen;
} deepsleep_cb_info_t ;

/** @addtogroup dbg  WHD Wi-Fi Debug API
 *  @ingroup wifi
 *  WHD APIs which allows debugging like, printing whd log information, getting whd stats, etc.
 *  @{
 */
/** Retrieves the WLAN firmware version
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  version        Pointer to a buffer that version information will be written to
 *  @param  length         Length of the buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_wifi_version(whd_interface_t ifp, char *version, uint8_t length);

/** Retrieves the WLAN CLM version
 *
 *  @param  ifp            Pointer to handle instance of whd interface
 *  @param  version        Pointer to a buffer that version information will be written to
 *  @param  length         Length of the buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_clm_version(whd_interface_t ifp, char *version, uint8_t length);

/** To print whd log information
 *
 *  @param  whd_drv        Pointer to handle instance of the driver
 *  @param  buffer         Buffer to store read log results
 *  @param  buffer_size    Variable to store size of the buffer
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_read_wlan_log(whd_driver_t whd_drv, char *buffer, uint32_t buffer_size);

/** To print whd log information
 *
 *  @param  whd_drv        Pointer to handle instance of the driver
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_print_whd_log(whd_driver_t whd_drv);

/** Retrieves the ifidx from interface pointer.
 *  ifidx is a unique value and be used to identify a instance of tcp/ip stack
 *
 *  @param  ifp           Pointer to the whd_interface_t
 *  @param  ifidx         Pointer to ifidx
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_network_get_ifidx_from_ifp(whd_interface_t ifp, uint8_t *ifidx);

/** Retrieves the bsscfgidx from interface pointer.
 *
 *  Can be used to send IOCTL with requires bsscfgidx
 *
 *  @param  ifp           Pointer to handle instance of whd interface
 *  @param  bsscfgidx     Pointer to bsscfgidx
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_network_get_bsscfgidx_from_ifp(whd_interface_t ifp, uint8_t *bsscfgidx);


/** Retrives the bss info
 *
 *  @param  ifp                  Pointer to handle instance of whd interface
 *  @param  bi                   A pointer to the structure wl_bss_info_t
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_bss_info(whd_interface_t ifp, wl_bss_info_t *bi);

/** Prints WHD stats
 *
 *  @param  whd_drv              Pointer to handle instance of the driver
 *  @param  reset_after_print    Bool variable to decide if whd_stats to be reset
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_print_stats(whd_driver_t whd_drv, whd_bool_t reset_after_print);

/** Print CR4 TCM bytes
 *
 *  @param  ifp                  Pointer to handle instance of whd interface
 *  @param  offset               offset of TCM from where the data to be read
 *
 *  @return Value read from TCM
 */
extern whd_result_t whd_wifi_get_cr4_tcm_byte(whd_interface_t ifp, uint32_t offset);

/** Enable event logs
 *
 *  @param  ifp                  Pointer to handle instance of whd interface
 *
 *  @param  category             category to be enabled
 *
 *  @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_enable_event_log(whd_interface_t ifp, uint8_t category);

/** Function to register callbacks to be executed
 *
 *  @param ifp                   Pointer to handle instance of whd interface
 *  @param callback              Callback api to be registered
 *  @param ctx                   Pointer to context
 *  @param buf                   Buffer to be filled with data
 *  @param buflen                Buffer length of the above buffer
 *
 *  @return WHD_SUCCESS or WHD_UNKNOWN_INTERFACE
 */
extern whd_result_t whd_wifi_register_ds_callback(whd_interface_t ifp, whd_ds_callback_t callback, void *ctx, char* buf, uint32_t buflen);

/** Prints WHD wifi firmware logs
 *
 * @param ifp                    Pointer to handle instance of whd interface
 *
 * @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_get_fw_logs(whd_interface_t ifp);

/** Set WHD wifi Country code
 *
 * @param ifp                    Pointer to handle instance of whd interface
 * @param country_code           country_code set
 *
 * @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_country_code(whd_interface_t ifp, whd_country_code_t country_code);

#ifdef PROTO_MSGBUF
/**
 * whd_wifi_delete_peers -      Delete/remove the connected clients, during STA leave.
 *
 * @param ifp                    Pointer to handle instance of whd interface
 * @param peer_addr              MAC Address of the Client STA connected to SoftAP
 *
 * @return void                  No error code returns
 */
extern void whd_wifi_delete_peer(whd_interface_t ifp, uint8_t *peer_addr);
#endif /* PROTO_MSGBUF */

#if defined(WHD_CSI_SUPPORT)
/**
 * whd_wifi_csi_events_handler() - Handle the CSI Event notifications from Firmware.
 *
 * @ifp: interface instatnce.
 * @event_header: event message header.
 * @event_data: CSI information
 * @handler_user_data: Semaphore data
 *
 * return: 0 on success, value < 0 on failure.
 */

extern void
*whd_wifi_csi_events_handler(whd_interface_t ifp, const whd_event_header_t *event_header,const uint8_t *event_data, void *handler_user_data);

/**
 * whd_csi_register_handler() - Handler attach function that would attach the handler function.
 *
 * @ifp: interface instatnce.
 * @user_data: Semaphore data
 *
 * return: 0 on success, value < 0 on failure.
 */

extern whd_result_t whd_csi_register_handler(whd_interface_t ifp, void* user_data);

/**
 * whd_csi_register_handler() - Handler attach function that would attach the handler function.
 *
 * @params: Initializes csi struct with default parameters.
 *
 * return: 0 on success, value < 0 on failure.
 */
extern void whd_init_csi_cfg_params(wlc_csi_cfg_t *params);

/**
 * whd_wifi_csi_enable() - Called when user enters csi,enable cmd. Registers the event handler
 *
 * @ifp: interface instatnce.
 * @user_data: Semaphore data
 * @argv: passing the arguments that were provided by user in command console
 * @len: number of arguments that were provided by user in command console
 * @csi_data_cb_func: Callback function that should be used to sendup CSI data
 *
 * return: 0 on success, value < 0 on failure.
 */

extern whd_result_t whd_wifi_csi_enable(whd_interface_t ifp,void* user_data, char* argv[],int len,whd_csi_data_sendup csi_data_cb_func);

/**
 * whd_wifi_csi_disable() - Called when user enters csi,disable cmd. Deregisters the event handler
 *
 * @ifp: interface instance.
 *
 * return: 0 on success, value < 0 on failure.
 */

extern whd_result_t whd_wifi_csi_disable(whd_interface_t ifp);

/**
 * whd_wifi_csi_info() - Dumps the wlc_csi_cfg_t struct from FW.
 *
 * @ifp: interface instance.
 *
 * return: 0 on success, value < 0 on failure.
 */

extern whd_result_t whd_wifi_csi_info(whd_interface_t ifp);

/**
 * whd_wifi_csi() - CSI command handler function that is called from command console.
 *
 * @ifp: interface instatnce.
 * @user_data: Semaphore data
 * @params: passing the arguments that were provided by user in command console
 * @argc: number of arguments that were provided by user in command console
 * @csi_data_cb_func: Callback function that should be used to sendup CSI data
 *
 * return: 0 on success, value < 0 on failure.
 */

extern whd_result_t
whd_wifi_csi(whd_interface_t ifp, void* user_data, char* params[], int argc,whd_csi_data_sendup csi_data_cb_func);

#endif /* defined(WHD_CSI_SUPPORT) */

/** Function to unregister callbacks that are note needed to be executed anymore
 *
 *  @param ifp                   Pointer to handle instance of whd interface
 *  @param callback              Callback api to be registered
 *  @param ctx                   Pointer to context
 *  @param buf                   Buffer to be filled with data
 *  @param buflen                Buffer length of the above buffer
 *
 *  @return WHD_SUCCESS or WHD_UNKNOWN_INTERFACE
 */
extern whd_result_t
whd_wifi_deregister_ds_callback(whd_interface_t ifp, whd_ds_callback_t callback);

/** Function to configure scanmac randomisation
 *
 *  @param ifp                   Pointer to handle instance of whd interface
 *  @param config                Value to configure scanmac randomisation (0/1)
 *
 *  @return WHD result code
 */
extern whd_result_t
whd_configure_scanmac_randomisation(whd_interface_t ifp, whd_bool_t config);

/** Set the mac address via OTP bits
 *
 * @param ifp                    Pointer to handle instance of whd interface
 * @param tlvbuf                 tlv buffer to be written to the OTP section
 * @param len                    length of the buffer
 *
 * @return WHD_SUCCESS or Error code
 */
extern whd_result_t whd_wifi_set_mac_addr_via_otp(whd_interface_t ifp, char *tlvBuf, uint8_t len);


/* @} */

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* ifndef INCLUDED_WHD_WIFI_H */
