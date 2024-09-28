/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 *  Provides event handler functions for CSI functionality
 */

#if defined(WHD_CSI_SUPPORT)
#include "whd_csi.h"
#include "whd_types.h"
#include "whd_utils.h"
#include "whd_int.h"
#define NETLINK_USER 31
#define CSI_GRP  22
#define CSI_DATA_BUFFER_SIZE 1024

typedef unsigned char uint8_t;

/**
 * whd_wifi_csi_attach_and_handshake() - Mallocs required memory for the CSI data
 *                                       and sends first byte through WLAN UART .
 *
 * @csi_info: Pointer to CSI info in cfg
 */

whd_result_t
whd_wifi_csi_attach_and_handshake(whd_interface_t ifp)
{
    struct whd_csi_info *csi_info;
    whd_result_t err = 0;
    csi_info = ifp->csi_info;
    /* Allocating memory for csi_info data buffer for CSI message */
    csi_info->data = whd_mem_malloc(CSI_DATA_BUFFER_SIZE * sizeof(char));
    if (!csi_info->data) {
        printf("CSI: Failed to allocate buffer for CSI message \n");
        return WHD_BUFFER_ALLOC_FAIL;
    }

    /* HandShake byte */
    char *data = "b";
    csi_info->csi_data_cb_func(data,1);
    return err;
}

/**
 * whd_wifi_csi_process_csi_data() - Combine fragment CSI data received as events to a single
 * CSI message to send to user
 *
 * @ifp: Pointer to whd_interface
 * @event_data: Pointer to Event payload, CSI
 * @datalen: Length of event payload message
 *
 * return: whd_result_t.
 */

whd_result_t
whd_wifi_csi_process_csi_data(whd_interface_t ifp,
                            const uint8_t *event_data, uint32_t datalen)
{
    struct wlc_csi_fragment_hdr *frag_hdr = (struct wlc_csi_fragment_hdr *)event_data;
    struct whd_csi_info *csi_info_ifp = ifp->csi_info;
    char *data = csi_info_ifp->data;
    static char *last;
    int hdrlen = sizeof(struct wlc_csi_fragment_hdr);
    datalen = datalen -hdrlen;
    event_data = event_data + hdrlen;
    /* TODO: Handle sequence number, fragment number mismatches. Check header version. */
    if (frag_hdr->fragment_num == 0) {
        last = data;
    }
    whd_mem_memcpy(last, event_data, datalen);
    last = last + datalen;
    /* Handling the last frame */
    if (frag_hdr->fragment_num == (frag_hdr->total_fragments - 1)) {
        csi_info_ifp->csi_data_cb_func(data,(last - data));
        last = data;
    }
    return 0;
}

whd_result_t
whd_wifi_csi_detach_and_release_uart(whd_interface_t ifp)
{
    struct whd_csi_info *csi_info_ifp = ifp->csi_info;
    /* TODO: Send some signal that would indicate the script that disable event is being received */
    char *data = "BYE";
    csi_info_ifp->csi_data_cb_func(data,3);
    whd_mem_free(csi_info_ifp->data);
    whd_mem_free(ifp->csi_info);
    return 0;
}

#endif /* defined(WHD_CSI_SUPPORT) */