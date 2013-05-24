#include "postprocess.h"
#include <stdlib.h>
#include <string.h>

postprocess::postprocess()
{
    memset(this, 0, sizeof(postprocess));
}

postprocess::~postprocess()
{
    memset(this, 0, sizeof(postprocess));
}

int postprocess::setup_pp(VPU_FRAME *src, VPU_FRAME *dst)
{
    if (!pp_en)
        return -1;
    if (pp_pipeline)
        pp_rotate = 0;
    if ((!src->FrameWidth) || (!src->FrameHeight))
        return -1;
    if (!display_width)
        display_width = dst->DisplayWidth;
    reg->SetRegisterFile(HWIF_PP_AXI_RD_ID, 0);
    reg->SetRegisterFile(HWIF_PP_AXI_WR_ID, 0);
    reg->SetRegisterFile(HWIF_PP_AHB_HLOCK_E, 1);
    reg->SetRegisterFile(HWIF_PP_SCMD_DIS, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A2_ENDSEL, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A1_SWAP32, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A1_ENDIAN, 1);
    reg->SetRegisterFile(HWIF_PP_IN_SWAP32_E, 1);
    reg->SetRegisterFile(HWIF_PP_DATA_DISC_E, 1);
    reg->SetRegisterFile(HWIF_PP_CLK_GATE_E, 0);
    reg->SetRegisterFile(HWIF_PP_IN_ENDIAN, 1);
    reg->SetRegisterFile(HWIF_PP_OUT_ENDIAN, 1);
    reg->SetRegisterFile(HWIF_PP_OUT_SWAP32_E, 1);
    reg->SetRegisterFile(HWIF_PP_MAX_BURST, 16);

    reg->SetRegisterFile(HWIF_PP_IN_LU_BASE, src->FrameBusAddr[0]);
    reg->SetRegisterFile(HWIF_PP_IN_WIDTH, src->FrameWidth>>4);
    reg->SetRegisterFile(HWIF_PP_IN_HEIGHT, src->FrameHeight>>4);
    reg->SetRegisterFile(HWIF_EXT_ORIG_WIDTH, src->FrameWidth>>4);

    reg->SetRegisterFile(HWIF_DISPLAY_WIDTH, display_width);
    reg->SetRegisterFile(HWIF_PP_OUT_WIDTH, dst->DisplayWidth);
    reg->SetRegisterFile(HWIF_PP_OUT_HEIGHT, dst->DisplayHeight);
    reg->SetRegisterFile(HWIF_PP_OUT_LU_BASE, dst->FrameBusAddr[0]);

    reg->SetRegisterFile(HWIF_RANGEMAP_COEF_Y, 9);
    reg->SetRegisterFile(HWIF_RANGEMAP_COEF_C, 9);

    /*  brightness */
    reg->SetRegisterFile(HWIF_COLOR_COEFFF, pp_light);

    switch (src->ColorType)
    {
    case    PP_IN_FORMAT_YUV422INTERLAVE:
        return -1;
        break;
    case    PP_IN_FORMAT_YUV420SEMI:
        reg->SetRegisterFile(HWIF_PP_IN_FORMAT, 1);
        break;
    case    PP_IN_FORMAT_YUV420PLANAR:

        break;
    case    PP_IN_FORMAT_YUV400:
        break;
    case    PP_IN_FORMAT_YUV422SEMI:
        break;
    case    PP_IN_FORMAT_YUV420SEMITIELED:
        break;
    case    PP_IN_FORMAT_YUV440SEMI:
        break;
    case    PP_IN_FORMAT_YUV444:
        break;
    case    PP_IN_FORMAT_YUV411:
        break;
    default:
        return -1;
    }

    if (dst->ColorType <= PP_OUT_FORMAT_ARGB)
    {
        RK_U32  a = 298;
        RK_U32  b = 409;
        RK_U32  c = 208;
        RK_U32  d = 100;
        RK_U32  e = 516;
        RK_S32  satur = 0, tmp;

        reg->SetRegisterFile(HWIF_CONTRAST_THR1, 55);
        reg->SetRegisterFile(HWIF_CONTRAST_THR2, 165);

        reg->SetRegisterFile(HWIF_CONTRAST_OFF1, 0);
        reg->SetRegisterFile(HWIF_CONTRAST_OFF2, 0);
        reg->SetRegisterFile(HWIF_PP_OUT_ENDIAN, 0);
        tmp = a;
        if (tmp > 1023)
            tmp = 1023;
        else if (tmp < 0)
            tmp = 0;

        reg->SetRegisterFile(HWIF_COLOR_COEFFA1, tmp);
        reg->SetRegisterFile(HWIF_COLOR_COEFFA2, tmp);

        /* saturation */
        satur = 64 + pp_sat;

        tmp = (satur * (RK_S32) b) / 64;
        if (tmp > 1023)
            tmp = 1023;
        else if (tmp < 0)
            tmp = 0;
        reg->SetRegisterFile(HWIF_COLOR_COEFFB, (RK_U32) tmp);

        tmp = (satur * (RK_S32) c) / 64;
        if (tmp > 1023)
            tmp = 1023;
        else if (tmp < 0)
            tmp = 0;
        reg->SetRegisterFile(HWIF_COLOR_COEFFC, (RK_U32) tmp);

        tmp = (satur * (RK_S32) d) / 64;
        if (tmp > 1023)
            tmp = 1023;
        else if (tmp < 0)
            tmp = 0;
        reg->SetRegisterFile(HWIF_COLOR_COEFFD, (RK_U32) tmp);

        tmp = (satur * (RK_S32) e) / 64;
        if (tmp > 1023)
            tmp = 1023;
        else if (tmp < 0)
            tmp = 0;

        reg->SetRegisterFile(HWIF_COLOR_COEFFE, (RK_U32) tmp);
    }


    switch (dst->ColorType)
    {
    case    PP_OUT_FORMAT_RGB565:
        reg->SetRegisterFile(HWIF_R_MASK, 0xF800F800);
        reg->SetRegisterFile(HWIF_G_MASK, 0x07E007E0);
        reg->SetRegisterFile(HWIF_B_MASK, 0x001F001F);
        reg->SetRegisterFile(HWIF_RGB_R_PADD, 0);
        reg->SetRegisterFile(HWIF_RGB_G_PADD, 5);
        reg->SetRegisterFile(HWIF_RGB_B_PADD, 11);

        reg->SetRegisterFile(HWIF_DITHER_SELECT_R, 2);
        reg->SetRegisterFile(HWIF_DITHER_SELECT_G, 3);
        reg->SetRegisterFile(HWIF_DITHER_SELECT_B, 2);
        reg->SetRegisterFile(HWIF_RGB_PIX_IN32, 1);
        reg->SetRegisterFile(HWIF_PP_OUT_SWAP16_E,1);
        reg->SetRegisterFile(HWIF_PP_OUT_FORMAT, 0);
        break;
    case    PP_OUT_FORMAT_ARGB:
        reg->SetRegisterFile( HWIF_R_MASK, 0x00FF0000 | (0xff << 24));
        reg->SetRegisterFile( HWIF_G_MASK, 0x0000FF00 | (0xff << 24));
        reg->SetRegisterFile( HWIF_B_MASK, 0x000000FF | (0xff << 24));
        reg->SetRegisterFile( HWIF_RGB_R_PADD, 8);
        reg->SetRegisterFile( HWIF_RGB_G_PADD, 16);
        reg->SetRegisterFile( HWIF_RGB_B_PADD, 24);

        reg->SetRegisterFile(HWIF_RGB_PIX_IN32, 0);
        reg->SetRegisterFile(HWIF_PP_OUT_FORMAT, 0);
        break;
    case    PP_OUT_FORMAT_YUV422INTERLAVE:
        reg->SetRegisterFile(HWIF_PP_OUT_FORMAT, 3);
        break;
    case    PP_OUT_FORMAT_YUV420INTERLAVE:
        reg->SetRegisterFile(HWIF_PP_OUT_CH_BASE, dst->FrameBusAddr[0]+dst->DisplayWidth*dst->DisplayHeight);
        reg->SetRegisterFile(HWIF_PP_OUT_FORMAT, 5);
        break;
    default:
        return -1;
    }
    /*
            SetRegisterFile(HWIF_PP_IN_CB_BASE, srcframe->mem.addr+srcframe->width*srcframe->height);
            SetRegisterFile(HWIF_PP_IN_CR_BASE, srcframe->mem.addr+srcframe->width*srcframe->height);

            SetRegisterFile(HWIF_PP_OUT_CH_BASE, dstframe->mem.addr+dstframe->width*dstframe->height);
    */





    reg->SetRegisterFile(HWIF_ROTATION_MODE, pp_rotate);
    if (pp_deint)
    {
    }

    if (pp_scale)
    {
        RK_U32      inw,inh;
        RK_U32      outw,outh;

        inw = src->OutputWidth -1;
        inh = src->OutputHeight -1;
        outw = dst->DisplayWidth -1;
        outh = dst->DisplayHeight -1;
        if ((pp_rotate == 1)||(pp_rotate == 2))
        {
            inw = src->OutputHeight -1;
            inh = src->OutputWidth -1;
        }

        if (inw < outw)
        {
            reg->SetRegisterFile(HWIF_HOR_SCALE_MODE, 1);
            reg->SetRegisterFile(HWIF_SCALE_WRATIO, (outw<<16)/inw);
            reg->SetRegisterFile(HWIF_WSCALE_INVRA, (inw<<16)/outw);
        }
        else if (inw > outw)
        {
            reg->SetRegisterFile(HWIF_HOR_SCALE_MODE, 2);
            reg->SetRegisterFile(HWIF_WSCALE_INVRA, ((outw+1)<<16)/(inw+1));
        }
        else
            reg->SetRegisterFile(HWIF_HOR_SCALE_MODE, 0);

        if (inh < outh)
        {
            reg->SetRegisterFile(HWIF_VER_SCALE_MODE, 1);
            reg->SetRegisterFile(HWIF_SCALE_HRATIO, (outh<<16)/inh);
            reg->SetRegisterFile(HWIF_HSCALE_INVRA, (inh<<16)/outh);
        }
        else if (inh > outh)
        {
            reg->SetRegisterFile(HWIF_VER_SCALE_MODE, 2);
            reg->SetRegisterFile(HWIF_HSCALE_INVRA, ((outh+1)<<16)/(inh+1));
        }
        else
            reg->SetRegisterFile(HWIF_VER_SCALE_MODE, 0);
        reg->SetRegisterFile(HWIF_PP_FAST_SCALE_E, 1);
    }
    return 0;
}

int postprocess::setup_deint(VPU_FRAME *src, VPU_FRAME *dst)
{
    //if (!pp_en)
    //  return -1;
    if ((!src->FrameWidth) || (!src->FrameHeight))
        return -1;
    if (!display_width)
        display_width = dst->DisplayWidth;
    reg->SetRegisterFile(HWIF_PP_AXI_RD_ID, 0);
    reg->SetRegisterFile(HWIF_PP_AXI_WR_ID, 0);
    reg->SetRegisterFile(HWIF_PP_AHB_HLOCK_E, 1);
    reg->SetRegisterFile(HWIF_PP_SCMD_DIS, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A2_ENDSEL, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A1_SWAP32, 1);
    reg->SetRegisterFile(HWIF_PP_IN_A1_ENDIAN, 1);
    //reg->SetRegisterFile(HWIF_PP_IN_SWAP32_E, 1);
    reg->SetRegisterFile(HWIF_PP_DATA_DISC_E, 1);
    reg->SetRegisterFile(HWIF_PP_CLK_GATE_E, 0);
    reg->SetRegisterFile(HWIF_PP_IN_ENDIAN, 1);
    reg->SetRegisterFile(HWIF_PP_OUT_ENDIAN, 1);
    //reg->SetRegisterFile(HWIF_PP_OUT_SWAP32_E, 1);
    reg->SetRegisterFile(HWIF_PP_MAX_BURST, 16);

    reg->SetRegisterFile(HWIF_DEINT_E, 1);
    reg->SetRegisterFile(HWIF_DEINT_THRESHOLD, 256);
    reg->SetRegisterFile(HWIF_DEINT_BLEND_E, 0);
    reg->SetRegisterFile(HWIF_DEINT_EDGE_DET, 256);

    reg->SetRegisterFile(HWIF_PP_IN_LU_BASE, src->FrameBusAddr[0]);
    reg->SetRegisterFile(HWIF_PP_IN_CB_BASE, src->FrameBusAddr[1]);
    reg->SetRegisterFile(HWIF_PP_IN_CR_BASE, src->FrameBusAddr[1]+1);
    reg->SetRegisterFile(HWIF_PP_OUT_LU_BASE, dst->FrameBusAddr[0]);
    reg->SetRegisterFile(HWIF_PP_OUT_CH_BASE, dst->FrameBusAddr[1]);

    reg->SetRegisterFile(HWIF_PP_IN_WIDTH, src->FrameWidth>>4);
    reg->SetRegisterFile(HWIF_PP_IN_HEIGHT, src->FrameHeight>>4);

    reg->SetRegisterFile(HWIF_PP_BOT_YIN_BASE, src->FrameBusAddr[0]+src->FrameWidth);
    reg->SetRegisterFile(HWIF_PP_BOT_CIN_BASE, src->FrameBusAddr[1]+src->FrameWidth);

    reg->SetRegisterFile(HWIF_PP_IN_FORMAT, 1);
    reg->SetRegisterFile(HWIF_PP_OUT_FORMAT, 5);
    reg->SetRegisterFile(HWIF_PP_OUT_WIDTH, dst->DisplayWidth);
    reg->SetRegisterFile(HWIF_PP_OUT_HEIGHT, dst->DisplayHeight);

    reg->SetRegisterFile(HWIF_EXT_ORIG_WIDTH, src->FrameWidth>>4);

    reg->SetRegisterFile(HWIF_DISPLAY_WIDTH, display_width);
    reg->SetRegisterFile(HWIF_PP_E, 1);

    return 0;
}

void    postprocess::start_pp(void)
{
    if (pp_en)
    {
        if (pp_pipeline)
            reg->SetRegisterFile(HWIF_PP_PIPELINE_E, 1);
        else
            reg->SetRegisterFile(HWIF_PP_E, 1);
    }
}
