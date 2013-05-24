LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                     \
        ColorConverter.cpp            \
        SoftwareRenderer.cpp

LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/hardware/msm7k \
        $(TOP)/hardware/rk29/libyuvtorgb \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common \
        $(TOP)/frameworks/av/media/libstagefright/include \
        $(TOP)/frameworks/av/media/libstagefright

ifeq ($(strip $(TARGET_BOARD_HARDWARE)),rk29board)
LOCAL_C_INCLUDES += hardware/rk29/libgralloc
else
LOCAL_C_INCLUDES += hardware/rk29/libgralloc_ump
LOCAL_CFLAGS += -DTARGET_RK30
endif
LOCAL_MODULE:= libstagefright_color_conversion

include $(BUILD_STATIC_LIBRARY)
