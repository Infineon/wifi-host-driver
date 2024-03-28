# Wi-Fi Host Driver (WHD)  v3.1.0
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://infineon.github.io/wifi-host-driver/html/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Out-of-Band (OOB) 
* Provides API functions for ARP, packet filters
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports concurrent operation of STA and AP interface
* Supports multiple security methods such as WPA2, WPA3, and open
* Provides functions for Advanced Power Management
* Supports low-power offloads, including ARP, packet filters, TCP Keepalive offload, DHCP lease time renewal offload, and Beacon trim
* Includes WFA pre-certification support for 802.11n and WPA3

## Changes since v3.0.0
### New Features
* Added MQTT offload support
* Added NAT/NULL Keepalive offload support
* Added auto TKO offload support

### Defect Fixes
* 43022CUB M2 Board NVRAM update to rev1.2
* Remove ARM compilation warning
* Fix mfg firmware assoication issue with open security

### Known Issues


#### CYW4343W
* --- 7.45.98.120 ---

#### CYW43012
* --- 13.10.271.305 ---

#### CYW4373
* --- 13.10.246.321 ---

#### CYW43439
* --- 7.95.64 ---

#### CYW43909
* --- 7.15.168.163 ---

#### CYW43022
* --- 13.60.4 ---
* Offload support
* --- 13.54.1      ---

Note: [r] is regulatory-related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following software and tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 11.3         |
| IAR Compiler                                            | 9.40         |
| Arm Compiler 6                                          | 6.16         |
| Mbed OS                                                 | 6.2.0        |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.1.2        |


## More Information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://infineon.github.io/wifi-host-driver/html/index.html)
* [Infineon Technologies](http://www.infineon.com) 

---
