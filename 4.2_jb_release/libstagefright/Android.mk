LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                         \
        version.cpp

$(SHELL $(LOCAL_PATH)/version.sh)

LOCAL_CFLAGS += -Wno-multichar
LOCAL_MODULE:= libstagefright_version

include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
include frameworks/av/media/libstagefright/codecs/common/Config.mk

LOCAL_SRC_FILES:=                         \
        ACodec.cpp                        \
        AACExtractor.cpp                  \
        AACWriter.cpp                     \
        AMRExtractor.cpp                  \
        AMRWriter.cpp                     \
        AudioPlayer.cpp                   \
        AudioSource.cpp                   \
        AwesomePlayer.cpp                 \
        CameraSource.cpp                  \
        CameraSourceTimeLapse.cpp         \
        DataSource.cpp                    \
        DRMExtractor.cpp                  \
        ESDS.cpp                          \
        FileSource.cpp                    \
        FLACExtractor.cpp                 \
        FragmentedMP4Extractor.cpp        \
        HTTPBase.cpp                      \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MIRRORINGWriter.cpp		  \
        MPEG2TSWriter.cpp                 \
        MPEG4Writer.cpp                   \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        ExtendedExtractor.cpp							\
        MediaSource.cpp                   \
        MetaData.cpp                      \
        NuCachedSource2.cpp               \
        NuMediaExtractor.cpp              \
        OMXClient.cpp                     \
        OMXCodec.cpp                      \
        OggExtractor.cpp                  \
        SkipCutBuffer.cpp                 \
        StagefrightMediaScanner.cpp       \
        StagefrightMetadataRetriever.cpp  \
        SurfaceMediaSource.cpp            \
        ThrottledSource.cpp               \
        TimeSource.cpp                    \
        TimedEventQueue.cpp               \
        Utils.cpp                         \
        VBRISeeker.cpp                    \
        WAVExtractor.cpp                  \
        WVMExtractor.cpp                  \
        XINGSeeker.cpp                    \
        avc_utils.cpp                     \
        mp4/FragmentedMP4Parser.cpp       \
        mp4/TrackFragment.cpp             \
	get_ape_id3.cpp                   \
        get_flac_id3.cpp                  \
        ApeGetFileInfo.cpp                \
	Audio_Mirror_Source.cpp		  \
	Video_Mirror_Source.cpp		  

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/expat/lib \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
	$(TOP)/frameworks/av/include/media/stagefright \
        $(TOP)/frameworks/av/media/libstagefright/rtsp \
        $(TOP)/frameworks/av/media/libstagefright/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common/include \
        $(TOP)/frameworks/av/media/libstagefright/libvpu/common \
        $(TOP)/frameworks/av/media/libstagefright/ffmpg/include \
        $(TOP)/external/openssl/include \
        $(TOP)/hardware/rk29/libyuvtorgb \
	$(TOP)/frameworks/av/media/libstagefright/libvpu/common/include

LOCAL_SHARED_LIBRARIES := \
        libbinder \
        libcamera_client \
        libcrypto \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libmedia \
        libmedia_native \
        libsonivox \
        libssl \
        libstagefright_omx \
        libstagefright_yuv \
        libsync \
        libui \
        libutils \
        libvorbisidec \
        librk_on2     \
        libz \

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
        libstagefright_aacdec \
        libstagefright_aacenc \
	libstagefright_wmaprodec \
        libstagefright_flacdec\
        libstagefright_avcdec \
        libstagefright_avcdec_flash\
        libstagefright_avcenc \
        libstagefright_rvdec \
        libstagefright_m2vdec \
        libstagefright_vc1dec \
        libstagefright_flvdec \
        libstagefright_m4vh263dec \
        libstagefright_mp3dec \
        libstagefright_timedtext \
        libstagefright_vpxdec \
        libvpx \
        libstagefright_id3 \
        libFLAC \
        libstagefright_version
	
LOCAL_LDFLAGS :=  \
	$(LOCAL_PATH)/matroska/libstagefright_matroska.a \
	$(LOCAL_PATH)/mpeg2ts/libstagefright_mpeg2ts.a \
	$(LOCAL_PATH)/httplive/libstagefright_httplive.a \
        $(LOCAL_PATH)/cmov/libstagefright_cmov.a \
	$(LOCAL_PATH)/ffmpg/libstagefright_ffmpg.a \
	$(LOCAL_PATH)/libstagefright_framemanage.a \
	$(LOCAL_PATH)/mpeg4/libstagefright_mp4.a \
	$(LOCAL_PATH)/wimoVer1/libstagefright_wimover1.a \
	$(LOCAL_PATH)/mirroring/libstagefright_mirroring.a \
	$(LOCAL_PATH)/tvpad_decoder/libtvpad_decoder.a \
	$(LOCAL_PATH)/codecs/aacdec_mirroring/libstagefright_aacdec_mirroring.a \
	$(LOCAL_PATH)/codecs/ac3dec/libstagefright_ac3dec.a \
	$(LOCAL_PATH)/codecs/radec/libstagefright_radec.a \
	$(LOCAL_PATH)/codecs/dtsdec/libstagefright_dtsdec.a \
	$(LOCAL_PATH)/codecs/wavdec/libstagefright_wavdec.a \
	$(LOCAL_PATH)/codecs/wmadec/libstagefright_wmadec.a

LOCAL_SRC_FILES += \
        chromium_http_stub.cpp
LOCAL_CPPFLAGS += -DCHROMIUM_AVAILABLE=1

LOCAL_SHARED_LIBRARIES += libstlport
include external/stlport/libstlport.mk

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl\
        libvpu

LOCAL_CFLAGS += -Wno-multichar

LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
