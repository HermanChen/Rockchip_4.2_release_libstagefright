/******************************************************************************
 * xmedia_demuxers.h
 *
 * Copyright (c) 2009 Fuzhou Rockchip Co.,Ltd.
 *
 * DESCRIPTION: -
 *     Media demuxers interface.
 *
 * modification history
 * --------------------
 * Keith Lin, Feb 17, 2009,  Initial version
 * --------------------
 ******************************************************************************/

#ifndef XMEDIA_DEMUXERS_H
#define XMEDIA_DEMUXERS_H

#include <inttypes.h>

#define XMEDIA_DEMUX_MAX_STREAMS 8

typedef enum XMDemuxerID
{
    XMEDIA_DEMUXER_AVI = 0,
    XMEDIA_DEMUXER_MKV,
    XMEDIA_DEMUXER_RM,
    XMEDIA_DEMUXER_FLV,
    XMEDIA_DEMUXER_MPEG,
    XMEDIA_DEMUXER_MOV,
    XMEDIA_DEMUXER_ASF,

    XMEDIA_DEMUXER_MAX = 32
} XMDemuxerID;

typedef enum XMDemuxErr
{
    XMDEMUX_ERROR_NONE = 0,
    XMDEMUX_ERROR_FORMAT,
    XMDEMUX_ERROR_UNSUPPORTED,
    XMDEMUX_ERROR_MEMORY,
    XMDEMUX_ERROR_STREAM_CORRUPT,
    XMDEMUX_ERROR_STREAM_IO,
    XMDEMUX_ERROR_STREAM_EOF,
    XMDEMUX_ERROR_NO_VIDEOSTREAM
} XMDemuxErr;

typedef enum XMCodecID
{
    XM_CODEC_ID_NONE,

    /* video codecs */
    XM_CODEC_ID_MPEG1VIDEO,
    XM_CODEC_ID_MPEG2VIDEO,
    XM_CODEC_ID_H263,
    XM_CODEC_ID_RV10,
    XM_CODEC_ID_RV20,
    XM_CODEC_ID_MJPEG,
    XM_CODEC_ID_MPEG4,
    XM_CODEC_ID_MSMPEG4V1,
    XM_CODEC_ID_MSMPEG4V2,
    XM_CODEC_ID_MSMPEG4V3,
    XM_CODEC_ID_WMV1,
    XM_CODEC_ID_WMV2,
    XM_CODEC_ID_FLV1,
    XM_CODEC_ID_H264,
    XM_CODEC_ID_RV30,
    XM_CODEC_ID_RV40,
    XM_CODEC_ID_VC1,
    XM_CODEC_ID_WMV3,
    XM_CODEC_ID_BMP,
    XM_CODEC_ID_AVS,
    XM_CODEC_ID_VP6,

    /* PCM */
    XM_CODEC_ID_PCM = 0x10000,

    /* ADPCM */
    XM_CODEC_ID_ADPCM = 0x11000,

    /* AMR */
    XM_CODEC_ID_AMR_NB = 0x12000,
    XM_CODEC_ID_AMR_WB,

    /* RealAudio codecs*/
    XM_CODEC_ID_RA_144 = 0x13000,
    XM_CODEC_ID_RA_288,

    /* Other audio codecs */
    XM_CODEC_ID_MP2 = 0x15000,
    XM_CODEC_ID_MP3,
    XM_CODEC_ID_AAC,
    XM_CODEC_ID_AC3,
    XM_CODEC_ID_DTS,
    XM_CODEC_ID_WMAV1,
    XM_CODEC_ID_WMAV2,
    XM_CODEC_ID_FLAC,
    XM_CODEC_ID_APE,
    XM_CODEC_ID_WMAPRO
}XMCodecID;

typedef enum StreamType
{
    STREAM_TYPE_UNKNOWN = -1,
    STREAM_TYPE_VIDEO,
    STREAM_TYPE_AUDIO,
    STREAM_TYPE_DATA,
    STREAM_TYPE_SUBTITLE
} StreamType;

typedef struct AudioStream
{
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bps;
    uint16_t encoding;
    uint16_t align;
    uint16_t bits_per_sample;
    uint16_t extra_len;
    uint8_t *extra_data;
} AudioStream;

typedef struct VideoStream
{
    uint32_t encoding;
//    uint32_t ms_per_frame;
    uint32_t width;
    uint32_t height;
    uint32_t display_width;
    uint32_t display_height;
    uint16_t extra_len;
    uint8_t *extra_data;
} VideoStream;

typedef struct SubtitleStream {
    uint16_t extra_len;
    uint8_t *extra_data;
} SubtitleStream;

typedef struct XMStream
{
    /* type of the stream */
    StreamType type;

    /* codec ID of the stream, in ms. */
    XMCodecID codec_id;

    /* pts of the first frame of the stream, in ms. */
    uint32_t start_time;

    /* duration of the stream, in ms. */
    uint32_t duration;

    uint64_t base_time;

    /* ISO 639 3-letter language code (empty string if undefined) */
    char *language;

    /* number of frames in this stream if known or 0 */
    int64_t nb_frames;

    /* codec specific data */
    union
    {
        AudioStream audio;
        VideoStream video;
		SubtitleStream subtitle;//addby for subtitle data;
    } specific;

} XMStream;

typedef struct XMFormatContext
{
    /* private data */
    void *priv_data;

    /* stream number of this media file */
    uint32_t nb_streams;

    /* stream information */
    XMStream streams[XMEDIA_DEMUX_MAX_STREAMS];

    /* duration of the stream, in ms. */
    int32_t duration;

} XMFormatContext;

typedef struct XMPacket
{
    /* Presentation timestamp in ms units. */
    double pts;

    /* data of this packet */
    uint8_t *data;

    /* size of this packet */
    int   size;

    /* stream ID of this packet */
    int   stream_index;

    /* indicate key frame or not */
    int   flags;

    /* Duration of this packet in ms units. */
    int   duration;

    /* private data size */
    int   priv_size;

    /* private data */
    void  *priv;
} XMPacket;

void xmedia_register_demuxer(void *demuxer);

XMDemuxErr xmedia_init_pkt(XMPacket *pkt);

XMDemuxErr xmedia_read_pkt(XMFormatContext *ifctx, XMPacket *pkt);

XMDemuxErr xmedia_seek_pkt(XMFormatContext *ifctx, uint32_t timestamp);

void xmedia_free_pkt(XMPacket *pkt);

void xmedia_close_input_file(XMFormatContext *ifctx);

#endif
