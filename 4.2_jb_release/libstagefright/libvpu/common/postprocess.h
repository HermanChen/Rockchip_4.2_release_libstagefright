#ifndef _POSTPROCESS_H_
#define _POSTPROCESS_H_

#include "vpu_global.h"
#include "reg.h"

#define PP_IN_FORMAT_YUV422INTERLAVE            0
#define PP_IN_FORMAT_YUV420SEMI                     1
#define PP_IN_FORMAT_YUV420PLANAR                  2
#define PP_IN_FORMAT_YUV400                             3
#define PP_IN_FORMAT_YUV422SEMI                     4
#define PP_IN_FORMAT_YUV420SEMITIELED           5
#define PP_IN_FORMAT_YUV440SEMI                     6
#define PP_IN_FORMAT_YUV444                                 7
#define PP_IN_FORMAT_YUV411                                 8

#define PP_OUT_FORMAT_RGB565                    0
#define PP_OUT_FORMAT_ARGB                       1
#define PP_OUT_FORMAT_YUV422INTERLAVE    3
#define PP_OUT_FORMAT_YUV420INTERLAVE    5

class postprocess
{
public:
    postprocess();
    ~postprocess();
    void    pp_enable(void)
    {
        pp_en = 1;
    }
    void pp_disable(void)
    {
        pp_en = 0;
    }
    void pp_scale_enable(void)
    {
        pp_scale = 1;
    }
    void pp_scale_disable(void)
    {
        pp_scale = 0;
    }
    void pp_pipeline_enable(void)
    {
        pp_pipeline = 1;
    }
    void pp_pipeline_disable(void)
    {
        pp_pipeline = 0;
    }

    void pp_deint_enable(void)
    {
        pp_deint = 1;
    }
    void pp_deint_disable(void)
    {
        pp_deint = 0;
    }
    void pp_set_rotate(RK_U32 type)
    {
        pp_rotate = type;
    }
    void pp_set_bright_adjust(RK_S8 val)
    {
        pp_light = val;
    }
    void pp_set_colorsat_adjust(RK_S8 val)
    {
        pp_sat = val;
    }
    void pp_set_displaywidth(RK_U16 width)
    {
        display_width = width;
    }
    void setup_reg(void *p)
    {
        reg = (on2register*)p;
    }
    int setup_pp(VPU_FRAME *src, VPU_FRAME *dst);
    int setup_deint(VPU_FRAME *src, VPU_FRAME *dst);
    void start_pp();
private:
    on2register     *reg;
    RK_U32      pp_en            : 1;
    RK_U32      pp_pipeline    : 1;
    RK_U32      pp_rotate        : 3;
    RK_U32      pp_deint            : 1;
    RK_U32      pp_scale        : 1;

    RK_U16      display_width;
    RK_S8           pp_light;
    RK_S8           pp_sat;
};

#endif
