# Wi-Fi Host Driver (WHD)  v2.6.1
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

## Changes since v2.5.0
### New Features
* Supports WPA2 with SHA256
* Add support MURATA-1YN, MURATA-2AE and MURATA-2BC modules
* Make NVRAM image size alighment configurable for supporting the specific MCU
* Configurable for drive mode in OOB pin

### Defect Fixes


### Known Issues


#### CYW4343W
* --- 7.45.98.120 ---

#### CYW43012
* --- 13.10.271.293 ---
* Add Clear PMKID Cache API
* --- 13.10.271.289 ---

#### CYW4373
* --- 13.10.246.286 ---

#### CYW43439
* --- 7.95.62 ---
* Fixed EVM issue
* --- 7.95.55 ---

#### CYW43909
* --- 7.15.168.159 ---
* Supports WPA3 R3 in STA mode
* --- 7.15.168.156 ---

Note: [r] is regulatory-related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following software and tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 10.3         |
| IAR Compiler                                            | 9.3          |
| Arm Compiler 6                                          | 6.16         |
| Mbed OS                                                 | 6.2.0        |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.1.2        |


## More Information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://infineon.github.io/wifi-host-driver/html/index.html)
* [Infineon Technologies](http://www.infineon.com) 

---
© Infineon Technologies, 2019.
