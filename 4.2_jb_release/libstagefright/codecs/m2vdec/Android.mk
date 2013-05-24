LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  M2VDecoder.cpp \


LOCAL_MODULE := libstagefright_m2vdec

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
 	$(LOCAL_PATH)/include \
 	$(LOCAL_PATH)/../common/include \
 	$(TOP)/frameworks/av/media/libstagefright/include \
  $(TOP)/frameworks/native/include/media/openmax \
  $(TOP)/frameworks/av/media/libstagefright/libvpu/m2v/dec/include \
  $(TOP)/frameworks/av/media/libstagefright/libvpu/m2v/dec/src \
  $(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
  $(TOP)/frameworks/av/media/libstagefright/libvpu/common
LOCAL_CFLAGS := -DOSCL_IMPORT_REF= -DOSCL_UNUSED_ARG= -DOSCL_EXPORT_REF=

include $(BUILD_STATIC_LIBRARY)
