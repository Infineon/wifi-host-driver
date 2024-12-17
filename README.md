# Wi-Fi Host Driver (WHD)

### Overview
The WHD is an independent, embedded Wi-Fi Host Driver that provides a set of APIs to interact with Infineon WLAN chips. The WHD is an independent firmware product that is easily portable to any embedded software environment, including popular IoT frameworks such as Mbed OS, Amazon FreeRTOS and Azure RTOS ThreadX. Therefore, the WHD includes hooks for RTOS and TCP/IP network abstraction layers.

The [release notes](./RELEASE.md) detail the current release. You can also find information about previous versions.

### WIFI6

### Supported bus interface
---------------------------------
|  Interface  |55500|55900|55572|
|:-----------:|:---:|:---:|:---:|
|  SDIO       |  O  |     |  O  | 
|  SPI        |     |     |     |
|  M2M        |     |     |     |
|  OCI        |     |  O  |     |

### AP mode support 
---------------------------------
|  Security   |55500|55900|55572|
|:-----------:|:---:|:---:|:---:|
|  WPA3       |  O  |  O  |  O  |
|  WPA2       |  O  |  O  |  O  |

### WIFI5

### Supported bus interface
---------------------------------------------------------
|  Interface  |4343W|43438|4373 |43012|43439|43907|43022|
|:-----------:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|  SDIO       |  O  |  O  |  O  |  O  |  O  |     |  O  |
|  SPI        |  O  |  O  |     |     |  O  |     |     |
|  M2M        |     |     |     |     |     |  O  |     |

### WPA3 AP mode support
---------------------------------------------------------
|  Security   |4343W|43438|4373 |43012|43439|43907|43022|
|:-----------:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|  WPA3       |     |     |  O  |     |  O  |     |     |


### More information
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://infineon.github.io/wifi-host-driver/html/index.html)
* [Wi-Fi Host Driver Release Notes](./RELEASE.md)
* [Infineon Technologies](http://www.infineon.com)

---
