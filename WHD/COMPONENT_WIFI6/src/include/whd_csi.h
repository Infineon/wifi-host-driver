#if defined(WHD_CSI_SUPPORT)
/* Infineon WLAN driver: Channel State Information (CSI)- Header
 *
 * Copyright 2023 Cypress Semiconductor Corporation (an Infineon company)
 * or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
 * This software, including source code, documentation and related materials
 * ("Software") is owned by Cypress Semiconductor Corporation or one of its
 * affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license agreement
 * accompanying the software package from which you obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source code
 * solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without
 * the expresswritten permission of Cypress.
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * Cypress reserves the right to make changes to the Software without notice.
 * Cypress does not assume any liability arising out of the application or
 * use of the Software or any product or circuit described in the Software.
 * Cypress does not authorize its products for use in any products where a malfunction
 * or failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product").
 * By including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing so
 * agrees to indemnify Cypress against all liability.
 */

#ifndef INCLUDED_WHD_CSI_H
#define INCLUDED_WHD_CSI_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "whd.h"
struct wlc_csi_fragment_hdr {
	uint8_t hdr_version;
	uint8_t sequence_num;
	uint8_t fragment_num;
	uint8_t total_fragments;
};

/** Callback for CSI data that is called when CSI data is processed and is ready to be sent to upper layer
 *
 * @param data					  Pointer to CSI data
 * @param len					  Length of the CSI data
 *
 */

typedef void (*whd_csi_data_sendup)(char* data, int len);

struct whd_csi_info {
	char *data;
	whd_csi_data_sendup csi_data_cb_func;
};

whd_result_t
whd_wifi_csi_attach_and_handshake(whd_interface_t ifp);

whd_result_t
whd_wifi_csi_process_csi_data(whd_interface_t ifp,
							const uint8_t *event_data, uint32_t datalen);

whd_result_t
whd_wifi_csi_detach_and_release_uart(whd_interface_t ifp);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
#endif /* defined(WHD_CSI_SUPPORT) */