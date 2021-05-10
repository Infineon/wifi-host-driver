# Wi-Fi Host Driver (WHD)  v1.94.0
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

## Changes since v1.93.0
### New Features
* Support Nvram for LAIRD_LWB5PM2
* Supports error handling callback for BUS error or FW halt.
* Support different maximum credit numbers for different firmware

### Defect Fixes
* Update structure of counters
* Build failure with IAR toolchain
* Fix the return of the unexpected value
* Fix unknown security type in scan result
* Fix test_wifi_set_ioctl_value failure

### Known Issues

### Firmware Changes
#### CYW4343W
* --- 7.45.98.117 ---
* Security fixes
* Memory usage reduction by disabling debug features
* --- 7.45.98.110 ---

#### CYW43012
* --- 13.10.271.265 ---
* Security fixes
* Fix BT coex throughput for DOS
* Fix for roam failure when we miss reassociation response from AP at range edge
* iLPO related changes
* Beacon collision fix
* Porting protection frame enhancement
* RSA signature verification implementation
* SHA implementation in blocks and MGF function needed for RSA PKCS2.1
* SHA implementation rework
* Addition of RSA test files and cleanup
* Fix for crash during WPA3 join
* Handle window overflow and tx queue depth errors in AT command.
* No-memcpy optimization for UDP server mode operation
* --- 13.10.271.253 ---

#### CYW4373
* --- 13.10.246.252 ---
* VSDB support of AP+STA usecase
* Security fixes
* Fix beacon loss issue
* Fix USB livelock
* Fix flow control to allow for the host to send new traffic
* Allow SAE password length of 128 characters
* Prevent device from responding to broadcast probe req when hidden ssid is set
* --- 13.10.246.242 ---

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
