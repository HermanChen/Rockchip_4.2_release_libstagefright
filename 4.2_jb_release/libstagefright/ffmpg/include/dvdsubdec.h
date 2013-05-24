#ifndef DVD_SUB_DEC_H_
#define DVD_SUB_DEC_H_

#include "avformat.h"
#include "avcodec.h"
#include "colorspace.h"
#include "get_bits.h"

#ifndef ALPHA_BLEND
#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))
#endif

#ifndef RGBA_IN
#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}
#endif

#ifndef YUVA_IN
#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}
#endif

#ifndef YUVA_OUT
#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}
#endif

#ifndef BPP
#define BPP 1
#endif

namespace android {

typedef struct DVDSubContext
{
  uint32_t palette[16];
  int      has_palette;
  uint8_t  colormap[4];
  uint8_t  alpha[256];
} DVDSubContext;

typedef struct SubPicture {
    int64_t pts; /* presentation time stamp for this picture */
    uint32_t durationMs;
    AVSubtitle sub;
} SubPicture;

class DvdSubtitleDecoder
{
public:
	DvdSubtitleDecoder();
	~DvdSubtitleDecoder();

    int dvdsub_init(AVCodecContext *avctx);

    int dvdsub_decode(AVCodecContext *avctx,
                         void *data, int *data_size,
                         AVPacket *avpkt);


private:

};

}// namespace android

#endif  // DVD_SUB_DEC_H_

