# Wi-Fi Host Driver (WHD)

### Overview
The WHD is an independent, embedded Wi-Fi Host Driver that provides a set of APIs to interact with Cypress WLAN chips. The WHD is an independent firmware product that is easily portable to any embedded software environment, including popular IoT frameworks such as Mbed OS and Amazon FreeRTOS. Therefore, the WHD includes hooks for RTOS and TCP/IP network abstraction layers.

The [release notes](./RELEASE.md) detail the current release. You can also find information about previous versions.

### Supported bus interface
--------------------------------------
|  Interface  |4343W|43438|4373 |43012|
|:-----------:|:---:|:---:|:---:|:---:|
|  SDIO       |  O  |  O  |  O  |  O  |
|  SPI        |  O  |  O  |     |     |

### More information
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://cypresssemiconductorco.github.io/wifi-host-driver/html/index.html)
* [Wi-Fi Host Driver Release Notes](./RELEASE.md)
* [Cypress Semiconductor](http://www.cypress.com)

---
© Cypress Semiconductor Corporation, 2019.
