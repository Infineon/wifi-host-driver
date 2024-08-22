# Wi-Fi Host Driver (WHD)  v4.0.1
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://infineon.github.io/wifi-host-driver/html/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports multiple security methods such as WPA2, WPA3, and open

### New Features
Added H1-CP support for CM and SM
Added offload config support


### Defect Fixes

#### CYW55500
#A0
* --- 28.10.59 ---

#A1
* --- 28.10.190 ---


#### CYW55900
* --- 28.10.215 ---

* Supports concurrent operation of STA and AP interface
* Supports low-power offloads like ARP, packet filters, TCP Keepalive offload
* Includes WFA pre-certification support for 802.11n, 802.11ac, 802.11ax
* Provides API functions for ARP, packet filters
* Provides functions for Advanced Power Management

### Known Issues
NA

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
