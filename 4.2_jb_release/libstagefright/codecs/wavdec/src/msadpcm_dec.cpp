#include        <stdio.h>

#define WAV_INCLUDE
#ifdef WAV_INCLUDE





#include "stdlib.h"
#include "string.h"
#include "PCM.h"

#define _ATTR_WAVDEC_BSS_ 
#define _ATTR_WAVDEC_DATA_
#define _ATTR_WAVDEC_TEXT_



#define MSADPCM_ADAPT_COEFF_COUNT       7
//----------------------------------


/* These required here because we write the header in this file. */

#define RIFF_MARKER     (MAKE_MARKER ('R', 'I', 'F', 'F'))
#define WAVE_MARKER     (MAKE_MARKER ('W', 'A', 'V', 'E'))
#define fmt_MARKER      (MAKE_MARKER ('f', 'm', 't', ' '))
#define fact_MARKER     (MAKE_MARKER ('f', 'a', 'c', 't'))
#define data_MARKER     (MAKE_MARKER ('d', 'a', 't', 'a'))

#define WAVE_FORMAT_MS_ADPCM    0x0002



/*============================================================================================
** MS ADPCM static data and functions.
*/
_ATTR_WAVDEC_DATA_
static int AdaptationTable []    =
{       230, 230, 230, 230, 307, 409, 512, 614,
        768, 614, 512, 409, 307, 230, 230, 230
} ;

/* TODO : The first 7 coef's are are always hardcode and must
   appear in the actual WAVE file.  They should be read in
   in case a sound program added extras to the list. */
_ATTR_WAVDEC_DATA_
/*static*/ int OmxAdaptCoeff1 [MSADPCM_ADAPT_COEFF_COUNT] =
{       256, 512, 0, 192, 240, 460, 392
} ;

_ATTR_WAVDEC_DATA_
/*static*/ int OmxAdaptCoeff2 [MSADPCM_ADAPT_COEFF_COUNT] =
{       0, -256, 0, 64, 0, -208, -232
} ;

/*============================================================================================
**      MS ADPCM Block Layout.
**      ======================
**      Block is usually 256, 512 or 1024 bytes depending on sample rate.
**      For a mono file, the block is laid out as follows:
**              byte    purpose
**              0               block predictor [0..6]
**              1,2             initial idelta (positive)
**              3,4             sample 1
**              5,6             sample 0
**              7..n    packed bytecodes
**
**      For a stereo file, the block is laid out as follows:
**              byte    purpose
**              0               block predictor [0..6] for left channel
**              1               block predictor [0..6] for right channel
**              2,3             initial idelta (positive) for left channel
**              4,5             initial idelta (positive) for right channel
**              6,7             sample 1 for left channel
**              8,9             sample 1 for right channel
**              10,11   sample 0 for left channel
**              12,13   sample 0 for right channel
**              14..n   packed bytecodes
*/
#define sf_count_t int

/*============================================================================================
** Static functions.
*/
_ATTR_WAVDEC_TEXT_
 int     omx_msadpcm_decode_block (/*SF_PRIVATE *psf, */MSADPCM_PRIVATE *pms) ;

_ATTR_WAVDEC_TEXT_
static sf_count_t omx_msadpcm_read_block (/*SF_PRIVATE *psf, */MSADPCM_PRIVATE *pms, short *ptr, int len) ;

_ATTR_WAVDEC_TEXT_
sf_count_t       omx_msadpcm_read_s (/*SF_PRIVATE *psf,*/MSADPCM_PRIVATE *pms, short *ptr, sf_count_t len) ;


/*============================================================================================
** MS ADPCM Read Functions.
*/

_ATTR_WAVDEC_BSS_
//char fdata_buf[14312];

_ATTR_WAVDEC_TEXT_

int omx_msadpcm_dec_init (tPCM *pPCM, MSADPCM_PRIVATE *pms)
{
        memset (pms, 0, sizeof(MSADPCM_PRIVATE)) ;
        pms->block   = (unsigned char *)malloc(pPCM->usBytesPerBlock);//(unsigned char*) pms->dummydata ;
        pms->samples = (short*) malloc(pPCM->ucChannels * pPCM->usSamplesPerBlock * 2);//(pms->dummydata + pPCM->usBytesPerBlock);

        pms->channels        = pPCM->ucChannels;
        pms->blocksize       = pPCM->usBytesPerBlock;
        pms->samplesperblock = pPCM->usSamplesPerBlock ;

       
      	pms->dataremaining = pPCM->ulLength;

        if ( pPCM->ulLength % pms->blocksize)
                pms->blocks = pPCM->ulLength / pms->blocksize  + 1 ;
        else
                pms->blocks = pPCM->ulLength / pms->blocksize ;
        return 0 ;
} /* wav_w64_msadpcm_init */

_ATTR_WAVDEC_TEXT_
 int omx_msadpcm_decode_block (/*SF_PRIVATE *psf, */MSADPCM_PRIVATE *pms)
{       int             chan, k, blockindx, sampleindx ;
        short   bytecode, bpred [2], chan_idelta [2] ;

	    int predict ;
	    int current ;
	    int idelta ;

        pms->blockcount ++ ;
        pms->samplecount = 0 ;

        if (pms->blockcount > pms->blocks)
        {
		        memset (pms->samples, 0, pms->samplesperblock * pms->channels) ;
                return 1 ;
        };

        //if ((k = psf_fread (pms->block, 1, pms->blocksize, psf)) != pms->blocksize)
		//if ((k = psf_fread (pms->block, 1, pms->blocksize, pRawFileCache)) != pms->blocksize)
		//if ((k = fread (pms->block, 1, pms->blocksize, pRawFileCache)) != pms->blocksize)
		{
		//TODO: ERROR HANDLER
        //        psf_log_printf (psf, "*** Warning : short read (%d != %d).\n", k, pms->blocksize) ;
        //    return 0;
		}
		k = pms->blocksize;//add by Charles Chen 09.12.14
        /* Read and check the block header. */

        if (pms->channels == 1)
        {       bpred [0] = pms->block [0] ;

                if (bpred [0] >= 7)
				{
						//TODO: ERROR HANDLER
                        //psf_log_printf (psf, "MS ADPCM synchronisation error (%d).\n", bpred [0]) ;
				}

                chan_idelta [0] = pms->block [1] | (pms->block [2] << 8) ;
                chan_idelta [1] = 0 ;

                //psf_log_printf (psf, "(%d) (%d)\n", bpred [0], chan_idelta [0]) ;

                pms->samples [1] = pms->block [3] | (pms->block [4] << 8) ;
                pms->samples [0] = pms->block [5] | (pms->block [6] << 8) ;
                blockindx = 7 ;
                }
        else
        {       bpred [0] = pms->block [0] ;
                bpred [1] = pms->block [1] ;

                if (bpred [0] >= 7 || bpred [1] >= 7)
				{
				//TODO: ERROR HANDLER
                //      psf_log_printf (psf, "MS ADPCM synchronisation error (%d %d).\n", bpred [0], bpred [1]) ;
				}

                chan_idelta [0] = pms->block [2] | (pms->block [3] << 8) ;
                chan_idelta [1] = pms->block [4] | (pms->block [5] << 8) ;

                //psf_log_printf (psf, "(%d, %d) (%d, %d)\n", bpred [0], bpred [1], chan_idelta [0], chan_idelta [1]) ;

                pms->samples [2] = pms->block [6] | (pms->block [7] << 8) ;
                pms->samples [3] = pms->block [8] | (pms->block [9] << 8) ;

                pms->samples [0] = pms->block [10] | (pms->block [11] << 8) ;
                pms->samples [1] = pms->block [12] | (pms->block [13] << 8) ;

                blockindx = 14 ;
                } ;

        /*--------------------------------------------------------
        This was left over from a time when calculations were done
        as ints rather than shorts. Keep this around as a reminder
        in case I ever find a file which decodes incorrectly.

		if (chan_idelta [0] & 0x8000)
					chan_idelta [0] -= 0x10000 ;
		if (chan_idelta [1] & 0x8000)
					chan_idelta [1] -= 0x10000 ;
        --------------------------------------------------------*/

        /* Pull apart the packed 4 bit samples and store them in their
        ** correct sample positions.
        */

        sampleindx = 2 * pms->channels ;
        while (blockindx <  pms->blocksize)
        {       bytecode = pms->block [blockindx++] ;
                pms->samples [sampleindx++] = (bytecode >> 4) & 0x0F ;
                pms->samples [sampleindx++] = bytecode & 0x0F ;
                } ;

        /* Decode the encoded 4 bit samples. */

        for (k = 2 * pms->channels ; k < (pms->samplesperblock * pms->channels) ; k ++)
        {       chan = (pms->channels > 1) ? (k % 2) : 0 ;

                bytecode = pms->samples [k] & 0xF ;

            /** Compute next Adaptive Scale Factor (ASF) **/
            idelta = chan_idelta [chan] ;
            chan_idelta [chan] = (AdaptationTable [bytecode] * idelta) >> 8 ; /* => / 256 => FIXED_POINT_ADAPTATION_BASE == 256 */
            if (chan_idelta [chan] < 16)
                        chan_idelta [chan] = 16 ;
            if (bytecode & 0x8)
                        bytecode -= 0x10 ;

        predict = ((pms->samples [k - pms->channels] * OmxAdaptCoeff1 [bpred [chan]])
                                        + (pms->samples [k - 2 * pms->channels] * OmxAdaptCoeff2 [bpred [chan]])) >> 8 ; /* => / 256 => FIXED_POINT_COEFF_BASE == 256 */
        current = (bytecode * idelta) + predict;

            if (current > 32767)
                        current = 32767 ;
            else if (current < -32768)
                        current = -32768 ;

                pms->samples [k] = current ;
                } ;

        return 1 ;
} /* msadpcm_decode_block */

_ATTR_WAVDEC_TEXT_
static sf_count_t
omx_msadpcm_read_block (/*SF_PRIVATE *psf, */MSADPCM_PRIVATE *pms, short *ptr, int len)
{       int     count, total = 0, indx = 0 ;

        while (indx < len)
        {       if (pms->blockcount >= pms->blocks && pms->samplecount >= pms->samplesperblock)
                {       memset (&(ptr[indx]), 0, (size_t) ((len - indx) * sizeof (short))) ;
                        return total ;
                        } ;

                if (pms->samplecount >= 0)
				{
					if (0==omx_msadpcm_decode_block (pms))
						return 0;
				}

                count = (pms->samplesperblock - pms->samplecount) * pms->channels ;
                count = (len - indx > count) ? count : len - indx ;

                memcpy (&(ptr[indx]), &(pms->samples [pms->samplecount * pms->channels]), count * sizeof (short)) ;
                indx += count ;
                pms->samplecount += count / pms->channels ;
                total = indx ;
                } ;

        return total ;
} /* msadpcm_read_block */

_ATTR_WAVDEC_TEXT_
sf_count_t omx_msadpcm_read_s (/*SF_PRIVATE *psf, */MSADPCM_PRIVATE *pms, short *ptr, sf_count_t len)
{       
        int                     readcount, count ;
        sf_count_t      total = 0 ;
#if 0
        if (! psf->fdata)
                return 0 ;
        pms = (MSADPCM_PRIVATE*) psf->fdata ;
#endif
        while (len > 0)
        {       readcount = len ; //(len > 0x10000000) ? 0x10000000 : (int) len ;

                count = omx_msadpcm_read_block (/*psf, */pms, ptr, readcount) ;

				/* fread failed , by Vincent Hsiung @ May 13 */
				if (count == 0)
					return 0;

                total += count ;
                len -= count ;
                if (count != readcount)
                        break ;
        } ;

		//mono to stereo , add by Vincent 
		if (pms->channels == 1)
		{
			int i ;
			for ( i = total ; i >= 0 ; i--)
			{
				ptr[i*2] 		= ptr[i];
				ptr[i*2 + 1]	= ptr[i];
			}
		}

        return total ;
} /* msadpcm_read_s */

#endif

