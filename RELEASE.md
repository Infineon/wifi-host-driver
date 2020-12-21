# Wi-Fi Host Driver (WHD)  v1.92.1
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://cypresssemiconductorco.github.io/wifi-host-driver/html/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Out-of-Band (OOB)
* Provides API functions for ARP, packet filters
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports concurrent operation of STA and AP interface
* Supports multiple security methods such as WPA2, WPA3, and open
* Provides functions for Advanced Power Management
* Supports low-power offloads, including ARP, packet filters, TCP Keepalive offload, DHCP lease time renewal offload, and Beacon trim
* Includes WFA pre-certification support for 802.11n and WPA3

## Changes since v1.92.0
### New Features
* Supports xml document

### Defect Fixes
* Fix the compilation error in ModusToolbox
* Fix memory leak when calling wifi_off()
* Fix app crash when calling whd_wifi_set_coex_config()
* Fix incorrect WHD_WLAN error code value

### Known Issues
* Allocate memory failure from Lw_IP when do test for wifi on/off over 878 times on mbed-os

### Firmware Changes
#### CYW4343W
* --- 7.45.98.95 ---
* Fixed zero stall on UDP
* --- 7.45.98.92 ---

#### CYW43012
* --- 13.10.271.253 ---
* Fix SoftAP low throughput issue under 80211 PS  mode
* Support DPP feature
* Support Coex security design of DOS
* Support to account for drift in LPO frequency
* Enhence power saving mechanism for Tx direction
* Fixed 11n certification 5.2.27 issue
* Fixed ATE FW crash in repeated RxPER tests
* Supported WPA3/SAE Softap
* Roaming enhancement
* Fixed roaming issue with password mismatch
* Fixed firmware trap caused by Big hammer
* --- 13.10.271.218 ---

#### CYW4373
* --- 13.10.246.234 ---

Note: [r] is regulatory-related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following software and tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 7.2.1        |
| IAR Compiler                                            | 8.32         |
| Arm Compiler 6                                          | 6.11         |
| Mbed OS                                                 | 6.2.0        |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.0.3        |


## More Information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://cypresssemiconductorco.github.io/wifi-host-driver/html/index.html)
* [Cypress Semiconductor](http://www.cypress.com)

---
© Cypress Semiconductor Corporation, 2019.
