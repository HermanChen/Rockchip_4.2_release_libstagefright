LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        VPXDecoder.cpp

LOCAL_MODULE := libstagefright_vpxdec

LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/av/media/libstagefright/include \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/libvpx \
        $(TOP)/external/libvpx/vpx_codec \
        $(TOP)/external/libvpx/vpx_ports \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/vp8/dec/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/vp6/dec/include \
 	$(TOP)/frameworks/av/media/libstagefright/libvpu/vp8/dec/src \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/vp6/dec/src \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/common

include $(BUILD_STATIC_LIBRARY)
