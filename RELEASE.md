# Wi-Fi Host Driver (WHD)  v1.40.0
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://cypresssemiconductorco.github.io/wifi-host-driver/API/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports concurrent operation of STA and AP interface
* Supports multiple security methods such as WPA2, WPA3, and open
* Provides functions for Advanced Power Management
* Supports low-power offloads, including ARP, packet filters, TCP Keepalive offload, DHCP lease time renewal offload, and Beacon trim
* Includes WFA pre-certification support for 802.11n and WPA3

## Changes since v1.30.0
### New Features
None

### Defect Fixes
* Security fix (KRACK all-zero-key)
* Fixed Wi-Fi connection issue after receiving DISASSOC_IND messages
* Fixed ioctl buffer length overflow
* Added API input argument checks

### Known Issues
None

### Firmware Changes
#### CYW4343W

* --- 7.45.98.92 ---
* Security fix (KRACK all-zero-key)
* --- 7.45.98.89 ---

#### CYW43012
* --- 13.10.271.203 ---
* Security fix (KRACK all-zero-key)
* --- 13.10.271.192 ---

Note: [r] is regulatory-related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following software and tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 7.2.1        |
| IAR Compiler                                            | 8.32         |
| Arm Compiler 6                                          | 6.11         |
| Mbed OS                                                 | 5.13.1       |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.0.3        |


## More Information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://cypresssemiconductorco.github.io/wifi-host-driver/API/index.html)
* [Cypress Semiconductor](http://www.cypress.com)

---
© Cypress Semiconductor Corporation, 2019.
