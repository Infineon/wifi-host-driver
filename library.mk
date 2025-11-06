#
# Copyright 2019 Cypress Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

print-%  : ; @echo $* = $($*)

ifneq ($(filter 55572 55900 55900A0 55500 55530 89530,$(DEVICE_COMPONENTS)),)
COMPONENTS+=WIFI6
FW_PATH    := $(SEARCH_wifi-host-driver)/WHD/COMPONENT_WIFI6/resources/firmware
else
COMPONENTS+=WIFI5
FW_PATH    := $(SEARCH_wifi-host-driver)/WHD/COMPONENT_WIFI5/resources/firmware
endif

ifeq ($(filter WIFI6,$(COMPONENTS)),WIFI6)
    ifeq ($(filter SM,$(DEVICE_COMPONENTS))$(filter SM,$(BSP_COMPONENTS)),SM)
    COMP_SECURITY_MODE := COMPONENT_SM
    CFW_EXTENTION_NAME := trxcse
    FW_EXTENTION_NAME := trxse
    else ifeq ($(filter CM,$(DEVICE_COMPONENTS))$(filter CM,$(BSP_COMPONENTS)),CM)
    COMP_SECURITY_MODE := COMPONENT_CM
    CFW_EXTENTION_NAME := trxc
    FW_EXTENTION_NAME := trx
    endif
else   #WIFI5
    ifeq ($(filter 43022,$(DEVICE_COMPONENTS)),43022)
    FW_EXTENTION_NAME := trxs
    else
    FW_EXTENTION_NAME := bin
    endif
endif

#HANDLE MACRO DEPENDENCIES
ifeq ($(findstring 43022,$(DEVICE_COMPONENTS)),43022)
DEFINES+=ULP_CONFIG
endif

#HANDLE FIRMWARE FILES
ifeq ($(filter 89530,$(DEVICE_COMPONENTS)),89530)
COMPONENTS+=89530
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
       FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_89530/$(COMP_SECURITY_MODE)/89530A0-mfgtest.$(CFW_EXTENTION_NAME)"
    else
       FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_89530/$(COMP_SECURITY_MODE)/89530A0.$(CFW_EXTENTION_NAME)"
    endif
else ifeq ($(filter 55900,$(DEVICE_COMPONENTS)),55900)
COMPONENTS+=55900
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
        FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55900/$(COMP_SECURITY_MODE)/55900A0-mfgtest.$(CFW_EXTENTION_NAME)"
    else ifeq ($(filter WLAN_CSI_FIRMWARE,$(DEFINES)),WLAN_CSI_FIRMWARE)
        FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55900/$(COMP_SECURITY_MODE)/55900A0-sense.$(CFW_EXTENTION_NAME)"
    else
        FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55900/$(COMP_SECURITY_MODE)/55900A0.$(CFW_EXTENTION_NAME)"
    endif
else ifeq ($(filter 55500,$(DEVICE_COMPONENTS)),55500)
COMPONENTS+=55500
	ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55500/$(COMP_SECURITY_MODE)/55500A1-mfgtest.$(CFW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55500/$(COMP_SECURITY_MODE)/55500A1.$(CFW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 55572,$(DEVICE_COMPONENTS)),55572)
COMPONENTS+=55572
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55572/$(COMP_SECURITY_MODE)/55572A1-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55572/$(COMP_SECURITY_MODE)/55572A1.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 55530,$(DEVICE_COMPONENTS)),55530)
COMPONENTS+=55530
	ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55530/$(COMP_SECURITY_MODE)/55530A0-mfgtest.$(CFW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_55530/$(COMP_SECURITY_MODE)/55530A0.$(CFW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 43439,$(DEVICE_COMPONENTS)),43439)
COMPONENTS+=43439
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
        FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43439/43439A0-mfgtest.$(FW_EXTENTION_NAME)"
    else
        FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43439/43439A0.$(FW_EXTENTION_NAME)"
    endif
else ifeq ($(filter 43012,$(DEVICE_COMPONENTS)),43012)
COMPONENTS+=43012
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43012/43012C0-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43012/43012C0.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 43022,$(DEVICE_COMPONENTS)),43022)
COMPONENTS+=43022
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43022/COMPONENT_SM/43022C1-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43022/COMPONENT_SM/43022C1.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 4373,$(DEVICE_COMPONENTS)),4373)
COMPONENTS+=4373
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4373/4373A0-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4373/4373A0.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 43438,$(DEVICE_COMPONENTS)),43438)
COMPONENTS+=43438
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43438/43438A1-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43438/43438A1.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 43364,$(DEVICE_COMPONENTS)),43364)
COMPONENTS+=43364
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43364/43364A1-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_43364/43364A1.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 4390X,$(DEVICE_COMPONENTS)),4390X)
COMPONENTS+=4390X
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4390X/43909B0-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4390X/43909B0.$(FW_EXTENTION_NAME)"
	endif
else ifeq ($(filter 4343W,$(DEVICE_COMPONENTS)),4343W)
COMPONENTS+=4343W
    ifeq ($(filter WLAN_MFG_FIRMWARE,$(DEFINES)),WLAN_MFG_FIRMWARE)
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4343W/4343WA1-mfgtest.$(FW_EXTENTION_NAME)"
	else
		FW_FILE_PATH_NAME := "$(FW_PATH)/COMPONENT_4343W/4343WA1.$(FW_EXTENTION_NAME)"
	endif
endif

$(info FW_FILE_PATH_NAME is: $(FW_FILE_PATH_NAME))

# Check if DATA_PATH exists
CHECK_PATH := $(shell [ -f "$(FW_FILE_PATH_NAME)" ] && echo "exists" || echo "notfound")
ifeq ($(CHECK_PATH),notfound)
    $(info DATA_PATH "$(FW_FILE_PATH_NAME)" does not exist. Please provide a valid directory.)
endif

ifeq ($(TOOLCHAIN),IAR)
    # Default IAR Settings
    ifeq ($(filter $(MAKECMDGOALS),ewarm ewarm8),)
        ifeq ($(filter 4343W,$(DEVICE_COMPONENTS)),4343W)
            ifeq ($(filter CY8C6245LQI-S3D72,$(DEVICE)),CY8C6245LQI-S3D72)
              LDFLAGS+= --image_input=$(FW_FILE_PATH_NAME),wifi_firmware_image_data,.cy_xip,4
            else
              LDFLAGS+= --image_input=$(FW_FILE_PATH_NAME),wifi_firmware_image_data,.data,4
            endif
        else
          LDFLAGS+= --image_input=$(FW_FILE_PATH_NAME),wifi_firmware_image_data,.data,4
        endif
    else
        # To compile in EWARM IAR IDE
        FW_PATH_WO_QUOTE_TMP    := $(subst ",,$(FW_FILE_PATH_NAME))
        FW_PATH_WO_QUOTE    := $(addprefix $$PROJ_DIR$$/, $(FW_PATH_WO_QUOTE_TMP))
        ifeq ($(filter 4343W,$(DEVICE_COMPONENTS)),4343W)
            ifeq ($(filter CY8C6245LQI-S3D72,$(DEVICE)),CY8C6245LQI-S3D72)
              LDFLAGS+= --image_input=$(FW_PATH_WO_QUOTE),wifi_firmware_image_data,.cy_xip,4
            else
              LDFLAGS+= --image_input=$(FW_PATH_WO_QUOTE),wifi_firmware_image_data,.data,4
            endif
        else
            LDFLAGS+= --image_input=$(FW_PATH_WO_QUOTE),wifi_firmware_image_data,.data,4
        endif
    endif
endif

UNAME_S := $(shell uname)
$(info UNAME_S is: $(UNAME_S))
ifeq ($(UNAME_S),Darwin)
   FW_FILE_SIZE := $(shell stat -f%z $(FW_FILE_PATH_NAME))
else
   FW_FILE_SIZE := $(shell stat -c%s $(FW_FILE_PATH_NAME))
endif
$(info FW_FILE_SIZE is: $(FW_FILE_SIZE))

DEFINES+= FW_IMAGE_SIZE=$(FW_FILE_SIZE)
DEFINES+= FW_IMAGE_NAME='$(FW_FILE_PATH_NAME)'
