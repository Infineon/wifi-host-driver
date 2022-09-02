# Wi-Fi Host Driver (WHD)  v2.4.0
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

## Changes since v2.3.0
### New Features
* Support API for roaming threshold setting

### Defect Fixes
* Remove deprecated resources
* Fix SDIO warning error
* Fix current issue on power saving mode
* Fix external SAE connection issue

### Known Issues


#### CYW4343W
* --- 7.45.98.120 ---

#### CYW43012
* --- 13.10.271.287 ---

#### CYW4373
* --- 13.10.246.279 ---
* Enable BCN_LOSS_WAR flag
* --- 13.10.246.264 ---

#### CYW43439
* --- 7.95.54 ---
* Beacon loss Addition
* --- 7.95.50 ---

#### CYW43909
* --- 7.15.168.156 ---

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
* [Infineon Technologies](http://www.infineon.com)

---
© Infineon Technologies, 2019.
