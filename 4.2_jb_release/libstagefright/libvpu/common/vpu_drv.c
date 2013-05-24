/*
 * Copyright (C) 2013 Rockchip Open Libvpu Project
 * Author: Herman Chen chm@rock-chips.com
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

#ifndef RVDS_PROGRAME

#define ALOG_TAG "VPU"

#include "vpu_type.h"
#include "vpu.h"
#include "vpu_drv.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <unistd.h>

// 本地套接字用头文件
#include <cutils/sockets.h>
#include <sys/un.h>
#include "vpu_list.h"

// 中断等待
#include <sys/poll.h>

#include <errno.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef  ALOGI
#undef  ALOGI
#define ALOGI
#endif
#ifdef  ALOGD
//#undef  ALOGD
//#define ALOGD
#endif
#ifdef  ALOGE
//#undef  ALOGE
//#define ALOGE
#endif


#define VPU_IOC_MAGIC  'k'
/* Use 'k' as magic number */
#define VPU_IOC_CLOCK_ON        _IOW(VPU_IOC_MAGIC, 1, unsigned long)
#define VPU_IOC_CLOCK_OFF       _IOW(VPU_IOC_MAGIC, 2, unsigned long)

#define VPU_IOC_CLOCK_RESET     _IOW(VPU_IOC_MAGIC, 3, unsigned long)
#define VPU_IOC_CLOCK_UNRESET   _IOW(VPU_IOC_MAGIC, 4, unsigned long)

#define VPU_IOC_DOMAIN_ON       _IO(VPU_IOC_MAGIC,  5)
#define VPU_IOC_DOMAIN_OFF      _IO(VPU_IOC_MAGIC,  6)

#define VPU_IOC_WR_DEC          _IOW(VPU_IOC_MAGIC, 8,  unsigned long)
#define VPU_IOC_WR_DEC_PP       _IOW(VPU_IOC_MAGIC, 9,  unsigned long)
#define VPU_IOC_WR_ENC          _IOW(VPU_IOC_MAGIC, 10, unsigned long)
#define VPU_IOC_WR_PP           _IOW(VPU_IOC_MAGIC, 11, unsigned long)

#define VPU_IOC_RD_DEC          _IOW(VPU_IOC_MAGIC, 12, unsigned long)
#define VPU_IOC_RD_DEC_PP       _IOW(VPU_IOC_MAGIC, 13, unsigned long)
#define VPU_IOC_RD_ENC          _IOW(VPU_IOC_MAGIC, 14, unsigned long)
#define VPU_IOC_RD_PP           _IOW(VPU_IOC_MAGIC, 15, unsigned long)

#define VPU_IOC_CLS_IRQ         _IOW(VPU_IOC_MAGIC, 16, unsigned long)


#define DWL_MPEG2_E         31  /* 1 bit */
#define DWL_VC1_E           29  /* 2 bits */
#define DWL_JPEG_E          28  /* 1 bit */
#define DWL_MPEG4_E         26  /* 2 bits */
#define DWL_H264_E          24  /* 2 bits */
#define DWL_VP6_E           23  /* 1 bit */
#define DWL_PJPEG_E         22  /* 1 bit */
#define DWL_REF_BUFF_E      20  /* 1 bit */

#define DWL_JPEG_EXT_E          31  /* 1 bit */
#define DWL_REF_BUFF_ILACE_E    30  /* 1 bit */
#define DWL_MPEG4_CUSTOM_E      29  /* 1 bit */
#define DWL_REF_BUFF_DOUBLE_E   28  /* 1 bit */
#define DWL_RV_E            26  /* 2 bits */
#define DWL_VP7_E           24  /* 1 bit */
#define DWL_VP8_E           23  /* 1 bit */
#define DWL_AVS_E           22  /* 1 bit */
#define DWL_MVC_E           20  /* 2 bits */

#define DWL_CFG_E           24  /* 4 bits */
#define DWL_PP_E            16  /* 1 bit */

#define DWL_SORENSONSPARK_E 11  /* 1 bit */

#define DWL_H264_FUSE_E          31 /* 1 bit */
#define DWL_MPEG4_FUSE_E         30 /* 1 bit */
#define DWL_MPEG2_FUSE_E         29 /* 1 bit */
#define DWL_SORENSONSPARK_FUSE_E 28 /* 1 bit */
#define DWL_JPEG_FUSE_E          27 /* 1 bit */
#define DWL_VP6_FUSE_E           26 /* 1 bit */
#define DWL_VC1_FUSE_E           25 /* 1 bit */
#define DWL_PJPEG_FUSE_E         24 /* 1 bit */
#define DWL_CUSTOM_MPEG4_FUSE_E  23 /* 1 bit */
#define DWL_RV_FUSE_E            22 /* 1 bit */
#define DWL_VP7_FUSE_E           21 /* 1 bit */
#define DWL_VP8_FUSE_E           20 /* 1 bit */
#define DWL_AVS_FUSE_E           19 /* 1 bit */
#define DWL_MVC_FUSE_E           18 /* 1 bit */

#define DWL_DEC_MAX_1920_FUSE_E  15 /* 1 bit */
#define DWL_DEC_MAX_1280_FUSE_E  14 /* 1 bit */
#define DWL_DEC_MAX_720_FUSE_E   13 /* 1 bit */
#define DWL_DEC_MAX_352_FUSE_E   12 /* 1 bit */
#define DWL_REF_BUFF_FUSE_E       7 /* 1 bit */


#define DWL_PP_FUSE_E    31  /* 1 bit */
#define DWL_PP_DEINTERLACE_FUSE_E   30  /* 1 bit */
#define DWL_PP_ALPHA_BLEND_FUSE_E   29  /* 1 bit */
#define DWL_PP_MAX_1920_FUSE_E  15  /* 1 bit */
#define DWL_PP_MAX_1280_FUSE_E  14  /* 1 bit */
#define DWL_PP_MAX_720_FUSE_E  13  /* 1 bit */
#define DWL_PP_MAX_352_FUSE_E  12  /* 1 bit */


#define MPEG4_NOT_SUPPORTED             (RK_U32)(0x00)
#define MPEG4_SIMPLE_PROFILE            (RK_U32)(0x01)
#define MPEG4_ADVANCED_SIMPLE_PROFILE   (RK_U32)(0x02)
#define MPEG4_CUSTOM_NOT_SUPPORTED      (RK_U32)(0x00)
#define MPEG4_CUSTOM_FEATURE_1          (RK_U32)(0x01)
#define H264_NOT_SUPPORTED              (RK_U32)(0x00)
#define H264_BASELINE_PROFILE           (RK_U32)(0x01)
#define H264_MAIN_PROFILE               (RK_U32)(0x02)
#define H264_HIGH_PROFILE               (RK_U32)(0x03)
#define VC1_NOT_SUPPORTED               (RK_U32)(0x00)
#define VC1_SIMPLE_PROFILE              (RK_U32)(0x01)
#define VC1_MAIN_PROFILE                (RK_U32)(0x02)
#define VC1_ADVANCED_PROFILE            (RK_U32)(0x03)
#define MPEG2_NOT_SUPPORTED             (RK_U32)(0x00)
#define MPEG2_MAIN_PROFILE              (RK_U32)(0x01)
#define JPEG_NOT_SUPPORTED              (RK_U32)(0x00)
#define JPEG_BASELINE                   (RK_U32)(0x01)
#define JPEG_PROGRESSIVE                (RK_U32)(0x02)
#define PP_NOT_SUPPORTED                (RK_U32)(0x00)
#define PP_SUPPORTED                    (RK_U32)(0x01)
#define PP_DITHERING                    (RK_U32)(0x10000000)
#define PP_SCALING                      (RK_U32)(0x0C000000)
#define PP_DEINTERLACING                (RK_U32)(0x02000000)
#define PP_ALPHA_BLENDING               (RK_U32)(0x01000000)
#define SORENSON_SPARK_NOT_SUPPORTED    (RK_U32)(0x00)
#define SORENSON_SPARK_SUPPORTED        (RK_U32)(0x01)
#define VP6_NOT_SUPPORTED               (RK_U32)(0x00)
#define VP6_SUPPORTED                   (RK_U32)(0x01)
#define VP7_NOT_SUPPORTED               (RK_U32)(0x00)
#define VP7_SUPPORTED                   (RK_U32)(0x01)
#define VP8_NOT_SUPPORTED               (RK_U32)(0x00)
#define VP8_SUPPORTED                   (RK_U32)(0x01)
#define REF_BUF_NOT_SUPPORTED           (RK_U32)(0x00)
#define REF_BUF_SUPPORTED               (RK_U32)(0x01)
#define REF_BUF_INTERLACED              (RK_U32)(0x02)
#define REF_BUF_DOUBLE                  (RK_U32)(0x04)
#define AVS_NOT_SUPPORTED               (RK_U32)(0x00)
#define AVS_SUPPORTED                   (RK_U32)(0x01)
#define JPEG_EXT_NOT_SUPPORTED          (RK_U32)(0x00)
#define JPEG_EXT_SUPPORTED              (RK_U32)(0x01)
#define RV_NOT_SUPPORTED                (RK_U32)(0x00)
#define RV_SUPPORTED                    (RK_U32)(0x01)
#define MVC_NOT_SUPPORTED               (RK_U32)(0x00)
#define MVC_SUPPORTED                   (RK_U32)(0x01)

#define H264_NOT_SUPPORTED_FUSE             (RK_U32)(0x00)
#define H264_FUSE_ENABLED                   (RK_U32)(0x01)
#define MPEG4_NOT_SUPPORTED_FUSE            (RK_U32)(0x00)
#define MPEG4_FUSE_ENABLED                  (RK_U32)(0x01)
#define MPEG2_NOT_SUPPORTED_FUSE            (RK_U32)(0x00)
#define MPEG2_FUSE_ENABLED                  (RK_U32)(0x01)
#define SORENSON_SPARK_NOT_SUPPORTED_FUSE   (RK_U32)(0x00)
#define SORENSON_SPARK_ENABLED              (RK_U32)(0x01)
#define JPEG_NOT_SUPPORTED_FUSE             (RK_U32)(0x00)
#define JPEG_FUSE_ENABLED                   (RK_U32)(0x01)
#define VP6_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define VP6_FUSE_ENABLED                    (RK_U32)(0x01)
#define VP7_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define VP7_FUSE_ENABLED                    (RK_U32)(0x01)
#define VP8_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define VP8_FUSE_ENABLED                    (RK_U32)(0x01)
#define VC1_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define VC1_FUSE_ENABLED                    (RK_U32)(0x01)
#define JPEG_PROGRESSIVE_NOT_SUPPORTED_FUSE (RK_U32)(0x00)
#define JPEG_PROGRESSIVE_FUSE_ENABLED       (RK_U32)(0x01)
#define REF_BUF_NOT_SUPPORTED_FUSE          (RK_U32)(0x00)
#define REF_BUF_FUSE_ENABLED                (RK_U32)(0x01)
#define AVS_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define AVS_FUSE_ENABLED                    (RK_U32)(0x01)
#define RV_NOT_SUPPORTED_FUSE               (RK_U32)(0x00)
#define RV_FUSE_ENABLED                     (RK_U32)(0x01)
#define MVC_NOT_SUPPORTED_FUSE              (RK_U32)(0x00)
#define MVC_FUSE_ENABLED                    (RK_U32)(0x01)

#define PP_NOT_SUPPORTED_FUSE               (RK_U32)(0x00)
#define PP_FUSE_ENABLED                     (RK_U32)(0x01)
#define PP_FUSE_DEINTERLACING_ENABLED       (RK_U32)(0x40000000)
#define PP_FUSE_ALPHA_BLENDING_ENABLED      (RK_U32)(0x20000000)
#define MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED  (RK_U32)(0x00008000)
#define MAX_PP_OUT_WIDHT_1280_FUSE_ENABLED  (RK_U32)(0x00004000)
#define MAX_PP_OUT_WIDHT_720_FUSE_ENABLED   (RK_U32)(0x00002000)
#define MAX_PP_OUT_WIDHT_352_FUSE_ENABLED   (RK_U32)(0x00001000)




#define VPU_MODULE_PATH         "/dev/vpu"
#define VPU_SERVICE_PATH        "/dev/vpu_service"
// 编码器： 96 个寄存器，大小 384B
#define VPU_REG_ENC_SIZE        (0x180)
// 编码器：100 个寄存器，大小 400B
#define VPU_REG_DEC_SIZE        (0x194)

#define VPU_FD_ISR              0
#define VPU_FD_CONNECT          1

#define VPU_TASK_ID_MIN         2
#define VPU_TASK_ID_MAX         10

#define VPU_FD_TASK_NUM         8
#define VPU_FD_MAX_NUM          10



#define USE_CODEC(task)             (((task->client_type)&VPU_DEC)||((task->client_type)==VPU_ENC))
#define USE_PPROC(task)             ((task->client_type)&VPU_PP)
#define CODEC_USED(p)               (NULL != p->codec)
#define PPROC_USED(p)               (NULL != p->pproc)


#define BIT(nr)                     (1UL << (nr))

#define IRQ_DEC_READY               BIT(12)
#define IRQ_DEC_BUS_ERROR           BIT(13)
#define IRQ_DEC_BUFFER_EMPTY        BIT(14)
#define IRQ_DEC_ASO                 BIT(15)
#define IRQ_DEC_STREAM_ERROR        BIT(16)
#define IRQ_DEC_TIMEOUT             BIT(18)
#define IRQ_DEC_PIC_INF             BIT(24)
#define IRQ_DEC_ERROR               (IRQ_DEC_BUS_ERROR|IRQ_DEC_ASO|IRQ_DEC_STREAM_ERROR|IRQ_DEC_TIMEOUT)

#define IRQ_PP_READY                BIT(12)
#define IRQ_PP_BUS_ERROR            BIT(13)

#define IRQ_ENC_DISABLE             BIT(1)
#define IRQ_ENC_READY               BIT(2)
#define IRQ_ENC_BUS_ERROR           BIT(3)
#define IRQ_ENC_RESET               BIT(4)
#define IRQ_ENC_BUFFER_FULL         BIT(5)
#define IRQ_ENC_TEST1               BIT(6)
#define IRQ_ENC_TEST2               BIT(7)
#define IRQ_ENC_INTERVAL            BIT(8)
#define IRQ_ENC_ERROR               (IRQ_ENC_BUS_ERROR|IRQ_ENC_BUFFER_FULL)


#define VPU_IRQ_EVENT_DEC_BIT       BIT(0)
#define VPU_IRQ_EVENT_DEC_IRQ_BIT   BIT(1)
#define VPU_IRQ_EVENT_PP_IRQ_BIT    BIT(2)
#define VPU_IRQ_EVENT_ENC_BIT       BIT(8)
#define VPU_IRQ_EVENT_ENC_IRQ_BIT   BIT(9)


#define VPU_REG_BASE_DEC            0x80
#define VPU_REG_BASE_PP             0xBC
#define VPU_REG_BASE_DEC_PP         VPU_REG_BASE_DEC
#define VPU_REG_BASE_ENC            0x00

#define VPU_REG_IRQS_DEC            1
#define VPU_REG_IRQS_PP             0xBC
#define VPU_REG_IRQS_DEC_PP         VPU_REG_IRQS_DEC
#define VPU_REG_IRQS_ENC            0x01

#define VPU_DEC_HWCFG0              50
#define VPU_DEC_HWCFG1              51
#define VPU_DEC_HW_FUSE_CFG         57
#define VPU_PP_HW_SYNTH_CFG         40
#define VPU_PP_HW_FUSE_CFG          41

#define SIZE_REG(reg)               ((reg)*4)

typedef struct VPU_REG_SET
{
    RK_U32 reg_base_in_u32;
    RK_U32 reg_size_in_u32;
    RK_U32 irq_status_pos;
} VPU_REG_SET_S;

typedef enum
{
    VPU_DEC_ID_9190 = 0x6731,
    VPU_ID_8270     = 0x8270,
    VPU_ID_4831     = 0x4831,
} VPU_HW_ID;

typedef struct
{
    VPU_HW_ID           hw_id;
    unsigned long       enc_io_size;
    unsigned long       dec_io_size;
} VPU_HW_INFO_E;

#define REG_NUM_ENC_8270            (96)
#define REG_NUM_ENC_4831            (164)

#define REG_NUM_9190_DEC            (60)
#define REG_NUM_9190_PP             (41)
#define REG_NUM_9190_DEC_PP         (REG_NUM_9190_DEC+REG_NUM_9190_PP)

VPU_HW_INFO_E vpu_hw_set[] =
{
    [0] = {
        .hw_id          = VPU_ID_8270,
        .enc_io_size    = REG_NUM_ENC_8270 * 4,
        .dec_io_size    = REG_NUM_9190_DEC_PP * 4,
    },
    [1] = {
        .hw_id          = VPU_ID_4831,
        .enc_io_size    = REG_NUM_ENC_4831 * 4,
        .dec_io_size    = REG_NUM_9190_DEC_PP * 4,
    },
};


#if 0
VPU_REG_SET_S vpu_reg[VPU_TYPE_BUTT] =
{
    {VPU_REG_BASE_ENC,      VPU_REG_NUM_ENC,    VPU_REG_IRQS_ENC},
    {VPU_REG_BASE_DEC,      VPU_REG_NUM_DEC,    VPU_REG_IRQS_DEC},
    {VPU_REG_BASE_PP,       VPU_REG_NUM_PP,     VPU_REG_IRQS_PP },
    {VPU_REG_BASE_DEC_PP,   VPU_REG_NUM_DEC_PP, VPU_REG_IRQS_DEC_PP},
};
#endif

typedef struct VPUHwFuseStatus
{
    RK_U32 h264SupportFuse;            /* HW supports h.264 */
    RK_U32 mpeg4SupportFuse;           /* HW supports MPEG-4 */
    RK_U32 mpeg2SupportFuse;           /* HW supports MPEG-2 */
    RK_U32 sorensonSparkSupportFuse;   /* HW supports Sorenson Spark */
    RK_U32 jpegSupportFuse;            /* HW supports JPEG */
    RK_U32 vp6SupportFuse;             /* HW supports VP6 */
    RK_U32 vp7SupportFuse;             /* HW supports VP6 */
    RK_U32 vp8SupportFuse;             /* HW supports VP6 */
    RK_U32 vc1SupportFuse;             /* HW supports VC-1 Simple */
    RK_U32 jpegProgSupportFuse;        /* HW supports Progressive JPEG */
    RK_U32 ppSupportFuse;              /* HW supports post-processor */
    RK_U32 ppConfigFuse;               /* HW post-processor functions bitmask */
    RK_U32 maxDecPicWidthFuse;         /* Maximum video decoding width supported  */
    RK_U32 maxPpOutPicWidthFuse;       /* Maximum output width of Post-Processor */
    RK_U32 refBufSupportFuse;          /* HW supports reference picture buffering */
    RK_U32 avsSupportFuse;             /* one of the AVS values defined above */
    RK_U32 rvSupportFuse;              /* one of the REAL values defined above */
    RK_U32 mvcSupportFuse;
    RK_U32 customMpeg4SupportFuse;     /* Fuse for custom MPEG-4 */

} VPUHwFuseStatus_t;

// 编码器硬件启动寄存器
// 其它硬件启动寄存器
#define VPU_REG_EN_RES              1

typedef struct VPU_TASK
{

    struct list_head list;
    VPU_CLIENT_TYPE client_type;
    RK_S32 fds_id;
    int sock;
    int pid;

    // 只更新必要区域的寄存器
    //RK_U32 reg_base;
    //RK_U32 reg_size;
    //RK_U32 reg_irqs;
} VPU_TASK_S;

/* vpu message header */

typedef struct VPU_MSG_HEADER
{
    RK_U8  magic;
    RK_U8  cmd;
    RK_U16 len;
} VPU_MSG_HEADER_S;

typedef struct VPU_INFO
{
    int     fd_vpu;
    RK_U32  power_on;

    RK_U32 *VPURegMap;
    RK_S32  fds_used;

    VPU_TASK_S *codec;          /* current running codec task */
    VPU_TASK_S *pproc;          /* current running pproc task */
    VPU_TASK_S *resev;          /* current running resev task */
    VPU_TASK_S fds_list;

    VPUHwDecConfig_t DecHwCfg;  /* decoder hardware configuration */
    VPUHwEncConfig_t EncHwCfg;  /* encoder hardware configuration */

    RK_U32  VPURegEnc[VPU_REG_NUM_ENC];
    RK_U32  VPURegDec[VPU_REG_NUM_DEC_PP];
    RK_U32 *VPURegPp;

    RK_U32 msg[128];
} VPU_INFO_S;

#define VPU_ERROR(e) \
    do { \
        err = (e); \
        goto END; \
    } while (0)

typedef struct VPU_FD_LIST_S
{
    struct list_head list;
    struct pollfd fd;
} VPU_FD_LIST;

static struct pollfd fds[VPU_FD_MAX_NUM];

static int vpu_service_check_hw(RK_U32 *reg)
{
    int ret = 0, i = 0;
    RK_U32 enc_id = reg[0];
    enc_id = (enc_id >> 16) & 0xFFFF;
    for (i = 0; i < sizeof(vpu_hw_set)/sizeof(vpu_hw_set[0]); i++)
    {
        if (enc_id == vpu_hw_set[i].hw_id)
        {
            ret = (vpu_hw_set[i].dec_io_size  >  vpu_hw_set[i].enc_io_size) ?
                  (vpu_hw_set[i].dec_io_size) : (vpu_hw_set[i].enc_io_size);
            break;
        }
    }
    return ret;
}

RK_S32 vpu_power_on(VPU_INFO_S *p)
{
    if (!p->power_on)
    {
        ALOGD("VPU: vpu_power_on %d\n", p->power_on);
        if (ioctl(p->fd_vpu, VPU_IOC_CLOCK_ON))
        {
            ALOGE("VPU: VPU_IOC_POWER_ON failed\n");
            return VPU_FAILURE;
        }
    }
    p->power_on = 1;
    return VPU_SUCCESS;
}

RK_S32 vpu_power_off(VPU_INFO_S *p)
{
    if (p->power_on)
    {
        ALOGD("VPU: vpu_power_off %d\n", p->power_on);
        if (ioctl(p->fd_vpu, VPU_IOC_CLOCK_OFF))
        {
            ALOGE("VPU: VPU_IOC_POWER_OFF failed\n");
            return VPU_FAILURE;
        }
    }
    p->power_on = 0;
    return VPU_SUCCESS;
}

RK_S32 vpu_get_dec_info(VPU_INFO_S *p)
{
    VPUHwDecConfig_t *pHwCfg = &p->DecHwCfg;
    RK_U32 configReg   = p->VPURegDec[VPU_DEC_HWCFG0];
    RK_U32 asicID      = p->VPURegDec[0];

    pHwCfg->h264Support = (configReg >> DWL_H264_E) & 0x3U;
    /* check jpeg */
    pHwCfg->jpegSupport = (configReg >> DWL_JPEG_E) & 0x01U;

    if (pHwCfg->jpegSupport && ((configReg >> DWL_PJPEG_E) & 0x01U))
        pHwCfg->jpegSupport = JPEG_PROGRESSIVE;

    pHwCfg->mpeg4Support = (configReg >> DWL_MPEG4_E) & 0x3U;

    pHwCfg->vc1Support = (configReg >> DWL_VC1_E) & 0x3U;

    pHwCfg->mpeg2Support = (configReg >> DWL_MPEG2_E) & 0x01U;

    pHwCfg->sorensonSparkSupport = (configReg >> DWL_SORENSONSPARK_E) & 0x01U;

    pHwCfg->refBufSupport = (configReg >> DWL_REF_BUFF_E) & 0x01U;

    pHwCfg->vp6Support = (configReg >> DWL_VP6_E) & 0x01U;

    pHwCfg->maxDecPicWidth = configReg & 0x07FFU;

    /* 2nd Config register */
    configReg = p->VPURegDec[VPU_DEC_HWCFG1];

    if (pHwCfg->refBufSupport)
    {
        if ((configReg >> DWL_REF_BUFF_ILACE_E) & 0x01U)
            pHwCfg->refBufSupport |= 2;

        if ((configReg >> DWL_REF_BUFF_DOUBLE_E) & 0x01U)
            pHwCfg->refBufSupport |= 4;
    }

    pHwCfg->customMpeg4Support = (configReg >> DWL_MPEG4_CUSTOM_E) & 0x01U;

    pHwCfg->vp7Support = (configReg >> DWL_VP7_E) & 0x01U;
    pHwCfg->vp8Support = (configReg >> DWL_VP8_E) & 0x01U;
    pHwCfg->avsSupport = (configReg >> DWL_AVS_E) & 0x01U;

    /* JPEG xtensions */

    if (((asicID >> 16) >= 0x8190U) ||
            ((asicID >> 16) == 0x6731U) )
        pHwCfg->jpegESupport = (configReg >> DWL_JPEG_EXT_E) & 0x01U;
    else
        pHwCfg->jpegESupport = JPEG_EXT_NOT_SUPPORTED;

    if (((asicID >> 16) >= 0x9170U) ||
            ((asicID >> 16) == 0x6731U) )
        pHwCfg->rvSupport = (configReg >> DWL_RV_E) & 0x03U;
    else
        pHwCfg->rvSupport = RV_NOT_SUPPORTED;

    pHwCfg->mvcSupport = (configReg >> DWL_MVC_E) & 0x03U;

    if (pHwCfg->refBufSupport &&
            (asicID >> 16) == 0x6731U )
    {
        pHwCfg->refBufSupport |= 8; /* enable HW support for offset */
    }

    {
        VPUHwFuseStatus_t hwFuseSts;
        /* Decoder fuse configuration */
        RK_U32 fuseReg = p->VPURegDec[VPU_DEC_HW_FUSE_CFG];

        hwFuseSts.h264SupportFuse = (fuseReg >> DWL_H264_FUSE_E) & 0x01U;
        hwFuseSts.mpeg4SupportFuse = (fuseReg >> DWL_MPEG4_FUSE_E) & 0x01U;
        hwFuseSts.mpeg2SupportFuse = (fuseReg >> DWL_MPEG2_FUSE_E) & 0x01U;
        hwFuseSts.sorensonSparkSupportFuse =
            (fuseReg >> DWL_SORENSONSPARK_FUSE_E) & 0x01U;
        hwFuseSts.jpegSupportFuse = (fuseReg >> DWL_JPEG_FUSE_E) & 0x01U;
        hwFuseSts.vp6SupportFuse = (fuseReg >> DWL_VP6_FUSE_E) & 0x01U;
        hwFuseSts.vc1SupportFuse = (fuseReg >> DWL_VC1_FUSE_E) & 0x01U;
        hwFuseSts.jpegProgSupportFuse = (fuseReg >> DWL_PJPEG_FUSE_E) & 0x01U;
        hwFuseSts.rvSupportFuse = (fuseReg >> DWL_RV_FUSE_E) & 0x01U;
        hwFuseSts.avsSupportFuse = (fuseReg >> DWL_AVS_FUSE_E) & 0x01U;
        hwFuseSts.vp7SupportFuse = (fuseReg >> DWL_VP7_FUSE_E) & 0x01U;
        hwFuseSts.vp8SupportFuse = (fuseReg >> DWL_VP8_FUSE_E) & 0x01U;
        hwFuseSts.customMpeg4SupportFuse = (fuseReg >> DWL_CUSTOM_MPEG4_FUSE_E) & 0x01U;
        hwFuseSts.mvcSupportFuse = (fuseReg >> DWL_MVC_FUSE_E) & 0x01U;

        /* check max. decoder output width */

        if (fuseReg & 0x8000U)
            hwFuseSts.maxDecPicWidthFuse = 1920;
        else if (fuseReg & 0x4000U)
            hwFuseSts.maxDecPicWidthFuse = 1280;
        else if (fuseReg & 0x2000U)
            hwFuseSts.maxDecPicWidthFuse = 720;
        else if (fuseReg & 0x1000U)
            hwFuseSts.maxDecPicWidthFuse = 352;
        else    /* remove warning */
            hwFuseSts.maxDecPicWidthFuse = 352;

        hwFuseSts.refBufSupportFuse = (fuseReg >> DWL_REF_BUFF_FUSE_E) & 0x01U;

        /* Pp configuration */
        configReg = p->VPURegPp[VPU_PP_HW_SYNTH_CFG];

        if ((configReg >> DWL_PP_E) & 0x01U)
        {
            pHwCfg->ppSupport = 1;
            pHwCfg->maxPpOutPicWidth = configReg & 0x07FFU;
            /*pHwCfg->ppConfig = (configReg >> DWL_CFG_E) & 0x0FU; */
            pHwCfg->ppConfig = configReg;

        }
        else
        {
            pHwCfg->ppSupport = 0;
            pHwCfg->maxPpOutPicWidth = 0;
            pHwCfg->ppConfig = 0;
        }

        /* check the HW versio */
        if (((asicID >> 16) >= 0x8190U) ||
                ((asicID >> 16) == 0x6731U) )
        {
            /* check fuse status */
            fuseReg = p->VPURegDec[VPU_DEC_HW_FUSE_CFG];

            //DWLReadAsicFuseStatus(&hwFuseSts);
            hwFuseSts.h264SupportFuse   = (fuseReg >> DWL_H264_FUSE_E) & 0x01U;
            hwFuseSts.mpeg4SupportFuse  = (fuseReg >> DWL_MPEG4_FUSE_E) & 0x01U;
            hwFuseSts.mpeg2SupportFuse  = (fuseReg >> DWL_MPEG2_FUSE_E) & 0x01U;
            hwFuseSts.sorensonSparkSupportFuse =
                (fuseReg >> DWL_SORENSONSPARK_FUSE_E) & 0x01U;
            hwFuseSts.jpegSupportFuse   = (fuseReg >> DWL_JPEG_FUSE_E) & 0x01U;
            hwFuseSts.vp6SupportFuse    = (fuseReg >> DWL_VP6_FUSE_E) & 0x01U;
            hwFuseSts.vc1SupportFuse    = (fuseReg >> DWL_VC1_FUSE_E) & 0x01U;
            hwFuseSts.jpegProgSupportFuse = (fuseReg >> DWL_PJPEG_FUSE_E) & 0x01U;
            hwFuseSts.rvSupportFuse     = (fuseReg >> DWL_RV_FUSE_E) & 0x01U;
            hwFuseSts.avsSupportFuse    = (fuseReg >> DWL_AVS_FUSE_E) & 0x01U;
            hwFuseSts.vp7SupportFuse    = (fuseReg >> DWL_VP7_FUSE_E) & 0x01U;
            hwFuseSts.vp8SupportFuse    = (fuseReg >> DWL_VP8_FUSE_E) & 0x01U;
            hwFuseSts.customMpeg4SupportFuse = (fuseReg >> DWL_CUSTOM_MPEG4_FUSE_E) & 0x01U;
            hwFuseSts.mvcSupportFuse    = (fuseReg >> DWL_MVC_FUSE_E) & 0x01U;

            /* check max. decoder output width */

            if (fuseReg & 0x8000U)
                hwFuseSts.maxDecPicWidthFuse = 1920;
            else if (fuseReg & 0x4000U)
                hwFuseSts.maxDecPicWidthFuse = 1280;
            else if (fuseReg & 0x2000U)
                hwFuseSts.maxDecPicWidthFuse = 720;
            else if (fuseReg & 0x1000U)
                hwFuseSts.maxDecPicWidthFuse = 352;

            hwFuseSts.refBufSupportFuse = (fuseReg >> DWL_REF_BUFF_FUSE_E) & 0x01U;

            /* Pp configuration */
            configReg = p->VPURegDec[VPU_DEC_HW_FUSE_CFG];

            if ((configReg >> DWL_PP_E) & 0x01U)
            {
                /* Pp fuse configuration */
                RK_U32 fuseRegPp = p->VPURegPp[VPU_PP_HW_FUSE_CFG];

                if ((fuseRegPp >> DWL_PP_FUSE_E) & 0x01U)
                {
                    hwFuseSts.ppSupportFuse = 1;

                    /* check max. pp output width */

                    if (fuseRegPp & 0x8000U)
                        hwFuseSts.maxPpOutPicWidthFuse = 1920;
                    else if (fuseRegPp & 0x4000U)
                        hwFuseSts.maxPpOutPicWidthFuse = 1280;
                    else if (fuseRegPp & 0x2000U)
                        hwFuseSts.maxPpOutPicWidthFuse = 720;
                    else if (fuseRegPp & 0x1000U)
                        hwFuseSts.maxPpOutPicWidthFuse = 352;
                    else        /* remove warning */
                        hwFuseSts.maxPpOutPicWidthFuse = 352;

                    hwFuseSts.ppConfigFuse = fuseRegPp;
                }
                else
                {
                    hwFuseSts.ppSupportFuse = 0;
                    hwFuseSts.maxPpOutPicWidthFuse = 0;
                    hwFuseSts.ppConfigFuse = 0;
                }
            }
            else
            {
                hwFuseSts.ppSupportFuse = 0;
                hwFuseSts.maxPpOutPicWidthFuse = 0;
                hwFuseSts.ppConfigFuse = 0;
            }

            /* Maximum decoding width supported by the HW */
            if (pHwCfg->maxDecPicWidth > hwFuseSts.maxDecPicWidthFuse)
                pHwCfg->maxDecPicWidth = hwFuseSts.maxDecPicWidthFuse;

            /* Maximum output width of Post-Processor */
            if (pHwCfg->maxPpOutPicWidth > hwFuseSts.maxPpOutPicWidthFuse)
                pHwCfg->maxPpOutPicWidth = hwFuseSts.maxPpOutPicWidthFuse;

            /* h264 */
            if (!hwFuseSts.h264SupportFuse)
                pHwCfg->h264Support = H264_NOT_SUPPORTED;

            /* mpeg-4 */
            if (!hwFuseSts.mpeg4SupportFuse)
                pHwCfg->mpeg4Support = MPEG4_NOT_SUPPORTED;

            /* custom mpeg-4 */
            if (!hwFuseSts.customMpeg4SupportFuse)
                pHwCfg->customMpeg4Support = MPEG4_CUSTOM_NOT_SUPPORTED;

            /* jpeg (baseline && progressive) */
            if (!hwFuseSts.jpegSupportFuse)
                pHwCfg->jpegSupport = JPEG_NOT_SUPPORTED;

            if ((pHwCfg->jpegSupport == JPEG_PROGRESSIVE) &&
                    !hwFuseSts.jpegProgSupportFuse)
                pHwCfg->jpegSupport = JPEG_BASELINE;

            /* mpeg-2 */
            if (!hwFuseSts.mpeg2SupportFuse)
                pHwCfg->mpeg2Support = MPEG2_NOT_SUPPORTED;

            /* vc-1 */
            if (!hwFuseSts.vc1SupportFuse)
                pHwCfg->vc1Support = VC1_NOT_SUPPORTED;

            /* vp6 */
            if (!hwFuseSts.vp6SupportFuse)
                pHwCfg->vp6Support = VP6_NOT_SUPPORTED;

            /* vp7 */
            if (!hwFuseSts.vp7SupportFuse)
                pHwCfg->vp7Support = VP7_NOT_SUPPORTED;

            /* vp6 */
            if (!hwFuseSts.vp8SupportFuse)
                pHwCfg->vp8Support = VP8_NOT_SUPPORTED;

            /* pp */
            if (!hwFuseSts.ppSupportFuse)
                pHwCfg->ppSupport = PP_NOT_SUPPORTED;

            /* check the pp config vs fuse status */
            if ((pHwCfg->ppConfig & 0xFC000000) &&
                    ((hwFuseSts.ppConfigFuse & 0xF0000000) >> 5))
            {
                /* config */
                RK_U32 deInterlace = ((pHwCfg->ppConfig & PP_DEINTERLACING) >> 25);
                RK_U32 alphaBlend  = ((pHwCfg->ppConfig & PP_ALPHA_BLENDING) >> 24);
                /* fuse */
                RK_U32 deInterlaceFuse =
                    (((hwFuseSts.ppConfigFuse >> 5) & PP_DEINTERLACING) >> 25);
                RK_U32 alphaBlendFuse  =
                    (((hwFuseSts.ppConfigFuse >> 5) & PP_ALPHA_BLENDING) >> 24);

                /* check if */

                if (deInterlace && !deInterlaceFuse)
                    pHwCfg->ppConfig &= 0xFD000000;

                if (alphaBlend && !alphaBlendFuse)
                    pHwCfg->ppConfig &= 0xFE000000;
            }

            /* sorenson */
            if (!hwFuseSts.sorensonSparkSupportFuse)
                pHwCfg->sorensonSparkSupport = SORENSON_SPARK_NOT_SUPPORTED;

            /* ref. picture buffer */
            if (!hwFuseSts.refBufSupportFuse)
                pHwCfg->refBufSupport = REF_BUF_NOT_SUPPORTED;

            /* rv */
            if (!hwFuseSts.rvSupportFuse)
                pHwCfg->rvSupport = RV_NOT_SUPPORTED;

            /* avs */
            if (!hwFuseSts.avsSupportFuse)
                pHwCfg->avsSupport = AVS_NOT_SUPPORTED;

            /* mvc */
            if (!hwFuseSts.mvcSupportFuse)
                pHwCfg->mvcSupport = MVC_NOT_SUPPORTED;
        }
    }

    return VPU_SUCCESS;
}

RK_S32 vpu_get_enc_info(VPU_INFO_S *p)
{
    VPUHwEncConfig_t *pHwCfg = &p->EncHwCfg;
    RK_U32 *pRegs = p->VPURegEnc;
    RK_U32 cfgval = pRegs[63];
    static const char *busTypeName[5] = { "UNKNOWN", "AHB", "OCP", "AXI", "PCI" };
    static const char *synthLangName[3] = { "UNKNOWN", "VHDL", "VERIALOG" };

    pHwCfg->maxEncodedWidth = cfgval & ((1 << 11) - 1);
    pHwCfg->h264Enabled = (cfgval >> 27) & 1;
    pHwCfg->mpeg4Enabled = (cfgval >> 26) & 1;
    pHwCfg->jpegEnabled = (cfgval >> 25) & 1;
    pHwCfg->vsEnabled = (cfgval >> 24) & 1;
    pHwCfg->rgbEnabled = (cfgval >> 28) & 1;

    pHwCfg->reg_size = vpu_service_check_hw(pRegs);
    pHwCfg->reserv[0] = pHwCfg->reserv[1] = 0;

    ALOGI("EWLReadAsicConfig:\n"
          "    maxEncodedWidth   = %d\n"
          "    h264Enabled       = %s\n"
          "    jpegEnabled       = %s\n"
          "    mpeg4Enabled      = %s\n"
          "    vsEnabled         = %s\n"
          "    rgbEnabled        = %s\n",
          pHwCfg->maxEncodedWidth,
          pHwCfg->h264Enabled == 1 ? "YES" : "NO",
          pHwCfg->jpegEnabled == 1 ? "YES" : "NO",
          pHwCfg->mpeg4Enabled == 1 ? "YES" : "NO",
          pHwCfg->vsEnabled == 1 ? "YES" : "NO",
          pHwCfg->rgbEnabled == 1 ? "YES" : "NO");

    return VPU_SUCCESS;
}

RK_S32 vpu_mmap_and_get_info(VPU_INFO_S *p, int fd)
{
    const int pageSize = getpagesize();
    const int pageAlignment = pageSize - 1;

    RK_S32 err = VPU_SUCCESS;;

    ALOGI("VPU: ioctl get offset done\n");

    p->fd_vpu = fd;

#if 0
    {
        unsigned long *regBase_vir = MAP_FAILED;
        size_t mapSize = pageSize;    // 注意，这里很暴力的做法，一次 map 4k 上来，即 0x1000

        /* map page aligned base */
        regBase_vir = (unsigned long *) mmap(0, mapSize, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, fd, 0);

        /* add offset from alignment to the io start address */

        if (regBase_vir == MAP_FAILED)
        {
            ALOGE("VPU: failed to mmap regs\n");
            VPU_ERROR(-4);
        }
        else
        {
            p->VPURegMap = (RK_U32*)(regBase_vir);
        }
    }

    ALOGI("VPU: VPURegMap : 0x%.08x mmap done\n", (RK_U32)p->VPURegMap);
#endif

    if (err = vpu_power_on(p))
    {
        ALOGE("VPU: vpu_power_on failed ret %d\n", err);
        VPU_ERROR(-1);
    }

    if (err = ioctl(p->fd_vpu, VPU_IOC_RD_ENC, (RK_U32)&p->VPURegEnc[0]))
    {
        ALOGE("VPU: VPU_IOC_RD_ENC failed \n");
        VPU_ERROR(-5);
    }

    if (ioctl(p->fd_vpu, VPU_IOC_RD_DEC_PP, (RK_U32)&p->VPURegDec[0]))
    {
        ALOGE("VPU: VPU_IOC_RD_ENC failed\n");
        VPU_ERROR(-6);
    }

    p->VPURegPp = &p->VPURegDec[VPU_REG_NUM_DEC];

    /* read hardware configuration */
    vpu_get_dec_info(p);

    ALOGI("VPU: vpu_get_dec_info done\n");

    vpu_get_enc_info(p);

    ALOGI("VPU: vpu_get_enc_info done\n");


END:
    vpu_power_off(p);
    return err;
}

#if 0
RK_S32 vpu_unmap(VPU_INFO_S *p)
{
    const int pageSize = getpagesize();
    const int pageAlignment = pageSize - 1;

    // 注：EncRegMap 是起始地址

    return munmap((void *) ((int) p->VPURegMap & (~pageAlignment)), pageSize);
}
#endif

RK_S32 vpu_try_accept_task(VPU_INFO_S *p, VPU_TASK_S *pTask)
{
    RK_S32 ret = 0;

    if (p->resev)
    {
        if (pTask == p->resev)
            return VPU_SUCCESS;
        else
            return VPU_FAILURE;
    }

    if ((CODEC_USED(p) && USE_CODEC(pTask)) ||
        (PPROC_USED(p) && USE_PPROC(pTask)))
    {
        ALOGI("VPU: vpu_try_accept_task 0x%.8x failed\n", pTask);
        return VPU_FAILURE;
    }
    else
    {
        if (USE_CODEC(pTask))
        {
            p->codec = pTask;
            ALOGI("VPU: vpu_try_accept_task codec 0x%.8x success\n", pTask);
        }

        if (USE_PPROC(pTask))
        {
            p->pproc = pTask;
            ALOGI("VPU: vpu_try_accept_task codec 0x%.8x success\n", pTask);
        }

        return VPU_SUCCESS;
    }
}

RK_S32 vpu_release_task(VPU_INFO_S *p, VPU_TASK_S *pTask)
{
    if (USE_CODEC(pTask)) {
        if (ioctl(p->fd_vpu, VPU_IOC_CLS_IRQ, pTask->client_type)) {
            ALOGE("VPU: VPU_IOC_CLS_IRQ failed\n");
        }
        p->codec = NULL;
        p->resev = NULL;
        ALOGI("VPU: vpu_release_task codec\n");
    }

    if (USE_PPROC(pTask)) {
        if (ioctl(p->fd_vpu, VPU_IOC_CLS_IRQ, pTask->client_type)) {
            ALOGE("VPU: VPU_IOC_CLS_IRQ failed\n");
        }
        p->pproc = NULL;
        ALOGI("VPU: vpu_release_task pproc\n");
    }

    return VPU_SUCCESS;
}

RK_S32 vpu_connect(VPU_INFO_S *p, int sock)
{
    VPU_REGISTER_S *msg  = (VPU_REGISTER_S *)p->msg;
    VPU_TASK_S *pTask = NULL;
    VPU_CMD_TYPE cmd;
    RK_S32 len, i;

    ALOGI("VPU: accept client %d\n", sock);

    if (p->fds_used >= VPU_FD_TASK_NUM) {
        ALOGE("VPU: reach max connection number, refuse socket: %d\n", sock);
        goto SOCK_REFUSE;
    }

    {
        struct pollfd fd;
        fd.fd = sock;
        fd.events = POLLIN;

        if (poll(&fd, 1, -1) <= 0) {
            ALOGE("VPU: poll client socket %d fail\n", sock);
            goto SOCK_REFUSE;
        }

        if (!(fd.revents & POLLIN)) {
            ALOGE("VPU: poll client socket %d unexpected return\n", sock);
            goto SOCK_REFUSE;
        }
    }

    len = VPURecvMsg(sock, &cmd, (void *)p->msg, sizeof(p->msg), 0);
    if (len != sizeof(VPU_REGISTER_S)) {
        ALOGE("VPU: client socket %d recv msg failed len %d\n", sock, len);
        goto SOCK_REFUSE;
    }

    if (VPU_CMD_REGISTER != cmd) {
        ALOGE("VPU: client socket %d register cmd err: %d\n", sock, cmd);
        goto SOCK_REFUSE;
    }

    if (msg->client_type >= VPU_TYPE_BUTT) {
        ALOGE("VPU: client socket %d err client type: %d\n", sock, msg->client_type);
        goto SOCK_REFUSE;
    }

    pTask = malloc(sizeof(VPU_TASK_S));
    if (NULL == pTask) {
        ALOGE("VPU: accept client socket %d malloc fail\n", sock);
        goto SOCK_REFUSE;
    }

    if (VPUSendMsg(sock, VPU_CMD_REGISTER_ACK_OK, NULL, 0, 0))
    {
        ALOGE("VPU: send VPU_CMD_REGISTER_ACK_OK faild\n");
        goto SOCK_REFUSE;
    }

    if (vpu_power_on(p))
    {
        ALOGE("VPU: vpu_power_on faild\n");
        goto SOCK_REFUSE;
    }

    // 请求 LINK 成功
    for (i = VPU_TASK_ID_MIN; i < VPU_TASK_ID_MAX; i++)
    {
        if (-1 == fds[i].fd)
        {
            ALOGI("VPU: find fd %d for store sock %d\n", i, sock);
            break;
        }
    }

    pTask->fds_id       = i;
    pTask->sock         = sock;
    pTask->pid          = msg->pid;
    pTask->client_type  = msg->client_type;
    //pTask->reg_base     = vpu_reg[msg->client_type].reg_base_in_u32;
    //pTask->reg_size     = vpu_reg[msg->client_type].reg_size_in_u32;
    //pTask->reg_irqs     = vpu_reg[msg->client_type].irq_status_pos;
    fds[i].fd = sock;

    list_add_tail(&pTask->list, &p->fds_list.list);

    p->fds_used++;
    ALOGI("VPU: accept client %d id %d success total %d task\n", sock, pTask->fds_id, p->fds_used);
    return 0;

SOCK_REFUSE:
    if (pTask) {
        free(pTask);
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return VPU_FAILURE;
}

RK_S32 vpu_disconnect(VPU_INFO_S *p, VPU_TASK_S *pTask)
{
    ALOGI("VPU: vpu_disconnect \n");
    vpu_release_task(p, pTask);
    shutdown(pTask->sock, SHUT_RDWR);
    close(pTask->sock);
    fds[pTask->fds_id].fd = -1;
    list_del_init(&pTask->list);
    memset(pTask, 0, sizeof(VPU_TASK_S));
    free(pTask);
    pTask = NULL;
    p->fds_used--;

    vpu_power_off(p);

    return 0;
}

RK_S32 vpu_isr(VPU_INFO_S *p, RK_U32 event)
{
    RK_S32 ret = 0;
    RK_U32 irq_stats;

    assert(p->codec || p->pproc);

    if ((NULL == p) || ((NULL == p->codec) && (NULL == p->pproc))) {
        ALOGD("VPU: no task wait for interrupt!!\n");
        return -1;
    }

    if (event & VPU_IRQ_EVENT_ENC_BIT) {
        if (NULL == p->codec) {
            ALOGD("VPU: no enc task wait for enc irq\n");
            return -2;
        } else {
            VPU_TASK_S *pTask   = p->codec;
            if (ioctl(p->fd_vpu, VPU_IOC_RD_ENC, (RK_U32)p->VPURegEnc)) {
                ALOGE("VPU: VPU_IOC_WR_ENC failed\n");
                return -3;
            }

            if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, p->VPURegEnc, SIZE_REG(VPU_REG_NUM_ENC), 0)) {
                ALOGE("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK faild\n");
            } else {
                ALOGI("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK success\n");
            }
            vpu_release_task(p, pTask);
        }
    }

    if (event & VPU_IRQ_EVENT_DEC_BIT) {
        if (!((event & VPU_IRQ_EVENT_DEC_IRQ_BIT) || (event & VPU_IRQ_EVENT_PP_IRQ_BIT))) {
            ALOGE("VPU_IRQ_EVENT_DEC but no IRQ_BIT!");
            return -4;
        }

        if (event & VPU_IRQ_EVENT_DEC_IRQ_BIT) {
            if (NULL == p->codec) {
                ALOGD("VPU: no dec task wait for dec irq\n");
                return -5;
            } else {
                VPU_TASK_S *pTask   = p->codec;
                RK_U32  irq_status;
                RK_U32  reg_size;
                int     io_type;
                assert(VPU_ENC != pTask->client_type);

                if (VPU_DEC == pTask->client_type) {
                    io_type  = VPU_IOC_RD_DEC;
                    reg_size = SIZE_REG(VPU_REG_NUM_DEC);
                } else if (VPU_DEC_PP == pTask->client_type) {
                    io_type  = VPU_IOC_RD_DEC_PP;
                    reg_size = SIZE_REG(VPU_REG_NUM_DEC_PP);
                } else {
                    ALOGE("VPU: errer task type for dec irq\n");
                    return -6;
                }

                if (ioctl(p->fd_vpu, io_type, (RK_U32)p->VPURegDec)) {
                    ALOGE("VPU: VPU_IOC_RD_DEC failed\n");
                    return -7;
                }

                if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, p->VPURegDec, reg_size, 0)) {
                    ALOGE("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK faild\n");
                } else {
                    ALOGI("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK success\n");
                }

                irq_status = p->VPURegDec[VPU_REG_IRQS_DEC];
                if (!(irq_status&IRQ_DEC_ERROR) && (irq_status&IRQ_DEC_BUFFER_EMPTY)) {
                    p->resev = pTask;
                    ALOGI("VPU: dec task reserve success 0x%.8x\n", pTask);
                } else {
                    vpu_release_task(p, pTask);
                }
            }
        }

        if (event & VPU_IRQ_EVENT_PP_IRQ_BIT)
        {
            if (NULL == p->pproc) {
                return -8;
            } else {
                VPU_TASK_S *pTask   = p->pproc;
                assert(p->codec != p->pproc);

                if (ioctl(p->fd_vpu, VPU_IOC_RD_PP, (RK_U32)p->VPURegPp)) {
                    ALOGE("VPU: VPU_IOC_RD_DEC failed\n");
                    return -9;
                }

                if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, p->VPURegPp, SIZE_REG(VPU_REG_NUM_PP), 0)) {
                    ALOGE("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK faild\n");
                } else {
                    ALOGI("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK success\n");
                }

                vpu_release_task(p, pTask);
            }
        }
    }

#if 0
    if (event & VPU_IRQ_EVENT_DEC_BIT)
    {
        assert((event & VPU_IRQ_EVENT_DEC_IRQ_BIT) || (event & VPU_IRQ_EVENT_PP_IRQ_BIT));

        // codec + pp or codec only

        if (event & VPU_IRQ_EVENT_DEC_IRQ_BIT)
        {
            if (NULL == p->codec) {
                return -2;
            } else {

                VPU_TASK_S *pTask   = p->codec;
                RK_U32 *reg_base    = &p->VPURegMap[pTask->reg_base];
                RK_U32  reg_size    = pTask->reg_size * sizeof(RK_U32);
                RK_U32  irq_status  = p->VPURegMap[VPU_REG_IRQS_DEC];

                ALOGD("VPU: irq_status 0x%.8x\n", p->VPURegMap[pTask->reg_irqs]);

                assert(VPU_ENC != pTask->client_type);

                if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, reg_base, reg_size, 0))
                {
                    ALOGE("VPU: send dec VPU_SEND_CONFIG_ACK_DONE faild\n");
                }
                else
                {
                    ALOGI("VPU: send dec VPU_SEND_CONFIG_ACK_DONE success\n");
                }

                if (!(irq_status&IRQ_DEC_ERROR) && (irq_status&IRQ_DEC_BUFFER_EMPTY))
                {
                    p->resev = pTask;
                    ALOGI("VPU: dec task reserve success 0x%.8x\n", pTask);
                }
                else
                {
                    vpu_release_task(p, pTask);
                }
            }
        }

        // pproc only
        if (event & VPU_IRQ_EVENT_PP_IRQ_BIT)
        {
            if (NULL == p->pproc) {
                return -3;
            } else {
                VPU_TASK_S *pTask = p->pproc;
                RK_U32 *reg_base = &p->VPURegMap[pTask->reg_base];
                RK_U32  reg_size = pTask->reg_size * sizeof(RK_U32);

                assert(p->codec);
                assert(p->codec != p->pproc);

                if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, reg_base, reg_size, 0))
                {
                    ALOGE("VPU: send pp VPU_SEND_CONFIG_ACK_DONE faild\n");
                }
                else
                {
                    ALOGI("VPU: send pp VPU_SEND_CONFIG_ACK_DONE success\n");
                }

                vpu_release_task(p, pTask);
            }
        }
    }

    if (event & VPU_IRQ_EVENT_ENC_BIT)
    {
        if (NULL == p->codec) {
            return -4;
        } else {
            VPU_TASK_S *pTask = p->codec;
            RK_U32 *reg_base = &p->VPURegMap[pTask->reg_base];
            RK_U32  reg_size = pTask->reg_size * sizeof(RK_U32);
            assert(pTask);
            assert(event & VPU_IRQ_EVENT_ENC_IRQ_BIT);
            assert(VPU_ENC == pTask->client_type);

            // reset enc HW

            if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_OK, reg_base, reg_size, 0))
            {
                ALOGE("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK faild\n");
            }
            else
            {
                ALOGI("VPU: send VPU_ENC_SEND_CONFIG_ACK_OK success\n");
            }

            vpu_release_task(p, pTask);
        }
    }
#endif
    ALOGI("VPU: msg isr done\n");

    return ret;
}

RK_S32 VPUSendMsg(int sock, VPU_CMD_TYPE cmd, void *msg, RK_U32 len, int flags)
{
    VPU_MSG_HEADER_S h;
    h.magic = VPU_MAGIC;
    h.cmd = (RK_U8)cmd;
    h.len = (RK_U16)len;

    ALOGI("VPUSendMsg : sock : %8d msg : 0x%.8x cmd : %4d len : %4d\n", sock, (RK_U32)msg, cmd, len);

    if ((send(sock, &h, sizeof(VPU_MSG_HEADER_S), flags) == sizeof(VPU_MSG_HEADER_S)) &&
            ((len == 0) || (msg == NULL) || (send(sock, msg, len, flags) == (RK_S32)len)))
        return VPU_SUCCESS;
    else
        return VPU_FAILURE;
}

RK_S32 VPURecvMsg(int sock, VPU_CMD_TYPE *cmd, void *msg, RK_U32 len, int flags)
{
    VPU_MSG_HEADER_S h;

    ALOGI("VPURecvMsg : sock : %8d msg : 0x%.8x\n", sock, (RK_U32)msg);

    if ((recv(sock, &h, sizeof(VPU_MSG_HEADER_S), flags) == sizeof(VPU_MSG_HEADER_S)) &&
            (h.magic == VPU_MAGIC) &&
            (VPU_CMD_BUTT > (*cmd = (VPU_CMD_TYPE)h.cmd)) &&
            ((h.len == 0) || (msg == NULL) || (len == 0) ||
             (recv(sock, msg, len, flags) == h.len)))
        return h.len;
    else
        return VPU_FAILURE;
}

RK_S32 VPUProcMsg(VPU_INFO_S *p, VPU_TASK_S *pTask, VPU_CMD_TYPE cmd, RK_S32 len)
{
    ALOGI("VPU: VPUProcMsg %d len %d\n", cmd, len);

    switch (cmd)
    {

    case VPU_CMD_UNREGISTER :
        assert(len == 0);
        vpu_disconnect(p, pTask);

        break;

    case VPU_GET_HW_INFO :
        ALOGI("VPU: VPUProcMsg VPU_GET_HW_INFO\n");

        if (VPU_ENC == pTask->client_type)
        {
            if (VPUSendMsg(pTask->sock, VPU_GET_HW_INFO_ACK_OK, &p->EncHwCfg, sizeof(VPUHwEncConfig_t), 0))
            {
                ALOGE("VPU: send VPU_DEC_GET_HW_INFO_ACK faild\n");
            }
        }
        else
        {
            if (VPUSendMsg(pTask->sock, VPU_GET_HW_INFO_ACK_OK, &p->DecHwCfg, sizeof(VPUHwDecConfig_t), 0))
            {
                ALOGE("VPU: send VPU_DEC_GET_HW_INFO_ACK faild\n");
            }
        }

        vpu_release_task(p, pTask);
        break;

    case VPU_SEND_CONFIG :
        ALOGI("VPU: VPUProcMsg VPU_SEND_CONFIG\n");

        //if ((RK_U32)len == (pTask->reg_size * sizeof(RK_U32)))
        {
            //RK_S32  i;
            RK_U32  pMsg = (RK_U32)&p->msg[0];
            //RK_U32 *preg = p->VPURegMap + pTask->reg_base;

            if (vpu_power_on(p))
            {
                vpu_release_task(p, pTask);
                break;
            }

            switch (pTask->client_type) {
                #define VPU_REG_EN_ENC              14
                #define VPU_REG_ENC_GATE            2
                #define VPU_REG_ENC_GATE_BIT        (1<<4)

                #define VPU_REG_EN_DEC              1
                #define VPU_REG_DEC_GATE            2
                #define VPU_REG_DEC_GATE_BIT        (1<<10)
                #define VPU_REG_EN_PP               0
                #define VPU_REG_PP_GATE             1
                #define VPU_REG_PP_GATE_BIT         (1<<8)
                #define VPU_REG_EN_DEC_PP           1
                #define VPU_REG_DEC_PP_GATE         61
                #define VPU_REG_DEC_PP_GATE_BIT     (1<<8)
            case VPU_ENC : {
                ALOGI("VPU: VPUProcMsg VPU_ENC_SEND_CONFIG\n");
/*
                preg[VPU_REG_EN_ENC] = pMsg[VPU_REG_EN_ENC] & 0x6;

                for (i = 0; i < VPU_REG_EN_ENC; i++)
                    preg[i] = pMsg[i];

                for (i = VPU_REG_EN_ENC + 1; i < VPU_REG_NUM_ENC; i++)
                    preg[i] = pMsg[i];

                preg[VPU_REG_ENC_GATE] = pMsg[VPU_REG_ENC_GATE] | VPU_REG_ENC_GATE_BIT;
                preg[VPU_REG_EN_ENC] = pMsg[VPU_REG_EN_ENC];
*/
                if (ioctl(p->fd_vpu, VPU_IOC_WR_ENC, pMsg))
                {
                    ALOGE("VPU: VPU_IOC_WR_ENC failed\n");
                    return VPU_FAILURE;
                }
                break;
            }
            case VPU_DEC : {
                ALOGI("VPU: VPUProcMsg VPU_DEC_SEND_CONFIG\n");
/*
                for (i = pTask->reg_size - 1; i > VPU_REG_DEC_GATE; i--)
                    preg[i] = pMsg[i];

                preg[VPU_REG_DEC_GATE] = pMsg[VPU_REG_DEC_GATE] | VPU_REG_DEC_GATE_BIT;
                preg[VPU_REG_EN_DEC]   = pMsg[VPU_REG_EN_DEC];
*/
                if (ioctl(p->fd_vpu, VPU_IOC_WR_DEC, pMsg))
                {
                    ALOGE("VPU: VPU_IOC_WR_DEC failed\n");
                    return VPU_FAILURE;
                }
                break;
            }
            case VPU_PP : {
                ALOGI("VPU: VPUProcMsg VPU_PP_SEND_CONFIG\n");
/*
                //    preg[i] = pMsg[i];
                preg[VPU_REG_PP_GATE] = pMsg[VPU_REG_PP_GATE] | VPU_REG_PP_GATE_BIT;
                for (i = VPU_REG_PP_GATE + 1; i < pTask->reg_size; i++)
                    preg[i] = pMsg[i];
                preg[VPU_REG_EN_PP] = pMsg[VPU_REG_EN_PP];
*/
                if (ioctl(p->fd_vpu, VPU_IOC_WR_PP, pMsg))
                {
                    ALOGE("VPU: VPU_IOC_WR_PP failed\n");
                    return VPU_FAILURE;
                }
                break;
            }
            case VPU_DEC_PP : {
                ALOGI("VPU: VPUProcMsg VPU_DEC_PP_SEND_CONFIG\n");
/*
                for (i = VPU_REG_EN_DEC_PP + 1; i < pTask->reg_size; i++)
                    preg[i] = pMsg[i];
                preg[VPU_REG_DEC_PP_GATE] = pMsg[VPU_REG_DEC_PP_GATE] | VPU_REG_PP_GATE_BIT;
                preg[VPU_REG_DEC_GATE]    = pMsg[VPU_REG_DEC_GATE]    | VPU_REG_DEC_GATE_BIT;
                preg[VPU_REG_EN_DEC] = pMsg[VPU_REG_EN_DEC];
*/
                if (ioctl(p->fd_vpu, VPU_IOC_WR_DEC_PP, pMsg))
                {
                    ALOGE("VPU: VPU_IOC_WR_DEC_PP failed\n");
                    return VPU_FAILURE;
                }
                break;
            }
            default : {
                ALOGE("VPU: VPUProcMsg VPU_DEC_PP_SEND_CONFIG error\n");
                break;
            }
            }
        }
#if 0
        else
        {
            if (VPUSendMsg(pTask->sock, VPU_SEND_CONFIG_ACK_FAIL, NULL, 0, 0))
            {
                ALOGE("VPU: send VPU_SEND_CONFIG_ACK_FAIL faild\n");
            }
            vpu_release_task(p, pTask);
        }
#endif

        break;

    case VPU_CMD_REGISTER :
        ALOGE("VPU: one task can not register more than once!!\n");
        break;

    default :
        ALOGE("undefined command\n");
        break;
    }

    ALOGI("VPU: VPUProcMsg %d len %d done\n", cmd, len);

    return VPU_SUCCESS;
}

void *vpu_service(void *arg)
{
    VPU_INFO_S *p = NULL;
    // fd for VPU_MODULE_PATH
    int fd = -1;
    int err = VPU_SUCCESS;;
    if (access(VPU_SERVICE_PATH, F_OK) == 0)
    {
        ALOGI("vpu_service exsists, use kernel function\n");
        return NULL;
    }

    p = malloc(sizeof(VPU_INFO_S));

    if (NULL == p)
    {
        ALOGE("VPU: failed to malloc vpu info\n");
        VPU_ERROR(-1);
    }

    memset(p, 0, sizeof(VPU_INFO_S));

    INIT_LIST_HEAD(&p->fds_list.list);
    p->power_on = 0;

    /* open hardware device in VPU_MODULE_PATH */
    fd = open(VPU_MODULE_PATH, O_RDWR);

    if (fd == -1)
    {
        ALOGE("VPU: failed to open vpu\n");
        VPU_ERROR(-1);
    }

    ALOGI("VPU: vpu opened\n");

    /* mmap hardware device register to virtual address */
    vpu_mmap_and_get_info(p, fd);

    ALOGI("VPU: service started\n");

    {
        RK_S32 i;
        int sock = android_get_control_socket("vpu_server");

        if (sock < 0)
        {
            ALOGE("VPU: failed to create socket\n");
            VPU_ERROR(-2);
        }

        ALOGI("VPU: socket create done\n");

        // 第二个参数是最大同时监听的数量

        if (listen(sock, 10) == -1)
        {
            ALOGE("VPU: listen: %sock", strerror(errno));
            VPU_ERROR(-4);
        }

        ALOGI("VPU: server start recving\n");

        // 动态分配

        // 设备句柄：作用是收中断，收事件 POLLIN
        fds[VPU_FD_ISR].fd = fd;
        fds[VPU_FD_ISR].events = POLLIN;

        // 接收解码实例的连接
        fds[VPU_FD_CONNECT].fd = sock;
        fds[VPU_FD_CONNECT].events = POLLIN;

        // 初始化解码实例连接

        for (i = VPU_TASK_ID_MIN; i < VPU_TASK_ID_MAX; i++)
        {
            fds[i].fd = -1;
            fds[i].events = POLLIN;
        }

        ALOGI("VPU: doing message process\n");

        for (;;)
        {
            if (poll(fds, VPU_FD_MAX_NUM, -1) > 0)
            {
                ALOGI("VPU: poll return\n");

                /* check interrupt event */

                if (fds[VPU_FD_ISR].revents & POLLIN)
                {
                    RK_U32 event;
                    ALOGI("VPU: start to read interrupt event\n");

                    if (read(fds[VPU_FD_ISR].fd, &event, sizeof(event)) == 4)
                    {
                        ALOGI("VPU: irq read 0x%x\n", event);
                        // 返回的是中断的数量
                        vpu_isr(p, event);
                    }
                    else
                    {
                        ALOGE("VPU: read failed\n");
                    }
                }

                /* check connection event */
                if (fds[VPU_FD_CONNECT].revents & POLLIN)
                {
                    int client_sock = accept(sock, NULL, 0);
                    if (client_sock >= 0) {
                        vpu_connect(p, client_sock);
                    } else {
                        ALOGE("VPU: accept client %d failed\n", client_sock);
                    }
                }

                {

                    struct list_head *list, *tmp;
                    VPU_CMD_TYPE cmd;
                    list_for_each_safe(list, tmp, &p->fds_list.list)
                    {
                        VPU_TASK_S *pTask = list_entry(list, VPU_TASK_S, list);
                        ALOGI("VPU: task: type %d id %d socket %d events %x\n", pTask->client_type, pTask->fds_id, pTask->sock, fds[pTask->fds_id].revents);
                        // 先判断能否接受新的任务，不能接受的任务，就完全不进行监听
                        ALOGI("fds_list.list: 0x%.8x list: 0x%.8x\n", &p->fds_list.list, list);

                        if ((fds[pTask->fds_id].revents) &&
                                (VPU_SUCCESS == vpu_try_accept_task(p, pTask)))
                        {
                            RK_S32 release_task = 0;
                            ALOGI("VPU: vpu_try_accept_task: type %d id %d success\n", pTask->client_type, pTask->fds_id);

                            if (fds[pTask->fds_id].revents == POLLIN)
                            {
                                int sock = pTask->sock;
                                RK_S32 len = 0;
                                VPU_CMD_TYPE cmd;
                                ALOGI("VPU: reading sock %d\n", sock);
                                len = VPURecvMsg(sock, &cmd, (void *)p->msg, sizeof(p->msg), 0);

                                if (len >= 0)
                                {
                                    if (VPU_SUCCESS == VPUProcMsg(p, pTask, cmd, len))
                                        ALOGI("VPU: task proc msg success\n");
                                    else
                                    {
                                        release_task = 1;
                                        ALOGE("VPU: task proc msg failed, len = %d\n", len);
                                    }
                                }
                                else
                                {
                                    release_task = 1;
                                    ALOGE("VPU: VPURecvMsg failed\n");
                                }
                            }
                            else
                            {
                                //release_task = 0;
                                ALOGD("VPU: task: type %d id %d poll return not only pollin\n", pTask->client_type, pTask->fds_id);
                                // 防止通道关闭

                                if (fds[pTask->fds_id].fd != -1)
                                {
                                    //VPU_DEBUG("VPU: poll for socket error\n");
                                    if (fds[pTask->fds_id].revents & (POLLERR | POLLHUP | POLLNVAL))
                                    {
                                        // 错误处理
                                        ALOGD("VPU: poll socket revents 0x%x\n", fds[pTask->fds_id].revents);
                                    }
                                }
                                else
                                {
                                    ALOGD("VPU: task %.8x id %d socket is released\n", (RK_U32)pTask, pTask->fds_id);
                                }

                                vpu_disconnect(p, pTask);
                            }

                            if (release_task)
                                vpu_release_task(p, pTask);
                        }
                        else
                        {
                            ALOGI("VPU: skip task: type %d id %d events %d\n", pTask->client_type, pTask->fds_id, fds[pTask->fds_id].revents);
                        }
                    }

                    ALOGI("VPU: list_for_each_safe done\n");
                }
            }
            else
            {
                ALOGE("poll failed %d\n", errno);
            }
        }

        ALOGE("msg process done\n");

        shutdown(sock, SHUT_RDWR);
        close(sock);
        ALOGE("Server done\n");
    }

END:
    if (-1 != fd) {
        close(fd);
        fd = -1;
        ALOGD("VPU: vpu closed\n");
    }

    if (p) {
        free(p);
        p = NULL;
    }

    return NULL ;
}

//#define VPU_DRV_TEST
#ifdef VPU_DRV_TEST

#include <pthread.h>
#include <errno.h>
#include <cutils/sockets.h>

int main(int argc, char **argv)
{
    int sock = -1;
    VPU_CMD_TYPE cmd;
    VPU_REGISTER_S chn;
    RK_U32 h264Regs[VPU_REG_NUM_DEC];

    {
        pthread_t tid;
        printf("vpu_service starting\n");
        pthread_create(&tid, NULL, (void *)vpu_service, NULL);
    }

    do
    {
        sock = socket_local_client("vpu_server", ANDROID_SOCKET_NAMESPACE_RESERVED,
                                   SOCK_STREAM);
    }
    while (-1 == sock);

    VPU_DEBUG("VPU: VPU client %d link success\n", sock);

    chn.client_type = VPU_DEC;

    chn.pid = gettid();

    // 注册应用实例
    if ((VPU_SUCCESS == VPUSendMsg(sock, VPU_CMD_REGISTER, &chn, sizeof(VPU_REGISTER_S), 0)) &&
            (VPU_SUCCESS == VPURecvMsg(sock, &cmd, NULL, 0, 0)) &&
            (VPU_CMD_REGISTER_ACK_OK == cmd))
        VPU_DEBUG("VPUClientInit: VPUSendMsg VPU_CMD_REGISTER success\n");
    else
        VPU_DEBUG("VPUClientInit: VPUSendMsg VPU_CMD_REGISTER fail\n");

    // 写入码流
    //

    // 发送寄存器配置数据
    if (VPUSendMsg(sock, VPU_SEND_CONFIG, &h264Regs, sizeof(VPU_DEC_REG_S), 0))
        VPU_DEBUG("vpu_test: VPUSendMsg VPU_SEND_CONFIG fail\n");
    else
        VPU_DEBUG("vpu_test: VPUSendMsg VPU_SEND_CONFIG success\n");

    if (VPU_SUCCESS == VPURecvMsg(sock, &cmd, NULL, 0, 0))
        VPU_DEBUG("vpu_test: VPURecvMsg success\n");
    else
        VPU_DEBUG("vpu_test: VPURecvMsg fail\n");

    if (cmd == VPU_SEND_CONFIG_ACK_OK)
        VPU_DEBUG("vpu_test: VPURecvMsg cmd VPU_SEND_CONFIG_ACK_OK done\n");
    else
        VPU_DEBUG("vpu_test: VPURecvMsg cmd VPU_SEND_CONFIG_ACK_OK false\n");

    if (VPU_SUCCESS == VPUSendMsg(sock, VPU_CMD_UNREGISTER, NULL, 0, 0))
        VPU_DEBUG("vpu_test: VPUSendMsg cmd VPU_CMD_UNREGISTER false\n");
    else
        VPU_DEBUG("vpu_test: VPUSendMsg cmd VPU_CMD_UNREGISTER done\n");

    while (1)
    {
        usleep(10);
    }

    return 0;
}

#endif /* VPU_DRV_TEST */
#endif

