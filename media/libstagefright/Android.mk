LOCAL_PATH:= $(call my-dir)
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
        ClockEstimator.cpp                \
        CodecBase.cpp                     \
        DataSource.cpp                    \
        DataURISource.cpp                 \
        DRMExtractor.cpp                  \
        ESDS.cpp                          \
        FileSource.cpp                    \
        FLACExtractor.cpp                 \
        HTTPBase.cpp                      \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        AVIExtractor.cpp                  \
        MPEG4Writer.cpp                   \
        MediaAdapter.cpp                  \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaCodecSource.cpp              \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        http/MediaHTTP.cpp                \
        MediaMuxer.cpp                    \
        MediaSource.cpp                   \
        MetaData.cpp                      \
        NuCachedSource2.cpp               \
        NuMediaExtractor.cpp              \
        OMXClient.cpp                     \
        OMXCodec.cpp                      \
        OggExtractor.cpp                  \
        SampleIterator.cpp                \
        SampleTable.cpp                   \
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
        avc_utils.cpp

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/ \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/openssl/include \
        $(TOP)/external/libvpx/libwebm \
        $(TOP)/system/netd/include \
        $(TOP)/external/icu/icu4c/source/common \
        $(TOP)/external/icu/icu4c/source/i18n \

LOCAL_SHARED_LIBRARIES := \
        libbinder \
        libcamera_client \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libmedia \
        libnetd_client \
        libopus \
        libsonivox \
        libssl \
        libstagefright_omx \
        libstagefright_yuv \
        libsync \
        libui \
        libutils \
        libvorbisidec \
        libz \
        libpowermanager

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
        libstagefright_aacenc \
        libstagefright_matroska \
        libstagefright_webm \
        libstagefright_timedtext \
        libvpx \
        libwebm \
        libstagefright_mpeg2ts \
        libstagefright_id3 \
        libFLAC \
        libmedia_helper

ifeq ($(ENABLE_BACKGROUND_MUSIC),true)
  LOCAL_CFLAGS += -DBGM_ENABLED
endif

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl

ifeq ($(USE_INTEL_ASF_EXTRACTOR),true)
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmix_asf_extractor
LOCAL_STATIC_LIBRARIES += libasfextractor
LOCAL_SHARED_LIBRARIES += libasfparser
LOCAL_CPPFLAGS += -DUSE_INTEL_ASF_EXTRACTOR
endif

ifeq ($(USE_INTEL_MULT_THREAD),true)
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libthreadedsource
LOCAL_STATIC_LIBRARIES += libthreadedsource
LOCAL_CPPFLAGS += -DUSE_INTEL_MULT_THREAD
endif

ifeq ($(USE_INTEL_MDP),true)
LOCAL_C_INCLUDES += \
          $(TARGET_OUT_HEADERS)/media_codecs

LOCAL_STATIC_LIBRARIES += \
          lib_stagefright_mdp_wmadec \
          libmc_wma_dec \
          libmc_umc_core_merged \
          libmc_codec_common

LOCAL_CPPFLAGS += -DUSE_INTEL_MDP

ifeq ($(TARGET_ARCH), x86)
ifeq ($(TARGET_BOARD_PLATFORM), clovertrail)
    IPP_LIB_EXT := ia32
    IPP_LIB_ARCH := s_s8
else
    IPP_LIB_EXT := ia32
    IPP_LIB_ARCH := s_p8
endif
else
    IPP_LIB_EXT := intel64
    IPP_LIB_ARCH := s_e9
endif

IPP_LIBS := \
    -L$(TOP)/vendor/intel/PRIVATE/media_codecs/codecs/core/ipp/lib/$(IPP_LIB_EXT)/ \
    -Xlinker --start-group \
    -lippcore \
    -lippac_$(IPP_LIB_ARCH) \
    -lippvc_$(IPP_LIB_ARCH) \
    -lippdc_$(IPP_LIB_ARCH) \
    -lippcc_$(IPP_LIB_ARCH) \
    -lipps_$(IPP_LIB_ARCH) \
    -lippsc_$(IPP_LIB_ARCH) \
    -lippi_$(IPP_LIB_ARCH) \
    -lippvm \
    -Xlinker --end-group \
    -lsvml \
    -limf \
    -lirc

LOCAL_LDFLAGS += \
         -Wl,--no-warn-shared-textrel \
         $(IPP_LIBS) \

endif

LOCAL_CFLAGS += -Wno-multichar

ifeq ($(TARGET_HAS_ISV), true)
LOCAL_CFLAGS +=-DTARGET_HAS_ISV
endif

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
    LOCAL_STATIC_LIBRARIES += libmultidisplayvideoclient
    LOCAL_SHARED_LIBRARIES += libmultidisplay
endif

ifeq ($(USE_FEATURE_ALAC),true)
LOCAL_CPPFLAGS += -DUSE_FEATURE_ALAC
LOCAL_C_INCLUDES += \
        $(TOP)/frameworks/av/media/libstagefright/omx
endif

LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
