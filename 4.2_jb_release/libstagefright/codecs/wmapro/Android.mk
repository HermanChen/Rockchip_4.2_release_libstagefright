LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	WMAPRODecoder.cpp	



ifeq ($(TARGET_ARCH),arm)
LOCAL_SRC_FILES += \

else
LOCAL_SRC_FILES += \

endif






LOCAL_SHARED_LIBRARIES  :=  \
					  \
			  libutils          \
        libcutils         \
        libdl	
     


LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/av/media/libstagefright/include \
       

       

LOCAL_CFLAGS := \
        -DOSCL_UNUSED_ARG=

LOCAL_MODULE := libstagefright_wmaprodec

include $(BUILD_STATIC_LIBRARY)

