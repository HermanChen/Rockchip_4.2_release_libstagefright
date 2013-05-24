/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "SoftwareRenderer"
#include <utils/Log.h>

#include "../include/SoftwareRenderer.h"
#include <cutils/properties.h> // for property_get
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <system/window.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/ISurfaceTexture.h>
#include <linux/android_pmem.h>
#include "gralloc_priv.h"
#include <sys/ioctl.h>
#include "yuvtorgb.h"
#include "rk29-ipp.h"
#include "rga.h"
#include <fcntl.h>
#include "version.h"

struct MemToMemInfo
{
    int SrcAddr;
    int DstAddr;
    int MenSize;
};
#define WRITE_DATA_DEBUG 0

#if WRITE_DATA_DEBUG
static FILE* pOutFile = NULL;
#endif

#define HW_YUV2RGB
namespace android {

static bool runningInEmulator() {
    char prop[PROPERTY_VALUE_MAX];
    return (property_get("ro.kernel.qemu", prop, NULL) > 0);
}

SoftwareRenderer::SoftwareRenderer(
        const sp<ANativeWindow> &nativeWindow, const sp<MetaData> &meta)
    : mConverter(NULL),
      mYUVMode(None),
      mNativeWindow(nativeWindow),
      init_Flag(true),
      mHttpFlag(0) {
#if WRITE_DATA_DEBUG
    pOutFile = fopen("/sdcard/Movies/rgb.dat", "wb");
    if (pOutFile) {
        ALOGE("pOutFile open ok!");
    } else {
        ALOGE("pOutFile open fail!");
    }
#endif
    mStructId.clear();
    int32_t tmp;
    mLastbuf = 0;
    CHECK(meta->findInt32(kKeyColorFormat, &tmp));
    mColorFormat = (OMX_COLOR_FORMATTYPE)tmp;

    CHECK(meta->findInt32(kKeyWidth, &mWidth));
    CHECK(meta->findInt32(kKeyHeight, &mHeight));
    ALOGI("SoftwareRenderer construct, width: %d, heigth: %d, mNativeWindow: %p",
        mWidth, mHeight, mNativeWindow.get());

    if (!meta->findRect(
                kKeyCropRect,
                &mCropLeft, &mCropTop, &mCropRight, &mCropBottom)) {
        mCropLeft = mCropTop = 0;
        mCropRight = mWidth - 1;
        mCropBottom = mHeight - 1;
    }

    mCropWidth = mCropRight - mCropLeft + 1;
    mCropHeight = mCropBottom - mCropTop + 1;

    int32_t rotationDegrees;
    if (!meta->findInt32(kKeyRotation, &rotationDegrees)) {
        rotationDegrees = 0;
    }

    char prop_value[PROPERTY_VALUE_MAX];
    sf_info *info = sf_info::getInstance();
    RK_CHIP_TYPE mBoardType = (info->get_chip_type());
    rga_fd  = open("/dev/rga",O_RDWR,0);
    if (rga_fd > 0) {
        mBoardType = RK30;
    }else{
        mBoardType = RK29;
    }
    meta->findInt32(kKeyIsHttp, &mHttpFlag);
    if(property_get("sf.power.control", prop_value, NULL) &&(mWidth*mHeight <= atoi(prop_value))){
        if(!mHttpFlag){
            power_fd = open("/dev/video_state", O_WRONLY);
            if(power_fd == -1){
                ALOGE("power control open fd fail");
            }
            ALOGV("power control open fd suceess and write to 1");
            char c = '1';
            write(power_fd, &c, 1);
        }
    }
    if (RK30 == mBoardType) {
        if (property_get("sys.hwc.compose_policy", prop_value, NULL) && atoi(prop_value) > 0){
            meta->findInt32(kKeyIsHttp, &mHttpFlag);
        }else{
            ALOGI("hwc no open");
            mHttpFlag = 1;
        }
        if (rga_fd  < 0) {
            ALOGE("open /dev/rk30-rga fail!");
        }
    }else if(RK29 == mBoardType){
    }else{
        init_Flag = false;
        ALOGE("oh! board info no found, may be no rkxx or rkxx set error");


    }
    int halFormat;
    size_t bufWidth, bufHeight;

    switch (mColorFormat) {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_TI_COLOR_FormatYUV420PackedSemiPlanar:
        {
            if (!runningInEmulator()) {
                halFormat = HAL_PIXEL_FORMAT_YV12;
                bufWidth = (mCropWidth + 1) & ~1;
                bufHeight = (mCropHeight + 1) & ~1;
                break;
            }

            // fall through.
        }

        default:
			if(mHttpFlag){
            	halFormat = HAL_PIXEL_FORMAT_RGBA_8888;
			}else{
			halFormat = HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO;
			}
            bufWidth = mCropWidth;
            bufHeight = mCropHeight;

            mConverter = new ColorConverter(
                    mColorFormat, OMX_COLOR_Format16bitRGB565);
            CHECK(mConverter->isValid());
            break;
    }

    CHECK(mNativeWindow != NULL);
    CHECK(mCropWidth > 0);
    CHECK(mCropHeight > 0);
    CHECK(mConverter == NULL || mConverter->isValid());

    CHECK_EQ(0,
            native_window_set_usage(
            mNativeWindow.get(),
            GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_OFTEN
            | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP));

    CHECK_EQ(0,
            native_window_set_scaling_mode(
            mNativeWindow.get(),
            NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW));

    CHECK_EQ(0, native_window_set_buffer_count(mNativeWindow.get(), 4));
    android_native_rect_t crop;
	    crop.left = 0;
	    crop.top = 0;
    crop.right = mWidth & (~3); //if no 4 aglin crop csy
	    crop.bottom = mHeight;
    if(RK30 == mBoardType && mHttpFlag){
        bufWidth = (bufWidth  + 31)&(~31);
        bufHeight = (bufHeight + 15)&(~15);
    }else{
    bufWidth = (bufWidth  + 15)&(~15);
    bufHeight = (bufHeight + 15)&(~15);
    }
    CHECK_EQ(0, native_window_set_buffers_geometry(
                mNativeWindow.get(),
                bufWidth,
                bufHeight,
                halFormat));
    CHECK_EQ(0, native_window_set_crop(
                mNativeWindow.get(), &crop));

    uint32_t transform;
    switch (rotationDegrees) {
        case 0: transform = 0; break;
        case 90: transform = HAL_TRANSFORM_ROT_90; break;
        case 180: transform = HAL_TRANSFORM_ROT_180; break;
        case 270: transform = HAL_TRANSFORM_ROT_270; break;
        default: transform = 0; break;
    }

    if (transform) {
        CHECK_EQ(0, native_window_set_buffers_transform(
                    mNativeWindow.get(), transform));
    }
}

SoftwareRenderer::~SoftwareRenderer() {
#if WRITE_DATA_DEBUG
    if (pOutFile) {
        fclose(pOutFile);
        pOutFile = NULL;
    }
#endif
    int err;
    bool mStatus = true;
    if(!mHttpFlag && init_Flag){
        if (rga_fd >0){
            if(native_window_set_buffers_geometry(mNativeWindow.get(),(mWidth + 31)&(~31),(mHeight + 15)&(~15),
                        HAL_PIXEL_FORMAT_RGB_565) != 0){
                ALOGE("native_window_set_buffers_geometry fail");
                goto Fail;
            }
            android_native_rect_t crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = mWidth;
            crop.bottom = mHeight;
            CHECK_EQ(0, native_window_set_crop(mNativeWindow.get(), &crop));
        }else{
             if(native_window_set_buffers_geometry(mNativeWindow.get(),
                        (mWidth + 15)&(~15),(mHeight + 15)&(~15),HAL_PIXEL_FORMAT_YCrCb_NV12) != 0){
                ALOGE("native_window_set_buffers_geometry fail");
                goto Fail;
             }
            android_native_rect_t crop;
            crop.left = 0;
            crop.top = 0;
            crop.right = mWidth;
            crop.bottom = mHeight;
        }
        ANativeWindowBuffer *buf;

        if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),
            &buf)) != 0) {
            goto Fail;
        }

        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        Rect bounds((mWidth + 15)&(~15), (mHeight + 15)&(~15));
        void *dst;
        if(mapper.lock(
                    buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst) != 0){
            goto Fail;
        }
        int32_t Width = (mWidth + 15)&(~15);
        int32_t Height = (mHeight + 15)&(~15);
        if (rga_fd >0) {
            ALOGV("SoftwareRenderer deconstruct, use rag render one RGB565 frame");
            struct rga_req  Rga_Request;
            memset(&Rga_Request,0x0,sizeof(Rga_Request));
            Rga_Request.src.vir_w = (Width + 15)&(~15);
            Rga_Request.src.vir_h = (Height + 15)&(~15);
            Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;
          	Rga_Request.src.act_w = Width;
            Rga_Request.src.act_h = Height;
            Rga_Request.src.x_offset = 0;
            Rga_Request.src.y_offset = 0;
            Rga_Request.dst.yrgb_addr = (uint32_t)dst;
            Rga_Request.dst.uv_addr  = 0;
            Rga_Request.dst.v_addr   = 0;
           	Rga_Request.dst.vir_w = (Width + 31)&(~31);
            Rga_Request.dst.vir_h = Height;
            Rga_Request.dst.format = RK_FORMAT_RGB_565;
            Rga_Request.clip.xmin = 0;
            Rga_Request.clip.xmax = (Width + 31)&(~31) - 1;
            Rga_Request.clip.ymin = 0;
            Rga_Request.clip.ymax = Height - 1;
        	Rga_Request.dst.act_w = Width;
        	Rga_Request.dst.act_h = Height;
        	Rga_Request.dst.x_offset = 0;
        	Rga_Request.dst.y_offset = 0;
        	Rga_Request.rotate_mode = 0;
            Rga_Request.render_mode = color_fill_mode;
           	Rga_Request.mmu_info.mmu_en    = 1;
           	Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
        	if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0)
        	{
                ALOGE("rga RGA_BLIT_SYNC fail");
                mStatus =  false;
        	}
        }else{
            int32_t size = Width*Height;
            ALOGI("Width = %d,Height = %d",Width,Height);
        	memset(dst,0x0,size);
            memset(dst+size,0x80,size/2);
        }
        ALOGV("SoftwareRenderer deconstruct queue buffer Id: 0x%X", buf);
        if(mapper.unlock(buf->handle) != 0){
            goto Fail;
        }
        if(mStatus){
            if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf,
                    -1)) != 0) {
                ALOGW("Surface::queueBuffer returned error %d", err);
            }
        }else{
            ALOGE("SoftwareRenderer last frame process error");
            mNativeWindow->cancelBuffer(mNativeWindow.get(), buf, -1);
        }
    }
Fail:
    while(!mStructId.isEmpty())
    {
        VPU_FRAME *frame =  mStructId.editItemAt(0);
        if(frame->vpumem.phy_addr)
        {
            VPUMemLink(&frame->vpumem);
            VPUFreeLinear(&frame->vpumem);
        }
        free(frame);
        mStructId.removeAt(0);
    }

    delete mConverter;
    mConverter = NULL;
    char prop_value[PROPERTY_VALUE_MAX];

    if(property_get("sf.power.control", prop_value, NULL) && atoi(prop_value) > 0){
        char c = '0';
        if(power_fd > 0){
            ALOGV("power control close fd and write to 0");
            write(power_fd, &c, 1);
            close(power_fd);
        }
    }
    if(rga_fd > 0) {
        close(rga_fd);
        rga_fd = -1;
    }
}

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

void SoftwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {
    ANativeWindowBuffer *buf;
    int err;
    if(!init_Flag){
        ALOGE("Board config no found no render");
        return;
    }
    if ((err = native_window_dequeue_buffer_and_wait(mNativeWindow.get(),
            &buf)) != 0) {
        ALOGE("Surface::dequeueBuffer returned error %d", err);
        VPU_FRAME *frame = (VPU_FRAME *)data;
        if(frame->vpumem.phy_addr)
        {
            VPUMemLink(&frame->vpumem);
            VPUFreeLinear(&frame->vpumem);
        }
        return;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    Rect bounds((mWidth + 15)&(~15), (mHeight+ 15)&(~15));

    void *dst;
    CHECK_EQ(0, mapper.lock(
                buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst));

    if(!mStructId.isEmpty()&& !mHttpFlag)
    {
         ALOGV("mStructId size in = %d",mStructId.size());
         for(uint32_t i = 0; i< mStructId.size();i++)
         {
             VPU_FRAME *tem  = mStructId.editItemAt(i);
             if(tem->Res[0]== (uint32_t)buf)
             {
                if(tem->vpumem.phy_addr)
                {
                    ALOGV("release releaseId = 0x%x",tem->vpumem.phy_addr);
                    VPUMemLink(&tem->vpumem);
                    VPUFreeLinear(&tem->vpumem);
                }
                free(tem);
                mStructId.removeAt(i);
             }
         }
         ALOGV("mStructId size out = %d",mStructId.size());
    }
#ifndef HW_YUV2RGB
    if (mConverter) {
        mConverter->convert(
                data,
                mWidth, mHeight,
                mCropLeft, mCropTop, mCropRight, mCropBottom,
                dst,
                buf->stride, buf->height,
                0, 0, mCropWidth - 1, mCropHeight - 1);
    } else if (mColorFormat == OMX_COLOR_FormatYUV420Planar) {
        const uint8_t *src_y = (const uint8_t *)data;
        const uint8_t *src_u = (const uint8_t *)data + mWidth * mHeight;
        const uint8_t *src_v = src_u + (mWidth / 2 * mHeight / 2);

        uint8_t *dst_y = (uint8_t *)dst;
        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
        size_t dst_c_size = dst_c_stride * buf->height / 2;
        uint8_t *dst_v = dst_y + dst_y_size;
        uint8_t *dst_u = dst_v + dst_c_size;

        for (int y = 0; y < mCropHeight; ++y) {
            memcpy(dst_y, src_y, mCropWidth);

            src_y += mWidth;
            dst_y += buf->stride;
        }

        for (int y = 0; y < (mCropHeight + 1) / 2; ++y) {
            memcpy(dst_u, src_u, (mCropWidth + 1) / 2);
            memcpy(dst_v, src_v, (mCropWidth + 1) / 2);

            src_u += mWidth / 2;
            src_v += mWidth / 2;
            dst_u += dst_c_stride;
            dst_v += dst_c_stride;
        }
    } else {
        CHECK_EQ(mColorFormat, OMX_TI_COLOR_FormatYUV420PackedSemiPlanar);

        const uint8_t *src_y =
            (const uint8_t *)data;

        const uint8_t *src_uv =
            (const uint8_t *)data + mWidth * (mHeight - mCropTop / 2);

        uint8_t *dst_y = (uint8_t *)dst;

        size_t dst_y_size = buf->stride * buf->height;
        size_t dst_c_stride = ALIGN(buf->stride / 2, 16);
        size_t dst_c_size = dst_c_stride * buf->height / 2;
        uint8_t *dst_v = dst_y + dst_y_size;
        uint8_t *dst_u = dst_v + dst_c_size;

        for (int y = 0; y < mCropHeight; ++y) {
            memcpy(dst_y, src_y, mCropWidth);

            src_y += mWidth;
            dst_y += buf->stride;
        }

        for (int y = 0; y < (mCropHeight + 1) / 2; ++y) {
            size_t tmp = (mCropWidth + 1) / 2;
            for (size_t x = 0; x < tmp; ++x) {
                dst_u[x] = src_uv[2 * x];
                dst_v[x] = src_uv[2 * x + 1];
            }

            src_uv += mWidth;
            dst_u += dst_c_stride;
            dst_v += dst_c_stride;
        }
    }
#else
    VPU_FRAME *frame = (VPU_FRAME *)data;
    if(!mHttpFlag){
        frame->FrameWidth = (mWidth + 15)&(~15);
        frame->FrameHeight = (mHeight + 15)&(~15);
        VPU_FRAME *info = (VPU_FRAME *)malloc(sizeof(VPU_FRAME));
    	ALOGV("push buffer Id = 0x%x",buf);
        memcpy(info,data,sizeof(VPU_FRAME));
    	VPUMemLink(&frame->vpumem);
        VPUMemDuplicate(&info->vpumem,&frame->vpumem);
    	VPUFreeLinear(&frame->vpumem);
        memcpy(dst,info,sizeof(VPU_FRAME));
        info->Res[0] = (uint32_t)buf;
        mStructId.push(info);
    }else{
        if (rga_fd >0) {
            int32_t Width  = (mWidth + 15)&(~15);
            int32_t Height = (mHeight + 15)&(~15);
            struct rga_req  Rga_Request;
            memset(&Rga_Request,0x0,sizeof(Rga_Request));
            Rga_Request.src.yrgb_addr =  (int)frame->FrameBusAddr[0]+ 0x60000000;
            Rga_Request.src.uv_addr  = Rga_Request.src.yrgb_addr + ((Width + 15)&(~15)) * ((Height+ 15)&(~15));
            Rga_Request.src.v_addr   =  Rga_Request.src.uv_addr;
            Rga_Request.src.vir_w = (Width + 15)&(~15);
            Rga_Request.src.vir_h = (Height + 15)&(~15);
            Rga_Request.src.format = RK_FORMAT_YCbCr_420_SP;

          	Rga_Request.src.act_w = Width;
            Rga_Request.src.act_h = Height;
            Rga_Request.src.x_offset = 0;
            Rga_Request.src.y_offset = 0;
            Rga_Request.dst.yrgb_addr = (uint32_t)dst;
            Rga_Request.dst.uv_addr  = 0;
            Rga_Request.dst.v_addr   = 0;
           	Rga_Request.dst.vir_w = (Width + 31)&(~31);
            Rga_Request.dst.vir_h = Height;
            Rga_Request.dst.format = RK_FORMAT_RGBA_8888;
            Rga_Request.clip.xmin = 0;
            Rga_Request.clip.xmax = (Width + 31)&(~31) - 1;
            Rga_Request.clip.ymin = 0;
            Rga_Request.clip.ymax = Height - 1;
        	Rga_Request.dst.act_w = Width;
        	Rga_Request.dst.act_h = Height;
        	Rga_Request.dst.x_offset = 0;
        	Rga_Request.dst.y_offset = 0;
        	Rga_Request.rotate_mode = 0;
           	Rga_Request.mmu_info.mmu_en    = 1;
           	Rga_Request.mmu_info.mmu_flag  = ((2 & 0x3) << 4) | 1;
        	if(ioctl(rga_fd, RGA_BLIT_SYNC, &Rga_Request) != 0)
        	{
                ALOGE("rga RGA_BLIT_SYNC fail");
        	}
        }
    }
#if WRITE_DATA_DEBUG
    fwrite(dst,1, mWidth*mHeight*4, pOutFile);
#endif
#endif
    CHECK_EQ(0, mapper.unlock(buf->handle));

    if ((err = mNativeWindow->queueBuffer(mNativeWindow.get(), buf,
            -1)) != 0) {
        ALOGW("Surface::queueBuffer returned error %d", err);
    }
    mLastbuf = (uint32_t)frame->FrameBusAddr[0];
    buf = NULL;
}

}  // namespace android
