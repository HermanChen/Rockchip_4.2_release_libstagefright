/***************************************************************************************************
    File:
        reg.cpp
    Decription:
        register operation
    Author:
        Jian Huan
    Date:
        2010-12-8 16:40:04
 **************************************************************************************************/
#include "reg.h"

on2register::on2register()
{
    regBase = 0;
}

on2register::~on2register()
{
    regBase = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
								/*HW Config as follows*/
const RK_U32 regMask[33] = {		0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000F,
    0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
    0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
    0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
    0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
    0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
    0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
    0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

/* { SWREG, BITS, POSITION } */
RK_U32 hwDecRegSpec[HWIF_LAST_REG + 1][3] = {
    /* include script-generated part */
#include "8170table.h"
    /* HWIF_DEC_IRQ_STAT */ {1,  7, 12},
    /* HWIF_PP_IRQ_STAT */  {60, 2, 12},
    /* dummy entry */       { 0, 0,  0}
};


/***************************************************************************************************
    Func:
        WriteReg
    Description:
        write hw register
    Notice:
        index是register的索引号，不是在register-mapped memory中的地址。
        例如:向register5中写入数据，index就等于5.
    Author:
        Jian Huan
    Date:
        2010-12-8 17:00:30
 **************************************************************************************************/
void on2register::WriteReg(RK_U32 index, RK_U32 val)
{
    wr(VDPU_BASE + index * 4, val);
}

/***************************************************************************************************
    Func:
        FlushRegs
    Description:
        write all register-mapped memory to HW registers except register0, register1.
    Author:
        Jian Huan
    Date:
        2010-12-8 17:00:30
 **************************************************************************************************/
void on2register::FlushRegs()
{
    int i;

    for (i = 2; i < DEC_X170_REGISTERS; i++)
    {
        wr(VDPU_BASE + i*4, regBase[i]);
    }
}

/***************************************************************************************************
    Func:
        DumpRegs
    Description:
        read all HW registers to register-mapped memory except register 0 , register 1
    Author:
        Jian Huan
    Date:
        2010-12-8 17:00:30
 **************************************************************************************************/
void on2register::DumpRegs()
{
#ifndef WIN32
    int i;

    for (i = 2; i < DEC_X170_REGISTERS; i++)
    {
        regBase[i] = rd(VDPU_BASE + i * 4);
    }
#endif
}
/***************************************************************************************************
    Func:
        SetRegisterFile
    Description:
        config register-mapped memory (not hw register)
    Author:
        Jian Huan
    Date:
        2010-12-8 16:57:35
 **************************************************************************************************/
void on2register::SetRegisterFile(RK_U32 id, RK_U32 value)
{
    RK_U32 tmp;

    tmp = regBase[hwDecRegSpec[id][0]];
    tmp &= ~(regMask[hwDecRegSpec[id][1]] << hwDecRegSpec[id][2]);
    tmp |= (value & regMask[hwDecRegSpec[id][1]]) << hwDecRegSpec[id][2];
    regBase[hwDecRegSpec[id][0]] = tmp;
}

/***************************************************************************************************
    Func:
        GetRegisterFile
    Description:
        read register-mapped memory (not hw register)
    Author:
        Jian Huan
    Date:
        2010-12-8 16:57:35
 **************************************************************************************************/
RK_U32 on2register::GetRegisterFile(RK_U32 id)
{
    RK_U32 tmp;

    tmp = regBase[hwDecRegSpec[id][0]];
    tmp = tmp >> hwDecRegSpec[id][2];
    tmp &= regMask[hwDecRegSpec[id][1]];
    return (tmp);
}

/***************************************************************************************************
    Func:
        HwStart
    Description:
        setup hw register
    Author:
        Jian Huan
    Date:
        2010-11-19 15:15:58
 **************************************************************************************************/
RK_S32 on2register::HwStart()
{
#ifndef WIN32
    //disable HW
    WriteReg(1, 0);

    //write memroy to register files
    FlushRegs();

    //enable HW
    WriteReg(1, 1);
#endif
    return VPU_OK;
}

/**************************************************************************************
    Func:
        HwFinish
    Description:
        waiting for HW finish working!
    Author:
        Jian Huan
    Date:
        2010-11-19 15:42:54
 **************************************************************************************/
RK_S32 on2register::HwFinish()
{
#ifndef WIN32
    int reg_data;

    reg_data = rd(VDPU_BASE + 1 * 4);

    //wait decoder IRQ and interrupt status bit
    while ((reg_data&0xffff00) != 0x1100)
    {

        /*__asm{
          nop;
          nop;
          nop;
          nop;
          nop;
          nop;
          nop;
        }*/

        reg_data = rd(VDPU_BASE + 1 * 4);
    }
#endif
    return VPU_OK;
}
