# Wi-Fi Host Driver (WHD)  v4.1.1
Please refer to the [README File](./README.md) and the [WHD API Reference Manual](https://infineon.github.io/wifi-host-driver/html/index.html) for a complete description of the Wi-Fi Host Driver.

## Features
* Supports Wi-Fi Station (STA) and AP mode of operation
* Supports multiple security methods such as WPA2, WPA3, and open

### New Features
* Support for SM on H1 family
* Offload config support


### Defect Fixes


### WIFI6 Supported Chip v4.1.0

### Known Issues
NA

#### CYW55500
* --- 28.10.301 ---



#### CYW55900
* --- 28.10.215 ---



### WIFI5 Suppported Chip v3.3.1

* Supports concurrent operation of STA and AP interface
* Supports low-power offloads like ARP, packet filters, TCP Keepalive offload
* Includes WFA pre-certification support for 802.11n, 802.11ac
* Provides API functions for ARP, packet filters
* Provides functions for Advanced Power Management


### Known Issues
NA

#### CYW4343W
* --- 7.45.98.120 ---

#### CYW43012
* --- 13.10.271.305 ---

#### CYW4373
* --- 13.10.246.321 ---

#### CYW43439
* --- 7.95.88 ---
* Fix Ping SOFTAP SCC issue
* Fix WPA3 AP H2E only setting is missing in supported/extended rate element (WPA3 Test 4.2.2)
* Fix BSSID is 0 after wl down/up
* Fix STA couldn't associate with WPA2PSk when DUT(SoftAP) is in Transition
* Update h2e set flag for idsup to be in sync with ext sae
* Continues to write dutycycle into shmem
* MFP could not enabled SoftAP with WPA3 Transition mode
* Fix M3 mismatch STA with WPA2PSk when DUT(SoftAP) is in Transition
* Correct the DSSS crsmin RSSI link power fix
* Fix for ch11 and ch13 PER floor during jammer test
* Fix link reason doesn't align with roam reason
* fix typo in wpa3sae/wpa2psk MR
* APSTA: Fix SAP connection failed in SCC cases
* Fix tput zero stall issue with mobile device
* CVE-2022-47522: PMF: AP sends Action Frame(SA Query Req) in plain text after sending Deauthentication
* WPA3-CERT: supporting WPA2PSK roams to WPA3SAE transition mode AP
* To enable OKC support in firmware
* add bip check for assoc.req malformed issue
* 43439 support for extsae
* Fix PSK-SHA256 suite not working in SAP mode
* wpa2 do roaming to wpa3
* 43439-SDIO: fix quicktrack certification failure on longer rsnx length
* --- 7.95.64 ---

#### CYW43909
* --- 7.15.168.163 ---

#### CYW43022
* --- 13.67.2 ---
* Offload support
* --- 13.54.1 ---

Note: [r] is regulatory-related

## Supported Software and Tools
This version of the WHD was validated for compatibility with the following software and tools:

| Software and Tools                                      | Version      |
| :---                                                    | :----        |
| GCC Compiler                                            | 10.3         |
| IAR Compiler                                            | 9.50         |
| Arm Compiler 6                                          | 6.22         |
| Mbed OS                                                 | 6.2.0        |
| ThreadX/NetX-Duo                                        | 5.8          |
| FreeRTOS/LWIP                                           | 2.1.2        |


## More Information
* [Wi-Fi Host Driver README File](./README.md)
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://infineon.github.io/wifi-host-driver/html/index.html)
* [Infineon Technologies](http://www.infineon.com)

---
© Infineon Technologies, 2019.
