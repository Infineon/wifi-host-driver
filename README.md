# Wi-Fi Host Driver (WHD)

### Overview
The WHD is an independent, embedded Wi-Fi Host Driver that provides a set of APIs to interact with Cypress WLAN chips. The WHD is an independent firmware product that is easily portable to any embedded software environment, including popular IOT frameworks like Mbed OS, Amazon FreeRTOS, etc. Hence, the WHD includes hooks for RTOS and TCP/IP network abstraction layers.
   
### Features
* Supports Wi-Fi Station (STA) and AP mode of operation.
* Supports concurrent operation of STA and AP interface.
* Includes multiple security support like WPA2, WPA3, and open.
* Provides function to perform Advanced Power Management.
* Supports low power offloads, including ARP and packet filters.
* Includes WFA Pre-certification support for 802.11n and WPA3.

### More information
* [Wi-Fi Host Driver API Reference Manual and Porting Guide](https://cypresssemiconductorco.github.io/wifi-host-driver/API/index.html)
* [Wi-Fi Host Driver Release Notes](./RELEASE.md)
* [Cypress Semiconductor](http://www.cypress.com)

---
© Cypress Semiconductor Corporation, 2019.