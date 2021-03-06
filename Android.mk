##
##
## Copyright 2009, The Android Open Source Project
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

BASE_PATH := $(call my-dir)

V8_SUPPORTED_ARCH := arm mips x86 arm64 mips64 x86_64

ifeq ($(TARGET_ARCH),arm)
    ifneq ($(strip $(ARCH_ARM_HAVE_ARMV7A)),true)
        $(warning WARNING: Building on ARM with non-ARMv7 variant. On ARM, V8 is well tested only on ARMv7.)
    endif
endif

# Build libv8 and d8

ENABLE_V8_SNAPSHOT = false
mksnapshot_arch := $(TARGET_ARCH)
ifeq ($(HOST_IS_64_BIT),true)
ifeq ($(TARGET_IS_64_BIT),true)
mksnapshot_multilib := 64
else  # Target is 32-bit.
mksnapshot_multilib := 32
endif
endif

include $(BASE_PATH)/Android.mksnapshot.mk

ifdef TARGET_2ND_ARCH

ifeq ($(HOST_IS_64_BIT),true)
ifeq ($(2ND_TARGET_IS_64_BIT),true)
mksnapshot_multilib := 64
else  # Target is 32-bit.
mksnapshot_multilib := 32
endif
endif

mksnapshot_arch := $(TARGET_2ND_ARCH)
include $(BASE_PATH)/Android.mksnapshot.mk
endif

include $(BASE_PATH)/Android.libv8.mk
include $(BASE_PATH)/Android.d8.mk
