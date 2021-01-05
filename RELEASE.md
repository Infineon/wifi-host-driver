# Wi-Fi Host Driver (WHD)  v1.93.0
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

## Changes since v1.92.1
### New Features
* Add nvram for CYBSYSKIT-DEV-01
* Print FW log buffer and IOCTL logs when getting trap indicator

### Defect Fixes
* Fix the enterprise scanresults parsing and printing
* Fix Kitprog3 reset failed after downloading image

### Known Issues

### Firmware Changes
#### CYW4343W
* --- 7.45.98.110 ---
* Fixed zero stall on UDP
* Fixed Tx traffic too less then RX
* --- 7.45.98.95 ---

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
* --- 13.10.246.242 ---
* Fix flow control to allow for the host to send new traffic
* Allow SAE password length of 128 characters
* Prevent device from responding to broadcast probe req when hidden ssid is set
* DOS: Coex Security design for DOS
* Fix for WiFi P2P cert 4.2.2.
* Ucode fix to avoid setting Multicast bit in non-DTIM Beacon
* DPP feature support enable
* PTK rotation WAR for EPSON
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
