LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        AVCDecoder.cpp \
	src/avcdec_api.cpp \
 	src/avc_bitstream.cpp \
 	src/header.cpp \
 	src/itrans.cpp \
 	src/pred_inter.cpp \
 	src/pred_intra.cpp \
 	src/residual.cpp \
 	src/slice.cpp \
 	src/vlc.cpp

LOCAL_MODULE := libstagefright_avcdec

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
 	$(LOCAL_PATH)/include \
 	$(LOCAL_PATH)/../common/include \
        $(TOP)/frameworks/av/media/libstagefright/include \
	$(TOP)/frameworks/native/include/media/openmax \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/avc_h264/dec/include \
  	$(TOP)/frameworks/av/media/libstagefright/libvpu/avc_h264/dec/src \
  	$(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
  	$(TOP)/frameworks/av/media/libstagefright/libvpu/common

LOCAL_CFLAGS := -DOSCL_IMPORT_REF= -DOSCL_UNUSED_ARG= -DOSCL_EXPORT_REF=

include $(BUILD_STATIC_LIBRARY)
