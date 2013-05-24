/*
 * Copyright (C) 2009 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "WAVExtractor"
#include <utils/Log.h>

#include "include/WAVExtractor.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>
#include <cutils/bitops.h>
#if SUPPORT_ADPCM
#include "codecs/wavdec/include/PCM.h"
#include "codecs/wavdec/include/ImaDecoder.h"
#endif
#define CHANNEL_MASK_USE_CHANNEL_ORDER 0

namespace android {

#if !SUPPORT_ADPCM
enum {
    WAVE_FORMAT_PCM        = 0x0001,
    WAVE_FORMAT_ALAW       = 0x0006,
    WAVE_FORMAT_MULAW      = 0x0007,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE
};
#endif
static const char* WAVEEXT_SUBFORMAT = "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71";


static uint32_t U32_LE_AT(const uint8_t *ptr) {
    return ptr[3] << 24 | ptr[2] << 16 | ptr[1] << 8 | ptr[0];
}

static uint16_t U16_LE_AT(const uint8_t *ptr) {
    return ptr[1] << 8 | ptr[0];
}

struct WAVSource : public MediaSource {
#if SUPPORT_ADPCM
	WAVSource(
            const sp<DataSource> &dataSource,
            const sp<MetaData> &meta,
           	const sp<WAVExtractor> & extractor);
#else
    WAVSource(
            const sp<DataSource> &dataSource,
            const sp<MetaData> &meta,
            uint16_t waveFormat,
            int32_t bitsPerSample,
            off64_t offset, size_t size);
#endif
    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

protected:
    virtual ~WAVSource();

private:
    static const size_t kMaxFrameSize;

    sp<DataSource> mDataSource;
    sp<MetaData> mMeta;
    uint16_t mWaveFormat;
    int32_t mSampleRate;
    int32_t mNumChannels;
    int32_t mBitsPerSample;
    off64_t mOffset;
    size_t mSize;
    bool mStarted;
#if SUPPORT_ADPCM
	uint32_t mOutNumSamples;//output the total num sample after each reset
	int64_t mTimeUs;
	uint32_t mByteRate;
    uint16_t mBlockAlign;
    uint16_t mBytesPerSample;
	uint16_t mSamplesPerBlock;
	uint32_t mNumSamples;
	uint32_t mWBitsPerSample;//the encoder data bits width each sample
	bool isLittleEndian;
	MSADPCM_PRIVATE * pMsAdpcm;
	tPCM * pPCM;
#endif
    MediaBufferGroup *mGroup;
    off64_t mCurrentPos;

    WAVSource(const WAVSource &);
    WAVSource &operator=(const WAVSource &);
};

WAVExtractor::WAVExtractor(const sp<DataSource> &source)
    : mDataSource(source),
      mValidFormat(false),
      mChannelMask(CHANNEL_MASK_USE_CHANNEL_ORDER) {
#if SUPPORT_ADPCM
    isLittleEndian = true;
#endif//SUPPORT_ADPCM
    mInitCheck = init();
}

WAVExtractor::~WAVExtractor() {
}

sp<MetaData> WAVExtractor::getMetaData() {
    sp<MetaData> meta = new MetaData;

    if (mInitCheck != OK) {
        return meta;
    }

    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_CONTAINER_WAV);

    return meta;
}

size_t WAVExtractor::countTracks() {
    return mInitCheck == OK ? 1 : 0;
}

sp<MediaSource> WAVExtractor::getTrack(size_t index) {
    if (mInitCheck != OK || index > 0) {
        return NULL;
    }
#if SUPPORT_ADPCM
	return new WAVSource(mDataSource, mTrackMeta,this);
#else
    return new WAVSource(
            mDataSource, mTrackMeta,
            mWaveFormat, mBitsPerSample, mDataOffset, mDataSize);
#endif
}

sp<MetaData> WAVExtractor::getTrackMetaData(
        size_t index, uint32_t flags) {
    if (mInitCheck != OK || index > 0) {
        return NULL;
    }

    return mTrackMeta;
}

status_t WAVExtractor::init() {
#if !SUPPORT_ADPCM
    uint8_t header[12];
    if (mDataSource->readAt(
                0, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return NO_INIT;
    }

    if (memcmp(header, "RIFF", 4) || memcmp(&header[8], "WAVE", 4)) {
        return NO_INIT;
    }

    size_t totalSize = U32_LE_AT(&header[4]);

    off64_t offset = 12;
    size_t remainingSize = totalSize;
    while (remainingSize >= 8) {
        uint8_t chunkHeader[8];
        if (mDataSource->readAt(offset, chunkHeader, 8) < 8) {
            return NO_INIT;
        }

        remainingSize -= 8;
        offset += 8;

        uint32_t chunkSize = U32_LE_AT(&chunkHeader[4]);

        if (chunkSize > remainingSize) {
            return NO_INIT;
        }

        if (!memcmp(chunkHeader, "fmt ", 4)) {
            if (chunkSize < 16) {
                return NO_INIT;
            }

            uint8_t formatSpec[40];
            if (mDataSource->readAt(offset, formatSpec, 2) < 2) {
                return NO_INIT;
            }

            mWaveFormat = U16_LE_AT(formatSpec);
            if (mWaveFormat != WAVE_FORMAT_PCM
                    && mWaveFormat != WAVE_FORMAT_ALAW
                    && mWaveFormat != WAVE_FORMAT_MULAW
                    && mWaveFormat != WAVE_FORMAT_EXTENSIBLE) {
                return ERROR_UNSUPPORTED;
            }

            uint8_t fmtSize = 16;
            if (mWaveFormat == WAVE_FORMAT_EXTENSIBLE) {
                fmtSize = 40;
            }
            if (mDataSource->readAt(offset, formatSpec, fmtSize) < fmtSize) {
                return NO_INIT;
            }

            mNumChannels = U16_LE_AT(&formatSpec[2]);
            if (mWaveFormat != WAVE_FORMAT_EXTENSIBLE) {
                if (mNumChannels != 1 && mNumChannels != 2) {
                    ALOGW("More than 2 channels (%d) in non-WAVE_EXT, unknown channel mask",
                            mNumChannels);
                }
            } else {
                if (mNumChannels < 1 && mNumChannels > 8) {
                    return ERROR_UNSUPPORTED;
                }
            }

            mSampleRate = U32_LE_AT(&formatSpec[4]);

            if (mSampleRate == 0) {
                return ERROR_MALFORMED;
            }

            mBitsPerSample = U16_LE_AT(&formatSpec[14]);

            if (mWaveFormat == WAVE_FORMAT_PCM
                    || mWaveFormat == WAVE_FORMAT_EXTENSIBLE) {
                if (mBitsPerSample != 8 && mBitsPerSample != 16
                    && mBitsPerSample != 24) {
                    return ERROR_UNSUPPORTED;
                }
            } else {
                CHECK(mWaveFormat == WAVE_FORMAT_MULAW
                        || mWaveFormat == WAVE_FORMAT_ALAW);
                if (mBitsPerSample != 8) {
                    return ERROR_UNSUPPORTED;
                }
            }

            if (mWaveFormat == WAVE_FORMAT_EXTENSIBLE) {
                uint16_t validBitsPerSample = U16_LE_AT(&formatSpec[18]);
                if (validBitsPerSample != mBitsPerSample) {
                    if (validBitsPerSample != 0) {
                        ALOGE("validBits(%d) != bitsPerSample(%d) are not supported",
                                validBitsPerSample, mBitsPerSample);
                        return ERROR_UNSUPPORTED;
                    } else {
                        // we only support valitBitsPerSample == bitsPerSample but some WAV_EXT
                        // writers don't correctly set the valid bits value, and leave it at 0.
                        ALOGW("WAVE_EXT has 0 valid bits per sample, ignoring");
                    }
                }

                mChannelMask = U32_LE_AT(&formatSpec[20]);
                ALOGV("numChannels=%d channelMask=0x%x", mNumChannels, mChannelMask);
                if ((mChannelMask >> 18) != 0) {
                    ALOGE("invalid channel mask 0x%x", mChannelMask);
                    return ERROR_MALFORMED;
                }

                if ((mChannelMask != CHANNEL_MASK_USE_CHANNEL_ORDER)
                        && (popcount(mChannelMask) != mNumChannels)) {
                    ALOGE("invalid number of channels (%d) in channel mask (0x%x)",
                            popcount(mChannelMask), mChannelMask);
                    return ERROR_MALFORMED;
                }

                // In a WAVE_EXT header, the first two bytes of the GUID stored at byte 24 contain
                // the sample format, using the same definitions as a regular WAV header
                mWaveFormat = U16_LE_AT(&formatSpec[24]);
                if (mWaveFormat != WAVE_FORMAT_PCM
                        && mWaveFormat != WAVE_FORMAT_ALAW
                        && mWaveFormat != WAVE_FORMAT_MULAW) {
                    return ERROR_UNSUPPORTED;
                }
                if (memcmp(&formatSpec[26], WAVEEXT_SUBFORMAT, 14)) {
                    ALOGE("unsupported GUID");
                    return ERROR_UNSUPPORTED;
                }
            }

            mValidFormat = true;
        } else if (!memcmp(chunkHeader, "data", 4)) {
            if (mValidFormat) {
                mDataOffset = offset;
                mDataSize = chunkSize;

                mTrackMeta = new MetaData;

                switch (mWaveFormat) {
                    case WAVE_FORMAT_PCM:
                        mTrackMeta->setCString(
                                kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
                        break;
                    case WAVE_FORMAT_ALAW:
                        mTrackMeta->setCString(
                                kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_G711_ALAW);
                        break;
                    default:
                        CHECK_EQ(mWaveFormat, (uint16_t)WAVE_FORMAT_MULAW);
                        mTrackMeta->setCString(
                                kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_G711_MLAW);
                        break;
                }

                mTrackMeta->setInt32(kKeyChannelCount, mNumChannels);
                mTrackMeta->setInt32(kKeyChannelMask, mChannelMask);
                mTrackMeta->setInt32(kKeySampleRate, mSampleRate);

                size_t bytesPerSample = mBitsPerSample >> 3;

                int64_t durationUs =
                    1000000LL * (mDataSize / (mNumChannels * bytesPerSample))
                        / mSampleRate;

                mTrackMeta->setInt64(kKeyDuration, durationUs);

                return OK;
            }
        }

        offset += chunkSize;
    }

    return NO_INIT;
#else
 //buffer
    uint8_t iBuffer[1024];
	uint32_t PCMBytesPresent = 0;


    // Get file size at the very first time
	if(mDataSource->getSize((off64_t *)&mDataSize)!= OK)
		return NO_INIT;


    uint32_t filepos = 0;

    // read 12 bytes of data for complete WAVE header including RIFF chunk dDescriptor

    uint32_t bytesread = 0;

	if(mDataSource->readAt(0, iBuffer, 12) < 12) {
        return NO_INIT;
    }

    // Update file position counter by 36 bytes
    filepos += 12;

    // Check for RIFF/RIFX
    uint8_t* pBuffer = &iBuffer[0];
    if (pBuffer[0] == 'R' &&
            pBuffer[1] == 'I' &&
            pBuffer[2] == 'F' &&
            pBuffer[3] == 'F')
    {
        isLittleEndian = true; // Little endian data
    }
    else if (pBuffer[0] == 'R' &&
             pBuffer[1] == 'I' &&
             pBuffer[2] == 'F' &&
             pBuffer[3] == 'X')
    {
        isLittleEndian = false; // Big endian data
    }
    else
    {
        return NO_INIT;
    }

    // Check for WAVE in Format field
    if (pBuffer[ 8] != 'W' ||
            pBuffer[ 9] != 'A' ||
            pBuffer[10] != 'V' ||
            pBuffer[11] != 'E')
    {
        return NO_INIT;
    }

    uint32_t SubChunk_Size = 0;

    bool fmtSubchunkFound = false;
    while (!fmtSubchunkFound)
    {
        // read 8 bytes from file to check for next subchunk
        bytesread = 0;
        if (mDataSource->readAt(filepos, iBuffer, 8) < 8)
        {
			return NO_INIT;
        }

        // Update file position counter by 8 bytes
        filepos += 8;
        uint8_t* pTempBuffer = &iBuffer[0];
        SubChunk_Size = (((*(pTempBuffer + 7)) << 24) | ((*(pTempBuffer + 6)) << 16) | ((*(pTempBuffer + 5)) << 8) | (*(pTempBuffer + 4)));

        // typecast filesize as uint32 - to get to this point, it MUST be
        // greater than 0.
        if ((filepos + SubChunk_Size) > mDataSize)
        {
            return NO_INIT;
        }
        // Check for FMT subchunk
        if (pTempBuffer[0] != 'f' ||
                pTempBuffer[1] != 'm' ||
                pTempBuffer[2] != 't' ||
                pTempBuffer[3] != ' ')
        {
            // "fmt " chunk not found - Unknown subchunk
            filepos += SubChunk_Size;
        }
        else
            fmtSubchunkFound = true;
    }


	//read fmt chuck data amend .by cch 090915
	//make sure the fmt chuck data is OK.
	uint32_t needbyte = (SubChunk_Size > 1024)?1024:SubChunk_Size;

	if (mDataSource->readAt(filepos, iBuffer, needbyte) < needbyte)
    {

        return NO_INIT;
    }

	filepos += SubChunk_Size;
	PCMWAVEFORMAT sWaveFormat;
	//long	read_size;

	/* clear the sWaveFormat structure */
	memset((void *)&sWaveFormat,0, sizeof(sWaveFormat));

	// Copy the wave format structure from the "fmt " chunk into a
	// local.  We do this instead of grabbing the values directly from
	// the data buffer since the "fmt " chunk can occur with any
	// alignment, making it a lot easier to simply copy the structure.
	//

	int type = (int)iBuffer[0];
	if (type == (int)WAVE_FORMAT_ADPCM)//MSADPCM
	{

		// If the length of the "fmt " chunk is not the same as the size
		// of the MS ADPCM wave format sturcture, then we can not decode
		// this file.
		//
		if (SubChunk_Size != sizeof(PCMWAVEFORMAT)-2)	//
		{
			ALOGE("--->PVWAVPARSER_READ_ERROR16");
			return NO_INIT;
		}
		memcpy((void *)&sWaveFormat,
			   iBuffer, sizeof(PCMWAVEFORMAT));

		/* sanity check */
		if ((sWaveFormat.nChannels > 2) ||
			(sWaveFormat.nBlockAlign > 4096) ||
			(sWaveFormat.wBitsPerSample != 4) ||
			(sWaveFormat.cbSize != 32) ||
			(sWaveFormat.wSamplesPerBlock > MSADPCM_MAX_PCM_LENGTH) ||
			(sWaveFormat.wNumCoef != 7))
		{
			//
			// The wave format does not pass the sanity checks, so we can
			// not decode this file.
			//
			ALOGE("--->PVWAVPARSER_READ_ERROR17");
			return NO_INIT;
		}

	}
	else if ( type == (int)WAVE_FORMAT_DVI_ADPCM)//IMAADPCM
	{
		memcpy((void *)&sWaveFormat, iBuffer, 20);

		/* sanity check */
		if ((sWaveFormat.nChannels > 2) ||
				(sWaveFormat.nBlockAlign > 4096) ||
				(sWaveFormat.cbSize != 2) ||
				(sWaveFormat.wSamplesPerBlock > IMAADPCM_MAX_PCM_LENGTH))
		{
			//
			// The wave format does not pass the sanity checks, so we can
			// not decode this file.
			//
			ALOGE("--->PVWAVPARSER_READ_ERROR18");
			return NO_INIT;
		}

		/* IMA ADPCM supports 3 or 4 bits per sample. */
		if (!( sWaveFormat.wBitsPerSample ==3 || sWaveFormat.wBitsPerSample ==4 ))
		{
			ALOGE("--->PVWAVPARSER_READ_ERROR19");
			return NO_INIT;

		}
	}
	else if ( type == WAVE_FORMAT_PCM||type == WAVE_FORMAT_ALAW ||type == WAVE_FORMAT_MULAW)//PCM
	{
		memcpy((void *)&sWaveFormat, iBuffer, SubChunk_Size);

		/* sanity check */
		if ((sWaveFormat.nChannels > 2)||(sWaveFormat.nSamplesPerSec == 0))
		{
			//
			// The wave format does not pass the sanity checks, so we can
			// not decode this file.
			//

			ALOGE("--->PVWAVPARSER_READ_ERROR20");
			return NO_INIT;
		}
	}
	else
	{
		ALOGE("--->PVWAVPARSER_READ_ERROR21 profile type %d",type);
		return NO_INIT;
		//do nothing here
	}


//--------------------------------------------------------------

	/* Save information about this file which we will need. */
	mWaveFormat = sWaveFormat.wFormatTag;
	mNumChannels = (unsigned char)sWaveFormat.nChannels;
	mSampleRate = (unsigned short)sWaveFormat.nSamplesPerSec;
	mByteRate = sWaveFormat.nAvgBytesPerSec;
	mBlockAlign = sWaveFormat.nBlockAlign;
	mBitsPerSample = (sWaveFormat.wBitsPerSample == 8)?8:16;
	mWBitsPerSample = sWaveFormat.wBitsPerSample;
	mSamplesPerBlock = sWaveFormat.wSamplesPerBlock;
	mBytesPerSample = (mBitsPerSample + 7) / 8;  // compute (ceil(BitsPerSample/8))
	switch (mSampleRate)
    {
		case 6000:
        case 8000:
        case 11025:
        case 12000:
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
            break;
        default:
			ALOGE("--->PVWAVPARSER_READ_ERROR22");
			return NO_INIT;
    }

    bool DataSubchunkFound = false;

    while (!DataSubchunkFound)
    {
        // read 8 bytes from file to check for next subchunk
        bytesread = 0;
        if (mDataSource->readAt(filepos, iBuffer, 8) < 8)
        {
			ALOGE("--->PVWAVPARSER_READ_ERROR23");
            return NO_INIT;
        }
        // Update file position counter by 8 bytes
        filepos += 8;

        uint8_t* pTempBuffer = &iBuffer[0];

        //It means that some unknown subchunk is present
        // Calculate  SubChunk Size
        SubChunk_Size = (((*(pTempBuffer + 7)) << 24) | ((*(pTempBuffer + 6)) << 16) | ((*(pTempBuffer + 5)) << 8) | (*(pTempBuffer + 4)));

        // Check for DATA subchunk ID
        if (pTempBuffer[0] != 'd' ||
                pTempBuffer[1] != 'a' ||
                pTempBuffer[2] != 't' ||
                pTempBuffer[3] != 'a')
        {
            // we need to skip this many bytes
            filepos += SubChunk_Size;
        }
        else
        {
            // data subchunk found

            // header size equals current file pos
            mDataOffset = filepos;

            //data subchunk is found
            DataSubchunkFound = true;

            // Read data SubChunk Size (is number of bytes in data or PCMBytesPresent)
            //fixed by Charles Chen .avoid the "data" length is wrong.
            PCMBytesPresent = mDataSize - mDataOffset;
			if(PCMBytesPresent > SubChunk_Size)
			{
            	PCMBytesPresent = SubChunk_Size;
				mDataSize = mDataOffset + PCMBytesPresent;//fixed the filesize ,maybe have same info at endof the file
			}

            //(this check is required to avoid memory crash if any of BytesPerSample or NumChannels is '0')
            if (mBytesPerSample && mNumChannels)
            {
				ALOGI("mBytesPerSample %d mNumChannels %d PCMBytesPresent %d offset %d size %d",
					mBytesPerSample,mNumChannels,PCMBytesPresent,mDataOffset,mDataSize);
				mNumSamples = ((PCMBytesPresent / (mBytesPerSample)) / mNumChannels);
            }
        }
    }

	 mTrackMeta = new MetaData;
	 ALOGV("----->mWaveFormat %d",mWaveFormat);
	 switch (mWaveFormat) {

	 	case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			//reset the mNumSamples value
			mNumSamples = (PCMBytesPresent/(uint32_t)mBlockAlign)*sWaveFormat.wSamplesPerBlock;//computer the totle num of samples
        case WAVE_FORMAT_PCM:
            mTrackMeta->setCString(
                    kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
            break;
        case WAVE_FORMAT_ALAW:
            mTrackMeta->setCString(
                    kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_G711_ALAW);
            break;
        default:
            CHECK_EQ(mWaveFormat, WAVE_FORMAT_MULAW);
            mTrackMeta->setCString(
                    kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_G711_MLAW);
            break;
    }

    mTrackMeta->setInt32(kKeyChannelCount, mNumChannels);
    mTrackMeta->setInt32(kKeySampleRate, mSampleRate);

	uint32_t duration_sec = mNumSamples / mSampleRate;
    uint32_t duration_msec = mNumSamples % mSampleRate;
    int64_t  duration_us = (duration_msec * 1000000LL) / mSampleRate + duration_sec * 1000000LL ;
	ALOGI("---->mNumSamples %d mSampleRate %d duration_us %lld",mNumSamples ,mSampleRate,duration_us);

    mTrackMeta->setInt64(kKeyDuration, duration_us);

    //return error if any of these value is not given in wav file header
    //(AudioFormat check is done at node level)
    if (!mNumChannels || !mNumSamples || !mSampleRate || !mBitsPerSample || !mBytesPerSample || !mByteRate)
    {
		ALOGE("--->PVWAVPARSER_READ_ERROR26");
        return NO_INIT;  //any error fom parse will be handled as PVMFFailure at corresponsding node level
    }

    return  OK;
#endif
}

const size_t WAVSource::kMaxFrameSize = 32768;
#if SUPPORT_ADPCM
WAVSource::WAVSource(
        const sp<DataSource> &dataSource,
        const sp<MetaData> &meta,
       	const sp<WAVExtractor>& extractor)
    : mDataSource(dataSource),
      mMeta(meta),
      mWaveFormat(extractor->mWaveFormat),
      mSampleRate(0),
      mNumChannels(0),
      mBitsPerSample(extractor->mBitsPerSample),
      mOffset(extractor->mDataOffset),
      mSize(extractor->mDataSize),
      mStarted(false),
      mGroup(NULL){
      pMsAdpcm = NULL;
	  pPCM = NULL;
	mByteRate = extractor->mByteRate;
    mBlockAlign =  extractor->mBlockAlign;
   	mBytesPerSample =  extractor->mBytesPerSample;
	mSamplesPerBlock = extractor->mSamplesPerBlock;
	mNumSamples = extractor->mNumSamples;
	mSampleRate = extractor->mSampleRate;
	mNumChannels = extractor->mNumChannels;
	mWBitsPerSample = extractor->mWBitsPerSample;
	isLittleEndian =  extractor->isLittleEndian;
	mOutNumSamples = 0;;//output the total num sample after each reset
	mTimeUs = 0LL;

   mMeta->setInt32(kKeySampleRate, mSampleRate);
   mMeta->setInt32(kKeyChannelCount, mNumChannels);
   mMeta->setInt32(kKeyMaxInputSize, kMaxFrameSize);
}

#else
WAVSource::WAVSource(
        const sp<DataSource> &dataSource,
        const sp<MetaData> &meta,
        uint16_t waveFormat,
        int32_t bitsPerSample,
        off64_t offset, size_t size)
    : mDataSource(dataSource),
      mMeta(meta),
      mWaveFormat(waveFormat),
      mSampleRate(0),
      mNumChannels(0),
      mBitsPerSample(bitsPerSample),
      mOffset(offset),
      mSize(size),
      mStarted(false),
      mGroup(NULL) {
    CHECK(mMeta->findInt32(kKeySampleRate, &mSampleRate));
    CHECK(mMeta->findInt32(kKeyChannelCount, &mNumChannels));

    mMeta->setInt32(kKeyMaxInputSize, kMaxFrameSize);
}
#endif
WAVSource::~WAVSource() {
    if (mStarted) {
        stop();
    }
}

status_t WAVSource::start(MetaData *params) {
    ALOGV("WAVSource::start");

    CHECK(!mStarted);

    mGroup = new MediaBufferGroup;
    mGroup->add_buffer(new MediaBuffer(kMaxFrameSize));

    if (mBitsPerSample == 8) {
        // As a temporary buffer for 8->16 bit conversion.
        mGroup->add_buffer(new MediaBuffer(kMaxFrameSize));
    }

    mCurrentPos = mOffset;
#if SUPPORT_ADPCM
	 /* Save information about this file which we will need. */
	if(mWaveFormat == WAVE_FORMAT_ADPCM|| mWaveFormat == WAVE_FORMAT_DVI_ADPCM)
	{
		pPCM = new tPCM;
		if(!pPCM)//creat failed ,return;
		{
			ALOGE("--->PVWAVPARSER_READ_ERROR27");
			return UNKNOWN_ERROR;
		}

		pPCM->usByteRate = mByteRate;
	    pPCM->usSampleRate = (unsigned short)mSampleRate;
	    pPCM->usBytesPerBlock = mBlockAlign;

	    pPCM->ucChannels = (unsigned char)mNumChannels;
	    pPCM->sPCMHeader.ucChannels = (unsigned long)mNumChannels;

	    pPCM->usSamplesPerBlock = mSamplesPerBlock;
	    pPCM->sPCMHeader.usSamplesPerBlock = (unsigned short)mSamplesPerBlock;

	    pPCM->uBitsPerSample = mWBitsPerSample;
	    pPCM->sPCMHeader.uBitsPerSample = (unsigned short)mWBitsPerSample;

	    pPCM->wFormatTag = mWaveFormat;
		pPCM->ulLength =  mSize - mOffset;//data chunk length

		if(mWaveFormat == WAVE_FORMAT_ADPCM)
		{
			if (pPCM->ulLength == 0xffffffff)
				pPCM->ulLength = 0x7fffffff;
			pMsAdpcm = new MSADPCM_PRIVATE;
			if(!pMsAdpcm)
			{
				ALOGE("--->PVWAVPARSER_READ_ERROR28");
				delete pPCM;
				pPCM = NULL;
				return UNKNOWN_ERROR;
			}
			omx_msadpcm_dec_init(pPCM, pMsAdpcm);
		}
		else
			mNumChannels = 2;//the IMA decoder,though the channel is mone or stero , the out data is stero data
	}
	mMeta->setInt32(kKeyChannelCount, mNumChannels);
#endif
    mStarted = true;

    return OK;
}

status_t WAVSource::stop() {
    ALOGV("WAVSource::stop");

    CHECK(mStarted);

    delete mGroup;
    mGroup = NULL;

    mStarted = false;
#if SUPPORT_ADPCM
	if(pPCM)
	{
		delete pPCM;
		pPCM = NULL;
	}
	if(pMsAdpcm)
	{
		delete pMsAdpcm;
		pMsAdpcm = NULL;
	}
#endif
    return OK;
}

sp<MetaData> WAVSource::getFormat() {
    ALOGV("WAVSource::getFormat");

    return mMeta;
}

status_t WAVSource::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options != NULL && options->getSeekTo(&seekTimeUs, &mode)) {
		int64_t pos = 0LL;
#if SUPPORT_ADPCM
		if(mWaveFormat == WAVE_FORMAT_ADPCM|| mWaveFormat == WAVE_FORMAT_DVI_ADPCM)
		{
			int64_t numSamples = (seekTimeUs * mSampleRate) / 1000000LL;
			int64_t blockCounter = numSamples/mSamplesPerBlock;
			blockCounter += ((numSamples%mSamplesPerBlock) > (mSamplesPerBlock >> 1))?1:0;
			pos = blockCounter * mBlockAlign;
			mOutNumSamples = 0;;//output the total num sample after each reset
			mTimeUs = seekTimeUs;
		}
		else
#endif
		pos = (seekTimeUs * mSampleRate) / 1000000 * mNumChannels * (mBitsPerSample >> 3);
        if (pos > mSize) {
            pos = mSize;
        }
        mCurrentPos = pos + mOffset;
    }

    MediaBuffer *buffer;
    status_t err = mGroup->acquire_buffer(&buffer);
    if (err != OK) {
        return err;
    }
#if SUPPORT_ADPCM
	if(mWaveFormat == WAVE_FORMAT_ADPCM || mWaveFormat == WAVE_FORMAT_DVI_ADPCM)
	{
		//whether reach end of the file
		if(mBlockAlign + mCurrentPos > mSize)
		{
			buffer->release();
			buffer = NULL;
			return ERROR_END_OF_STREAM;
		}

		ssize_t n = mDataSource->readAt(mCurrentPos, buffer->data(), mBlockAlign);
		if (n <= 0)
		{
			buffer->release();
			buffer = NULL;
			return ERROR_END_OF_STREAM;
		}
		//update the file offset
		mCurrentPos += mBlockAlign;

		ssize_t outLen = 0;
		 buffer->meta_data()->setInt64(kKeyTime,mTimeUs +( 1000000LL*mOutNumSamples /mSampleRate));
		if(mWaveFormat == WAVE_FORMAT_ADPCM)
		{
			memcpy(pMsAdpcm->block,buffer->data(),mBlockAlign);
			omx_msadpcm_decode_block(pMsAdpcm);
			outLen = pMsAdpcm->samplesperblock*2*pMsAdpcm->channels;
			memcpy(buffer->data(),pMsAdpcm->samples,outLen);
			mOutNumSamples += pMsAdpcm->samplesperblock;
		}
		else if(mWaveFormat == WAVE_FORMAT_DVI_ADPCM)
		{
			outLen = OMX_IMAADPCM_DEC((char*)buffer->data(), mBlockAlign, pPCM);
			mOutNumSamples += outLen;
			outLen = outLen *4;
			memcpy(buffer->data(), pPCM->OutputBuff, outLen);
		}
		 buffer->set_range(0, outLen);

	}
	else
	{
#endif
        size_t maxBytesToRead =
            mBitsPerSample == 8 ? kMaxFrameSize / 2 : kMaxFrameSize;
#if SUPPORT_ADPCM
    	size_t maxBytesAvailable =
    		(mCurrentPos  >= (off64_t)mSize)
    			? 0 : mSize - mCurrentPos ;

#else
    	size_t maxBytesAvailable =
    		(mCurrentPos - mOffset >= (off64_t)mSize)
    			? 0 : mSize - (mCurrentPos - mOffset);

#endif
        if (maxBytesToRead > maxBytesAvailable) {
            maxBytesToRead = maxBytesAvailable;
        }

        ssize_t n = mDataSource->readAt(
                mCurrentPos, buffer->data(),
                maxBytesToRead);

        if (n <= 0) {
            buffer->release();
            buffer = NULL;

            return ERROR_END_OF_STREAM;
        }

        mCurrentPos += n;

        buffer->set_range(0, n);

        if (mWaveFormat == WAVE_FORMAT_PCM) {
            if (mBitsPerSample == 8) {
                // Convert 8-bit unsigned samples to 16-bit signed.

                MediaBuffer *tmp;
                CHECK_EQ(mGroup->acquire_buffer(&tmp), (status_t)OK);

                // The new buffer holds the sample number of samples, but each
                // one is 2 bytes wide.
                tmp->set_range(0, 2 * n);

                int16_t *dst = (int16_t *)tmp->data();
                const uint8_t *src = (const uint8_t *)buffer->data();
                ssize_t numBytes = n;

                while (numBytes-- > 0) {
                    *dst++ = ((int16_t)(*src) - 128) * 256;
                    ++src;
                }

                buffer->release();
                buffer = tmp;
            } else if (mBitsPerSample == 24) {
                // Convert 24-bit signed samples to 16-bit signed.

                const uint8_t *src =
                    (const uint8_t *)buffer->data() + buffer->range_offset();
                int16_t *dst = (int16_t *)src;

                size_t numSamples = buffer->range_length() / 3;
                for (size_t i = 0; i < numSamples; ++i) {
                    int32_t x = (int32_t)(src[0] | src[1] << 8 | src[2] << 16);
                    x = (x << 8) >> 8;  // sign extension

                    x = x >> 8;
                    *dst++ = (int16_t)x;
                    src += 3;
                }

                buffer->set_range(buffer->range_offset(), 2 * numSamples);
            }
        }

        size_t bytesPerSample = mBitsPerSample >> 3;

        buffer->meta_data()->setInt64(
                kKeyTime,
                1000000LL * (mCurrentPos - mOffset)
                    / (mNumChannels * bytesPerSample) / mSampleRate);
#if SUPPORT_ADPCM
    }
#endif

    buffer->meta_data()->setInt32(kKeyIsSyncFrame, 1);

    *out = buffer;

    return OK;
}

////////////////////////////////////////////////////////////////////////////////

bool SniffWAV(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *) {
    char header[12];
    if (source->readAt(0, header, sizeof(header)) < (ssize_t)sizeof(header)) {
        return false;
    }

    if (memcmp(header, "RIFF", 4) || memcmp(&header[8], "WAVE", 4)) {
        return false;
    }

    sp<MediaExtractor> extractor = new WAVExtractor(source);
    if (extractor->countTracks() == 0) {
        return false;
    }

    *mimeType = MEDIA_MIMETYPE_CONTAINER_WAV;
    *confidence = WAV_CONTAINER_CONFIDENCE;

    return true;
}

}  // namespace android
