#ifndef VPU_API_H_
#define VPU_API_H_
#include "vpu_global.h"

typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
   int	qp;
   int	enableCabac;
   int	cabacInitIdc;
   int format;
   int  intraPicRate;
   int	reserved[6];
}EncParams1;

typedef enum
{
    H264ENC_YUV420_PLANAR = 0,  /* YYYY... UUUU... VVVV */
    H264ENC_YUV420_SEMIPLANAR = 1,  /* YYYY... UVUVUV...    */
    H264ENC_YUV422_INTERLEAVED_YUYV = 2,    /* YUYVYUYV...          */
    H264ENC_YUV422_INTERLEAVED_UYVY = 3,    /* UYVYUYVY...          */
    H264ENC_RGB565 = 4, /* 16-bit RGB           */
    H264ENC_BGR565 = 5, /* 16-bit RGB           */
    H264ENC_RGB555 = 6, /* 15-bit RGB           */
    H264ENC_BGR555 = 7, /* 15-bit RGB           */
    H264ENC_RGB444 = 8, /* 12-bit RGB           */
    H264ENC_BGR444 = 9, /* 12-bit RGB           */
    H264ENC_RGB888 = 10,    /* 24-bit RGB           */
    H264ENC_BGR888 = 11,    /* 24-bit RGB           */
    H264ENC_RGB101010 = 12, /* 30-bit RGB           */
    H264ENC_BGR101010 = 13  /* 30-bit RGB           */
} H264EncPictureType;

/*typedef enum {
    ON2_CODINT_TYPE_UNKNOW = -1,
    ON2_CODINT_TYPE_M2V,
    ON2_CODINT_TYPE_H263,
    ON2_CODINT_TYPE_M4V,
    ON2_CODINT_TYPE_RV,
    ON2_CODINT_TYPE_VC1,
    ON2_CODINT_TYPE_VP6,
    ON2_CODINT_TYPE_VP8,
    ON2_CODINT_TYPE_AVC,
    ON2_CODINT_TYPE_AVC_FLASH,
    ON2_CODINT_TYPE_JPEG,

    ON2_CODINT_TYPE_AVC_ENC = 0x100,
} On2CodingType;*/

/**
 * Enumeration used to define the possible video compression codings.
 * NOTE:  This essentially refers to file extensions. If the coding is
 *        being used to specify the ENCODE type, then additional work
 *        must be done to configure the exact flavor of the compression
 *        to be used.  For decode cases where the user application can
 *        not differentiate between MPEG-4 and H.264 bit streams, it is
 *        up to the codec to handle this.
 */

//sync with the omx_video.h
typedef enum OMX_ON2_VIDEO_CODINGTYPE {
    OMX_ON2_VIDEO_CodingUnused,     /**< Value when coding is N/A */
    OMX_ON2_VIDEO_CodingAutoDetect, /**< Autodetection of coding type */
    OMX_ON2_VIDEO_CodingMPEG2,      /**< AKA: H.262 */
    OMX_ON2_VIDEO_CodingH263,       /**< H.263 */
    OMX_ON2_VIDEO_CodingMPEG4,      /**< MPEG-4 */
    OMX_ON2_VIDEO_CodingWMV,        /**< all versions of Windows Media Video */
    OMX_ON2_VIDEO_CodingRV,         /**< all versions of Real Video */
    OMX_ON2_VIDEO_CodingAVC,        /**< H.264/AVC */
    OMX_ON2_VIDEO_CodingMJPEG,      /**< Motion JPEG */
    OMX_ON2_VIDEO_CodingFLV1 = 0x01000000,       /**< Sorenson H.263 */
    OMX_ON2_VIDEO_CodingDIVX3,                   /**< DIVX3 */
    OMX_ON2_VIDEO_CodingVPX,                     /**< VP8 */
    OMX_ON2_VIDEO_CodingVP6,
    OMX_ON2_VIDEO_EncodingAVC = 0x0200000,
    OMX_ON2_VIDEO_CodingKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    OMX_ON2_VIDEO_CodingVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    OMX_ON2_VIDEO_CodingMax = 0x7FFFFFFF
} OMX_ON2_VIDEO_CODINGTYPE;



extern void*         get_class_On2AvcDecoder(void);
extern int          init_class_On2AvcDecoder(void * AvcDecoder,int tsFlag);
extern void      destroy_class_On2AvcDecoder(void * AvcDecoder);
extern int        deinit_class_On2AvcDecoder(void * AvcDecoder);
extern int  dec_oneframe_class_On2AvcDecoder(void * AvcDecoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize,long long *InputTimestamp);
extern void reset_class_On2AvcDecoder(void * AvcDecoder);

extern void*         get_class_On2M4vDecoder(void);
extern void      destroy_class_On2M4vDecoder(void * M4vDecoder);
extern int          init_class_On2M4vDecoder(void * M4vDecoder, VPU_GENERIC *vpug);
extern int        deinit_class_On2M4vDecoder(void * M4vDecoder);
extern int         reset_class_On2M4vDecoder(void * M4vDecoder);
extern int  dec_oneframe_class_On2M4vDecoder(void * M4vDecoder,unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);

extern void*        get_class_On2H263Decoder(void);
extern void     destroy_class_On2H263Decoder(void * H263Decoder);
extern int         init_class_On2H263Decoder(void * H263Decoder, VPU_GENERIC *vpug);
extern int       deinit_class_On2H263Decoder(void * H263Decoder);
extern int        reset_class_On2H263Decoder(void * H263Decoder);
extern int dec_oneframe_class_On2H263Decoder(void * H263Decoder,unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);

extern void*         get_class_On2M2vDecoder(void);
extern void      destroy_class_On2M2vDecoder(void * M2vDecoder);
extern int          init_class_On2M2vDecoder(void * M2vDecoder);
extern int        deinit_class_On2M2vDecoder(void * M2vDecoder);
extern int         reset_class_On2M2vDecoder(void * M2vDecoder);
extern int  dec_oneframe_class_On2M2vDecoder(void * M2vDecoder,unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
extern int  get_oneframe_class_On2M2vDecoder(void * M2vDecoder, unsigned char* aOutBuffer, unsigned int* aOutputLength);

extern void*          get_class_On2RvDecoder(void);
extern void       destroy_class_On2RvDecoder(void * RvDecoder);
extern int           init_class_On2RvDecoder(void * RvDecoder);
extern int         deinit_class_On2RvDecoder(void * RvDecoder);
extern int          reset_class_On2RvDecoder(void * RvDecoder);
extern int   dec_oneframe_class_On2RvDecoder(void * RvDecoder,unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
extern void         get_width_Height_class_On2RvDecoder(void * RvDecoder, unsigned int* widht, unsigned int* height);

extern void*         get_class_On2Vp8Decoder(void);
extern void      destroy_class_On2Vp8Decoder(void * Vp8Decoder);
extern int          init_class_On2Vp8Decoder(void * Vp8Decoder);
extern int        deinit_class_On2Vp8Decoder(void * Vp8Decoder);
extern int         reset_class_On2Vp8Decoder(void * Vp8Decoder);
extern int  dec_oneframe_class_On2Vp8Decoder(void * Vp8Decoder,unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);

extern void*         get_class_On2Vc1Decoder(void);
extern void      destroy_class_On2Vc1Decoder(void * Vc1Decoder);
extern int          init_class_On2Vc1Decoder(void * Vc1Decoder, unsigned char *tmpStrm, unsigned int size,unsigned int extraDataSize);
extern int        deinit_class_On2Vc1Decoder(void * Vc1Decoder);
extern int         reset_class_On2Vc1Decoder(void * Vc1Decoder);
extern int  dec_oneframe_class_On2Vc1Decoder(void * Vc1Decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize, long long aTimeStamp);

extern void*         get_class_On2Vp6Decoder(void);
extern void      destroy_class_On2Vp6Decoder(void * Vp6Decoder);
extern int          init_class_On2Vp6Decoder(void * Vp6Decoder, int codecid);
extern int        deinit_class_On2Vp6Decoder(void * Vp6Decoder);
extern int         reset_class_On2Vp6Decoder(void * Vp6Decoder);
extern int  dec_oneframe_class_On2Vp6Decoder(void * Vp6Decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);

extern void*         get_class_On2JpegDecoder(void);
extern void      destroy_class_On2JpegDecoder(void * JpegDecoder);
extern int          init_class_On2JpegDecoder(void * JpegDecoder);
extern int        deinit_class_On2JpegDecoder(void * JpegDecoder);
extern int         reset_class_On2JpegDecoder(void * JpegDecoder);
extern int  dec_oneframe_class_On2JpegDecoder(void * JpegDecoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);

extern void* get_class_On2AvcEncoder(void);
extern void  destroy_class_On2AvcEncoder(void * AvcEncoder);
extern int init_class_On2AvcEncoder(void * AvcEncoder, EncParams1 *aEncOption, unsigned char* aOutBuffer,unsigned long * aOutputLength);
extern int deinit_class_On2AvcEncoder(void * AvcEncoder);
extern int enc_oneframe_class_On2AvcEncoder(void * AvcEncoder, unsigned char* aOutBuffer, unsigned int * aOutputLength,unsigned char* aInputBuf,unsigned int  aInBuffPhy,unsigned int *aInBufSize,unsigned int * aOutTimeStamp, int* aSyncFlag);
extern void enc_getconfig_class_On2AvcEncoder(void * AvcEncoder,EncParams1* vpug);
extern void enc_setconfig_class_On2AvcEncoder(void * AvcEncoder,EncParams1* vpug);
extern int enc_setInputFormat_class_On2AvcEncoder(void * AvcEncoder,H264EncPictureType inputFormat);
extern void enc_SetintraPeriodCnt_class_On2AvcEncoder(void * AvcEncoder);
extern void enc_SetInputAddr_class_On2AvcEncoder(void * AvcEncoder,unsigned long input);

extern void *  get_class_On2Vp8Encoder(void);
extern void  destroy_class_On2Vp8Encoder(void * Vp8Encoder);
extern int init_class_On2Vp8Encoder(void * Vp8Encoder, EncParams1 *aEncOption, unsigned char* aOutBuffer,unsigned int* aOutputLength);
extern int deinit_class_On2Vp8Encoder(void * Vp8Encoder);
extern int enc_oneframe_class_On2Vp8Encoder(void * Vp8Encoder, unsigned char* aOutBuffer, unsigned int* aOutputLength,
                                     unsigned char *aInBuffer,unsigned int aInBuffPhy,unsigned int* aInBufSize,unsigned int* aOutTimeStamp, int *aSyncFlag);

typedef struct tag_VPU_API {
    void* (*         get_class_On2Decoder)(void);
    void  (*     destroy_class_On2Decoder)(void *decoder);
    int   (*      deinit_class_On2Decoder)(void *decoder);
    int   (*        init_class_On2Decoder)(void *decoder);
    int   (*        init_class_On2Decoder_M4VH263)(void *decoder, VPU_GENERIC *vpug);
    int   (*        init_class_On2Decoder_VC1)(void *decoder, unsigned char *tmpStrm, unsigned int size,unsigned int extraDataSize);
    int   (*        init_class_On2Decoder_VP6)(void *decoder, int codecid);
	int   (*        init_class_On2Decoder_AVC)(void *decoder,int tsFlag);
    int   (*       reset_class_On2Decoder)(void *decoder);
    int   (*dec_oneframe_class_On2Decoder)(void *decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
    int   (*dec_oneframe_class_On2Decoder_WithTimeStamp)(void *decoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize, long long *InputTimestamp);
    int   (*get_oneframe_class_On2Decoder)(void *decoder, unsigned char* aOutBuffer, unsigned int* aOutputLength);
    void  (*get_width_Height_class_On2Decoder_RV)(void *decoder, unsigned int* width, unsigned int* height);

	void* (*         get_class_On2Encoder)(void);
    void  (*     destroy_class_On2Encoder)(void *encoder);
    int   (*      deinit_class_On2Encoder)(void *encoder);
    int   (*        init_class_On2Encoder)(void *encoder,EncParams1 *aEncOption, unsigned char * aOutBuffer,unsigned int* aOutputLength);
    int   (*enc_oneframe_class_On2Encoder)(void * ncoder, unsigned char* aOutBuffer, unsigned int * aOutputLength,unsigned char* aInputBuf,unsigned int  aInBuffPhy,unsigned int *aInBufSize,unsigned int * aOutTimeStamp, int* aSyncFlag);
    void  (*enc_getconfig_class_On2Encoder)(void * AvcEncoder,EncParams1* vpug);
    void  (*enc_setconfig_class_On2Encoder)(void * AvcEncoder,EncParams1* vpug);
    int   (*enc_setInputFormat_class_On2Encoder)(void * AvcEncoder,H264EncPictureType inputFormat);
    void  (*enc_SetintraPeriodCnt_class_On2Encoder)(void * AvcEncoder);
    void  (*enc_SetInputAddr_class_On2Encoder)(void * AvcEncoder,unsigned long input);
} VPU_API;
#ifdef __cplusplus
extern "C"
{
#endif
 void vpu_api_init(VPU_API *vpu_api, OMX_ON2_VIDEO_CODINGTYPE video_coding_type);
#ifdef __cplusplus
}
#endif

#endif

