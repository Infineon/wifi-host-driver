# Wi-Fi Host Driver (WHD)  v2.0.0
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

## Changes since v1.94.0
### New Features
* Support BSP v2.0 only
* 43439 support
* Supports new nvram folder structure
* Supports for sharing bus to BT
* Optimized memory for the unused firmware.
* Enable 4373 oob interrupt

### Defect Fixes
* Fix the failed build for 43438 and 43364
* Update the mechanism of the aliged address
* Changed macro to variable for polling mode
* Fix the length of the custom ie in api
* Fix the NULL pointer of btdev in whd_allow_wlan_bus_to_sleep
* Remove bus detach API from whd_deinit
* Fix the data is not synced in 64bit platform
* Add returns for the APIs of sending packed to WHD
* Fix the unknown charater in nvram file
* Fix the reading console log from 4373 FW
* Remove WEP support
* Fix out of memory due to meany packets in send queue
* Fix null pointer dereference during scanning

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
* --- 13.10.271.265 ---

#### CYW4373
* --- 13.10.246.254 ---
* TCP keepalive support
* Fix for crash during automated sw diversity test.
* Porting dos changes for counterring dos attacks during coex.
* Throughput fixs
* --- 13.10.246.252 ---

#### CYW43439
* --- 7.95.39 ---

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
