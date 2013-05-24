/*
 * Copyright (C) 2013 Rockchip Open Libvpu Project
 * Author: jian huan jh@rock-chips.com
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

#ifndef _VPU_GLOBAL_
#define _VPU_GLOBAL_

#include "vpu_macro.h"
#include "vpu_mem.h"

typedef struct
{
    RK_U32   TimeLow;
    RK_U32   TimeHigh;
} TIME_STAMP;

typedef struct
{
    RK_U32   CodecType;
    RK_U32   ImgWidth;          // not 16 aligned read from file
    RK_U32   ImgHeight;         // not 16 aligned read from file
} VPU_GENERIC;

typedef struct
{
    RK_U32      StartCode;
    RK_U32      SliceLength;
    TIME_STAMP  SliceTime;
    RK_U32      SliceType;
    RK_U32      SliceNum;
    RK_U32      Res[2];
} VPU_BITSTREAM; /*completely same as RK28*/

typedef struct
{
    RK_U32      InputAddr[2];
    RK_U32      OutputAddr[2];
    RK_U32      InputWidth;
    RK_U32      InputHeight;
    RK_U32      OutputWidth;
    RK_U32      OutputHeight;
    RK_U32      ColorType;
    RK_U32      ScaleEn;
    RK_U32      RotateEn;
    RK_U32      DitherEn;
    RK_U32      DeblkEn;
    RK_U32      DeinterlaceEn;
    RK_U32      Res[5];
} VPU_POSTPROCESSING;

typedef struct tVPU_FRAME
{
    RK_U32          FrameBusAddr[2];    // 0: Y address; 1: UV address;
    RK_U32          FrameWidth;         // 16 aligned horizontal stride
    RK_U32          FrameHeight;        // 16 aligned vertical   stride
    RK_U32          OutputWidth;        // non 16 aligned width
    RK_U32          OutputHeight;       // non 16 aligned height
    RK_U32          DisplayWidth;       // non 16 aligned width
    RK_U32          DisplayHeight;      // non 16 aligned height
    RK_U32          CodingType;
    RK_U32          FrameType;          // frame;top_field_first;bot_field_first
    RK_U32          ColorType;
    RK_U32          DecodeFrmNum;
    TIME_STAMP      ShowTime;
    RK_U32          ErrorInfo;          // error information of the frame
    RK_U32	        employ_cnt;
    VPUMemLinear_t  vpumem;
    struct tVPU_FRAME *    next_frame;
    RK_U32          Res[4];
} VPU_FRAME;

#endif /*_VPU_GLOBAL_*/
