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

ifeq ($(findstring 55900,$(DEVICE_COMPONENTS)),55900)
COMPONENTS+=WIFI6
else ifeq ($(findstring 55500,$(DEVICE_COMPONENTS)),55500)
COMPONENTS+=WIFI6 55500
else ifeq ($(findstring 55572,$(DEVICE_COMPONENTS)),55572)
COMPONENTS+=WIFI6 55572
else
COMPONENTS+=WIFI5
endif
