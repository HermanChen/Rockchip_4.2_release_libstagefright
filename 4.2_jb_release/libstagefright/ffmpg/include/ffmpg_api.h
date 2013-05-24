/*
 * copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef AVFORMAT_FFMPEG_API_H
#define AVFORMAT_FFMPEG_API_H
#define AVIF_HASINDEX           0x00000010        // Index at end of file?
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800        // Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000
#define AVI_MAX_RIFF_SIZE       0x40000000LL
#define AVI_MASTER_INDEX_SIZE   256

#define  SEEK_EROR       -1
#define  SEEK_SUCCESS    0
#define  SEEK_FAIL    1

#ifndef uint8
#define uint8 uint8_t
#endif

#ifndef uint16
#define uint16 uint16_t
#endif

#ifndef uint32
#define uint32 uint32_t
#endif

#ifndef uint64
#define uint64 uint64_t
#endif

#ifndef int32
#define int32 int32_t
#endif

#ifndef int64
#define int64 int64_t
#endif

#define AVI_PCM
#define AVI_MAX_AUDIO_WAVFMT_SIZE		20
#define AVI_WMA_HEAD_SIZE               18

typedef struct
{
    uint16_t  FormatTag;
    uint16_t  Channels;
    uint32_t  SamplesPerSec;
    uint32_t  AvgBytesPerSec;
    uint16_t  BlockAlign;
    uint16_t  BitsPerSample;
    uint16_t  Size;
	uint16_t  SamplesPerBlock;

}WaveFormatExStruct;

typedef struct
{
    uint32_t    headLen;
    uint8_t*    data;

}VC1StreamRCVInfo;

typedef struct
{
    uint32_t    len;
    uint8_t*    data;
    uint8_t     sendFlag;
}VC1ExtraInfo;


typedef struct {
    uint16_t wFormatTag;        /* format type */
    uint16_t nChannels;         /* number of channels (i.e. mono, stereo...) */
    uint32_t nSamplesPerSec;    /* sample rate */
    uint32_t nAvgBytesPerSec;   /* for buffer estimation */
    uint16_t nBlockAlign;       /* block size of data */
    uint16_t wBitsPerSample;    /* Number of bits per sample of mono data */
    uint16_t cbSize;            /* The count in bytes of the size of
                                   * extra information (after cbSize) */
} WmaFormat;

typedef struct
{
    uint32_t    headLen;
    uint8_t*    data;
    uint32_t        headSend;
}WmaInfo;

typedef struct
{
    uint32_t    start_code;
    uint32_t    size;
    uint32_t    timel;
    uint32_t    timeh;
    uint32_t    type;
    uint32_t    slices;
    uint32_t    retFlag;
    uint32_t    res[1];
}Mpeg2VideoSteamHead;



/* index flags */
#define AVIIF_INDEX             0x10
#include "avformat.h"
#include "dv.h"
#ifdef __cplusplus
extern "C"
{
#endif

#define NULL_IF_CONFIG_SMALL(x) NULL
int avi_read_packet(AVFormatContext *s, AVPacket *pkt);
int avi_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags);
int avi_read_init(AVFormatContext *s,const char *filename,void* pReader);
int avi_read_close(AVFormatContext *s);
int file_open(AVFormatContext *s,const char *filename,void* pReader);
int mov_read_init(AVFormatContext *s,const char *filename,void* pReader);
int mov_read_close(AVFormatContext *s);
int mov_read_seek(AVFormatContext *s, int stream_index, int64_t sample_time, int flags);
int mov_read_packet(AVFormatContext *s, AVPacket *pkt,int index);

void registerfile();


#ifdef __cplusplus
}
#endif

#endif /* AVFORMAT_FFMPEG_API_H */
