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

#include "include/AMRExtractor.h"

#if CHROMIUM_AVAILABLE
#include "include/chromium_http_stub.h"
#endif

#include "include/FragmentedMP4Extractor.h"
#include "include/MP3Extractor.h"
#include "include/MPEG4Extractor.h"
#include "include/WAVExtractor.h"
#include "include/OggExtractor.h"
#include "include/MPEG2TSExtractor.h"
#include "include/NuCachedSource2.h"
#include "include/HTTPBase.h"
#include "include/DRMExtractor.h"
#include "include/FLACExtractor.h"
#include "include/AACExtractor.h"
#include "include/WVMExtractor.h"

#include "matroska/MatroskaExtractor.h"
#include "include/ExtendedExtractor.h"
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/String8.h>

#include <cutils/properties.h>

namespace android {

bool DataSource::getUInt16(off64_t offset, uint16_t *x) {
    *x = 0;

    uint8_t byte[2];
    if (readAt(offset, byte, 2) != 2) {
        return false;
    }

    *x = (byte[0] << 8) | byte[1];

    return true;
}

status_t DataSource::getSize(off64_t *size) {
    *size = 0;

    return ERROR_UNSUPPORTED;
}

////////////////////////////////////////////////////////////////////////////////

Mutex DataSource::gSnifferMutex;
List<DataSource::SnifferFunc> DataSource::gSniffers;

bool DataSource::sniff(
        String8 *mimeType, float *confidence, sp<AMessage> *meta) {
    *confidence = 0.0f;
    meta->clear();
    String8 newMimeType;
    float newConfidence;
    sp<AMessage> newMeta;

    Mutex::Autolock autoLock(gSnifferMutex);
    if (gSniffers.empty() == true) {
       ALOGI("have not register any extractor now, register.");
       gSnifferMutex.unlock();
       RegisterDefaultSniffers();
       gSnifferMutex.lock();
    }

    if ((mimeType == NULL) || (mimeType->string() == NULL)) {
        for (List<SnifferFunc>::iterator it = gSniffers.begin();
             it != gSniffers.end(); ++it) {

            if ((*it)(this, &newMimeType, &newConfidence, &newMeta)) {
                if (newConfidence > *confidence) {
                    *mimeType = newMimeType;
                    *confidence = newConfidence;
                    *meta = newMeta;
                }
            }
        }
    } else {
        const char* mime = mimeType->string();
        SnifferFunc  func[10];
        int func_count =0;
        char mime_lower[5] = {'\0'};
		/*@jh: convert possible BIG character to SMALL one*/
		for (int id = 0; id < 5; id++)
		{
			if ((mime[id] >= 'A') && (mime[id] <= 'Z'))
				mime_lower[id] = mime[id] - 'A' + 'a';
			else
				mime_lower[id] = mime[id];
		}

		//LOGE("mime_lowerType->string()=%c%c%c%c%c",mime_lower[0],mime_lower[1],mime_lower[2],mime_lower[3],mime_lower[4]);
 		if(mime_lower[0] =='m' && mime_lower[1]=='p' && mime_lower[2]=='3' && mime_lower[3]=='\0')
		{
		 	func[0] = SniffMP3;
			func_count = 1;

		}
		else if(mime_lower[0] =='w' && mime_lower[1]=='a' && mime_lower[2]=='v' && mime_lower[3]=='\0')
		{
			func[0] = SniffWAV;
			func_count = 1;

		}
		else if(mime_lower[0] =='o' && mime_lower[1]=='g' && mime_lower[2]=='g' && mime_lower[3]=='\0')
		{
			func[0] = SniffOgg;
			func_count = 1;

		}
		else if(mime_lower[0] =='m' && mime_lower[1]=='k' && mime_lower[2]=='v' && mime_lower[3]=='\0')
		{
			func[0] = SniffMatroska;
			func_count = 1;

		}
		else if((mime_lower[0] =='m' && mime_lower[1]=='p' && mime_lower[2]=='4' && mime_lower[3]=='\0' )
				||(mime_lower[0] =='m' && mime_lower[1]=='o' && mime_lower[2]=='v' && mime_lower[3]=='\0' )
				||(mime_lower[0] =='3' && mime_lower[1]=='g' && mime_lower[2]=='p' && mime_lower[3]=='\0' ))
		{
			func[0] = SniffMPEG4;

			func_count = 1;

		}
		else if((mime_lower[0] =='t' && mime_lower[1]=='s' && mime_lower[2]=='\0')
				||(mime_lower[0] == 't' && mime_lower[1] == 'p' && mime_lower[2] == '\0')
				||(mime_lower[0] == 't' && mime_lower[1] == 'r' && mime_lower[2] == 'p' && mime_lower[3] == '\0')
				||(mime_lower[0] == 'm' && mime_lower[1] == '2' && mime_lower[2] == 't' && mime_lower[3] == 's' && mime_lower[4] == '\0'))
		{
			func[0] = SniffMPEG2TS;
			func_count = 1;

		}
		else if(mime_lower[0] =='f' && mime_lower[1]=='l' && mime_lower[2]=='a' && mime_lower[3]=='c' &&mime_lower[4]=='\0')
		{
			ALOGE("herer \n");
			func[0] = SniffFLAC;
			func_count = 1;
		}
		else
		{
			;
		}
		//gettimeofday(&timeFirst, NULL);
		for(int i = 0;i < func_count;i++)
		{
			if(func[i](this, &newMimeType, &newConfidence, &newMeta))
			{
				if (newConfidence > *confidence) {
	                *mimeType = newMimeType;
	                *confidence = newConfidence;
	                *meta = newMeta;
	            }
			}
		}
		//gettimeofday(&timeSec, NULL);
		//LOGE("func time  is  %d\n", (timeSec.tv_sec - timeFirst.tv_sec) * 1000 + (timeSec.tv_usec - timeFirst.tv_usec) / 1000);

		if(*confidence == 0)
		{
			for (List<SnifferFunc>::iterator it = gSniffers.begin();
	         it != gSniffers.end(); ++it) {

	        	if ((*it)(this, &newMimeType, &newConfidence, &newMeta)) {
	            	if (newConfidence > *confidence) {
	                	*mimeType = newMimeType;
	                	*confidence = newConfidence;
	                	*meta = newMeta;
	            	}
	        	}
	    	}
		}
    }

    return *confidence > 0.0;
}

// static
void DataSource::RegisterSniffer(SnifferFunc func) {
    Mutex::Autolock autoLock(gSnifferMutex);

    for (List<SnifferFunc>::iterator it = gSniffers.begin();
         it != gSniffers.end(); ++it) {
        if (*it == func) {
            return;
        }
    }

    gSniffers.push_back(func);
}

// static
void DataSource::RegisterDefaultSniffers() {
    RegisterSniffer(SniffMPEG4);
    RegisterSniffer(SniffFragmentedMP4);
    RegisterSniffer(SniffMatroska);
    RegisterSniffer(SniffOgg);
    RegisterSniffer(SniffWAV);
    RegisterSniffer(SniffFLAC);
    RegisterSniffer(SniffAMR);
    RegisterSniffer(SniffMPEG2TS);
    RegisterSniffer(SniffMP3);
    RegisterSniffer(SniffAAC);
    RegisterSniffer(SniffWAV);
    ExtendedExtractor::RegisterSniffers();
    RegisterSniffer(SniffWVM);

    char value[PROPERTY_VALUE_MAX];
    if (property_get("drm.service.enabled", value, NULL)
            && (!strcmp(value, "1") || !strcasecmp(value, "true"))) {
        RegisterSniffer(SniffDRM);
    }
}

// static
sp<DataSource> DataSource::CreateFromURI(
        const char *uri, const KeyedVector<String8, String8> *headers) {
    bool isWidevine = !strncasecmp("widevine://", uri, 11);

    sp<DataSource> source;
    if (!strncasecmp("file://", uri, 7)) {
        source = new FileSource(uri + 7);
    } else if (!strncasecmp("http://", uri, 7)
            || !strncasecmp("https://", uri, 8)
            || isWidevine) {
        sp<HTTPBase> httpSource = HTTPBase::Create();

        String8 tmp;
        if (isWidevine) {
            tmp = String8("http://");
            tmp.append(uri + 11);

            uri = tmp.string();
        }

        if (httpSource->connect(uri, headers) != OK) {
            return NULL;
        }

        if (!isWidevine) {
            String8 cacheConfig;
            bool disconnectAtHighwatermark;
            if (headers != NULL) {
                KeyedVector<String8, String8> copy = *headers;
                NuCachedSource2::RemoveCacheSpecificHeaders(
                        &copy, &cacheConfig, &disconnectAtHighwatermark);
            }

            source = new NuCachedSource2(
                    httpSource,
                    cacheConfig.isEmpty() ? NULL : cacheConfig.string());
        } else {
            // We do not want that prefetching, caching, datasource wrapper
            // in the widevine:// case.
            source = httpSource;
        }

# if CHROMIUM_AVAILABLE
    } else if (!strncasecmp("data:", uri, 5)) {
        source = createDataUriSource(uri);
#endif
    } else {
        // Assume it's a filename.
        source = new FileSource(uri);
    }

    if (source == NULL || source->initCheck() != OK) {
        return NULL;
    }

    return source;
}

String8 DataSource::getMIMEType() const {
    return String8("application/octet-stream");
}

}  // namespace android
