# Wi-Fi Host Driver (WHD)  v2.2.0
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

## Changes since v2.1.0
### New Features
* Support WPA3 for 4373

### Defect Fixes

### API Changes
* whd_wifi_scan_synch
Input argument "count" is now a pointer which also indicates the no of record received when returning.
* whd_tko_param
The variable type of parameter "set" is changed from int to uint8_t.
* whd_network_send_ethernet_data
Use whd_result_t return type for returning WHD_SUCCESS or Error code.
* Remove WEP connection (WHD_SECURITY_WEP_PSK / WHD_SECURITY_WEP_SHARED) support
* WHD de-initialization sequence changed to below:
```
      whd_wifi_leave(ifp);
      whd_wifi_off(ifp);
      whd_bus_sdio_detach(whd_driver);
      whd_deinit(ifp);
```

### Known Issues


#### CYW4343W
* --- 7.45.98.120 ---
* Fix pmk caching
* --- 7.45.98.117 ---

#### CYW43012
* --- 13.10.271.277 ---
* Fix WPA3_R1 Compatibility Issue

#### CYW4373
* --- 13.10.246.262 ---
* WPA3-R3 STA support
* WFA aggregation CVE fix
* Enable OKC, FBT and Voice Enterprise features in FW
* --- 13.10.246.254 ---

#### CYW43439
* --- 7.95.49 ---
* Fix WPA3_R1 Compatibility Issue
* WPA3_R3 Support
* --- 7.95.39 ---

#### CYW43909
* --- 7.15.168.155 ---

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
