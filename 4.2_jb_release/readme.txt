1.replace frameworks/av/media/libstagefright/ using libstagefright in this file

2.compare and replace the difference between header fold frameworks/av/include/media/ and media media

3.compare and replace the defferent binary library between device/rockchip/rk30sdk/proprietary/libvpu/ and libvpu
PS. the latest path under device will be device/rockchip/common/vpu/

4.add below lines to device/rockchip/rk30sdk/proprietary/libvpu/Android.mk

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := librk_demux.so
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := librk_on2.so
LOCAL_MODULE_TAGS := optional
include $(BUILD_MULTI_PREBUILT)

5.delete device/rockchip/rk30sdk/proprietary/libvpu/libstagefright.so and libvpu.so

6.make clean then make

7.NOTE: Because the open source libmediaplayerservice is using Awesomeplayer.h please make sure the merged Awesomeplayer.h has no conflict between binary and source code
