# Wi-Fi Host Driver (WHD)  v1.30.0
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://cypresssemiconductorco.github.io/wifi-host-driver/API/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports concurrent operation of STA and AP interface
* Includes multiple security support like WPA2, WPA3, and open
* Provides function to perform Advanced Power Management
* Supports low power offloads, including ARP, packet filters
* Includes WFA Pre-certification support for 802.11n and WPA3

## Changes since v1.10.0
### New Features
* TCP Keepalive offload, DHCP lease time renewal offload and Beacon trim
* Out-of-bound (OOB) interrupt support
* Allow clm_blob from external storage
* Nvram addition

### Defect Fixes
* Cdc semaphores are freed twice during deinit
* Whd_bus_sdio_attach API throws WHD_ERROR("Timeout while waiting for high throughput clock\n") while testing deinit and init case
* API input parameter validation
* Lots of assert message "failed to post AP link semaphore" in whd_handle_apsta_event

### Known Issues
None

### Firmware Changes
#### 4343W

* --- 7.45.98.89 ---
* Security fix(Dragonblood WPA3 attack)
* TCP Keepalive Implementation
* Security fix(CVE-2019-9501 / CVE-2019-9502)
* --- 7.45.98.81 ---

#### 43012
* --- 13.10.271.198 ---
* Security fix(Dragonblood WPA3 attack)
* Association failure fix
* BT coex throughput fix
* Fix for Beacon loss issue
* Fix for WPA3 Cert timeout failure
* 2G/5G band-edge improvement [r]
* RSSI value display fix
* Security fix(CVE-2019-9501 / CVE-2019-9502)
* --- 13.10.271.162 ---

Note: [r] is regulatory related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following Software and Tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 7.2.1        |
| IAR Compiler                                            | 8.32         |
| ARM Compiler 6                                          | 6.11         |
| MBED OS                                                 | 5.13.1       |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.0.3        |


## More information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://cypresssemiconductorco.github.io/wifi-host-driver/API/index.html)
* [Cypress Semiconductor](http://www.cypress.com)

---
© Cypress Semiconductor Corporation, 2019.
