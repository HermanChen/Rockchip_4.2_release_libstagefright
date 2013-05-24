/***************************************************************************************************
    File:
        bitstream.cpp
    Description:
        bitstream class realization
    Auhtor:
        Luo Ning
        Jian Huan
    Date:
        2010-12-8 17:22:16
 **************************************************************************************************/
#include "bitstream_on2.h"
#include <stdio.h>
#include <string.h>
#include <utils/Log.h>

bitstream::bitstream()
{
    strbase = NULL;
    phybase = 0;
    curstr  = NULL;
    pos     = 0;
    leftbits= 0;
    offset8 = 0;

    vpuBits.SliceLength = 0;
    vpuBits.SliceNum    = 0;
    vpuBits.SliceTime.TimeHigh = vpuBits.SliceTime.TimeLow = 0;
    vpuBits.SliceType   = 0xFFFFFFFF;
    vpuBits.StartCode   = 0;
    vpuBits.Res[0] = vpuBits.Res[1] = 0;
}

bitstream::~bitstream()
{
    strbase = NULL;
    phybase = 0;
    curstr  = NULL;
    pos     = 0;
    leftbits= 0;
    offset8 = 0;
}
/***************************************************************************************************
    Func:
        init
    Description:
        bitstream class initialization
    Author:
        Luo Ning
    Date:
        2010-12-8 20:45:20
 **************************************************************************************************/
void bitstream::init(RK_U8 *vir_addr, RK_U32 phy_addr, RK_U32 size)
{
    strbase     = vir_addr;
    phybase     = phy_addr;
    curstr      = vir_addr;
    pos         = 0;
    chunksize   = size;
    readbits    = 0;
    leftbits    = size << 3;

    vpuBits.SliceLength = 0;
    vpuBits.SliceNum    = 0;
    vpuBits.SliceTime.TimeHigh = vpuBits.SliceTime.TimeLow = 0;
    vpuBits.SliceType   = 0xFFFFFFFF;
    vpuBits.StartCode   = 0;
    vpuBits.Res[0] = vpuBits.Res[1] = 0;

    VPU_TRACE("chunksize = 0x%08x, leftbits = 0x%08x\n", chunksize, leftbits);

    offset8 = 0;
}

RK_U32 bitstream::showbits(RK_U32 bits)
{
    RK_U32  temp,result = 0;

    if (leftbits < (RK_S32)bits)
    {
        return 0;
    }

    temp = curstr[0];
    temp <<= (24+pos);
    temp |= (curstr[1]<<(16+pos));
    temp |= (curstr[2]<<(8+pos));
    temp |= (curstr[3]<<(0+pos));

    if (pos)
    {
        result = curstr[4]>>(8-pos);
    }

    result |= temp;

    if (bits)
    {
        return result>>(32-bits);
    }

    return 0;
}

RK_U32 bitstream::skipbits(RK_U32 bits)
{
    RK_U32 tpos;

    if (leftbits < (RK_S32)bits)
    {
        leftbits = 0;
        return 0;
    }

    tpos = bits+pos;

    if (tpos>7)
    {
        curstr += (tpos>>3);
    }

    pos = tpos&7;
    leftbits -= bits;
    readbits += bits;
    return 1;
}
/***************************************************************************************************
    Func:
        getbits
    Description:
        get assigned bits
    Author:
        Jian Huan
    Date:
        2010-12-8 21:10:09
 **************************************************************************************************/
RK_U32 bitstream::getbits(RK_U32 bits)
{
    RK_U32  result;

    result = showbits(bits);
    skipbits(bits);

    return result;
}
/***************************************************************************************************
    Func:
        leftbit
    Description:
        calc the left bits in current bitstream buffer
    Author:
        Jian Huan
    Date:
        2010-12-8 21:10:09
 **************************************************************************************************/
RK_S32 bitstream::leftbit()
{
    return (RK_S32)(chunksize<<3) - (RK_S32)readbits;
}
/***************************************************************************************************
    Func:
        bytealign
    Description:
        align the next byte
    Author:
        Jian Huan
    Date:
        2010-12-8 21:10:09
 **************************************************************************************************/
void bitstream::bytealign()
{
    int remainder = pos & 0x7;

	if (remainder) {
        skipbits(8 - remainder);
    }
}

/***************************************************************************************************
    Func:
        getslice
    Description:
        find the next slice
    Return:
        VPU_OK : if found it;
        VPU_ERR: if not found;
    Author:
        Jian Huan
    Date:
        2010-11-24 10:07:15
 **************************************************************************************************/
RK_S32 bitstream::getslice()
{
    RK_U32 val;

    pos = 0;



    while (1)
    {
        if (leftbits < (RK_S32)sizeof(VPU_BITSTREAM)*8)
        {
            /*@jh: �����ǰʣ���bits����VPU_BITSTREAM�ṹ���size��С���򷵻ش���*/
            VPU_TRACE("lefbits(0x%08x) < 0x%08x\n", leftbits, sizeof(VPU_BITSTREAM)*8);
            return VPU_ERR;
        }

        val = showbits(32);
        VPU_TRACE("val =  0x%08x\n", val);
        skipbits(32);
        break;

        if (val == /*VPU_BITSTREAM_START_CODE*/0x42564b52)
        {
            VPU_TRACE("get header sucess\n");
            skipbits(32);
            break;
        }
        else
        {
            VPU_TRACE("get header error\n");
            skipbits( 8);
        }
    }

    /*@jh: SliceLength��ʾ��ǰ���chunkȥ��RKVBͷ32Bytes��ĳ��ȣ�����Ҳ����������Ȳ�������ȷ�ġ�
           ��������SliceLenght����������£�ʵ����chunk�ĳ�����ż�������ܲ�1�����ܲ�3�����ԣ����
           Ҫ�����ı��һ��chunk�ĳ��ȣ���������ǿ���ס�ġ�*/
    vpuBits.SliceLength         = getbits(32) << 3;
    vpuBits.SliceTime.TimeLow   = getbits(32);
    vpuBits.SliceTime.TimeHigh  = getbits(32);
    vpuBits.SliceType           = getbits(32);
    vpuBits.SliceNum            = getbits(32);
    vpuBits.Res[0]              = getbits(32);
    vpuBits.Res[1]              = getbits(32);

    VPU_TRACE("New chunk is found...\n");
    VPU_TRACE("SliceLength(Bytes) 	= 0x%08x\n", vpuBits.SliceLength >> 3);
    VPU_TRACE("SliceTime.TimeLow  	= 0x%08x\n", vpuBits.SliceTime.TimeLow);
    VPU_TRACE("SliceTime.TimeHigh 	= 0x%08x\n", vpuBits.SliceTime.TimeHigh);
    VPU_TRACE("SliceType 			= 0x%08x\n", vpuBits.SliceType);
    VPU_TRACE("SliceNum 			= 0x%08x\n", vpuBits.SliceNum);

    return VPU_OK;
}

/***************************************************************************************************
    Func:
        getslice
    Description:
        find the next slice (specifically target MPEG-4 Decoding)
    Return:
        VPU_OK:     if found it;
        VPU_ERR:    if not found;
    Author:
        Jian Huan
    Date:
        2010-11-24 10:07:15
 **************************************************************************************************/
RK_S32 bitstream::testslice(RK_U32 bytes_phyaddr, RK_S32 vop)
{
    RK_S32 bytes_consumed = (RK_S32)bytes_phyaddr - (RK_S32)phybase;

    /*@jh: �����ǰʣ�µ��ֽ���������4�ֽڣ�����Ϊ���chunk�ﻹ��һ֡��Ҫ���룬������DIVX*/
    /*@jh: �����и����⣬strbase��8bytes align�ģ�����ʵ�����ĵ�bytes_consumed�Ǻ��п��ܶ���һ֡���ȵģ�
           �����Ȼ����Ϊ��8byte������������ֽڣ�����ֽ�������1B��7B*/
    //VPU_T("bytes_consumed(0x%08x) = bytes_phyaddr(0x%08x)-phybase(0x%08x), chunksize = 0x%08x\n", bytes_consumed, bytes_phyaddr, phybase, chunksize);
    if (((RK_S32)chunksize - bytes_consumed > 4) && vop)
    {
        RK_U32  bytes_8align_phyaddr;
        RK_U32  bytes_8align_consumed;

        //VPU_T("chunksize = (0x%08x)~~Need New Chunk\n", chunksize);

        /*@jh:  [1] ���������ǰλ�õ�8byte�����λ��;
                [2] ���¼���buffer��С;
                [3] ���¸�λʣ�µı�����;
                [4] ���¸�λ�Ѿ���ȡ�ı�����;
                [5] ���¸�λbit offset;
                [6] ���µ�ǰָ��;
                [7] �����趨��׼��ַ,��ˣ��ɼ�ÿһ�������趨���strbase����8-byte-align*/

        bytes_8align_phyaddr    = bytes_phyaddr & (~0x7);
        bytes_8align_consumed   = bytes_8align_phyaddr - phybase;
        offset8                 = bytes_phyaddr & 0x7;

        chunksize               = chunksize - bytes_8align_consumed;
        leftbits                = chunksize << 3;
        readbits                = 0;
        pos                     = 0;

        strbase                 += bytes_8align_consumed;
        curstr                  = strbase;
        phybase                 = bytes_8align_phyaddr;


        return VPU_ERR;
    }

    return VPU_OK;
}


