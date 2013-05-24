# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
BUILD_VPU_MEM_TEST := false
BUILD_VPU_TEST := false
BUILD_PPOP_TEST := true

include $(CLEAR_VARS)

LOCAL_MODULE := libvpu

LOCAL_CFLAGS += -Wno-multichar 

LOCAL_ARM_MODE := arm

LOCAL_PRELINK_MODULE := false

LOCAL_STATIC_LIBRARIES := libcutils

LOCAL_SHARED_LIBRARIES := libion

LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include \

LOCAL_SRC_FILES := vpu_mem.c \
                   vpu_drv.c \
	               vpu.c \
				   ppOp.cpp

LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

ifeq ($(BUILD_VPU_MEM_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := vpu_mem_test
LOCAL_CFLAGS += -Wno-multichar -DBUILD_VPU_MEM_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_STATIC_LIBRARIES := libcutils
LOCAL_SHARED_LIBRARIES := libion libvpu
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := vpu_mem.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif

ifeq ($(BUILD_VPU_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := vpu_test
LOCAL_CFLAGS += -Wno-multichar -DBUILD_VPU_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_STATIC_LIBRARIES := libcutils
LOCAL_SHARED_LIBRARIES := libion libvpu
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := vpu.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif


ifeq ($(BUILD_PPOP_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := pp_test
LOCAL_CPPFLAGS += -Wno-multichar -DBUILD_PPOP_TEST=1
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_STATIC_LIBRARIES := libcutils
LOCAL_SHARED_LIBRARIES := libion libvpu
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(LOCAL_PATH)/include \
		    $(TOP)/hardware/libhardware/include
LOCAL_SRC_FILES := ppOp.cpp
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif
