LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(strip $(USE_INTEL_LVSE)),true)
LOCAL_CFLAGS += -DLVSE
endif

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
        AVIExtractor.cpp                  \
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
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        MPEG4Writer.cpp                   \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        MediaSource.cpp                   \
        AsyncOMXCodecWrapper.cpp          \
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
        avc_utils.cpp                     \
        mp4/FragmentedMP4Parser.cpp       \
        mp4/TrackFragment.cpp             \
        ThreadedSource.cpp                \

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/stagefright/timedtext \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/openssl/include \
        $(TOP)/frameworks/av/media/libstagefright/omx \

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
        libz \

LOCAL_STATIC_LIBRARIES := \
        libstagefright_color_conversion \
        libstagefright_aacenc \
        libstagefright_matroska \
        libstagefright_timedtext \
        libvpx \
        libstagefright_mpeg2ts \
        libstagefright_httplive \
        libstagefright_id3 \
        libFLAC \
        libmedia_helper \

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP -DGFX_BUF_EXT
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmedia_utils_vpp \
        $(TARGET_OUT_HEADERS)/libva
LOCAL_STATIC_LIBRARIES += libvpp
LOCAL_SHARED_LIBRARIES += libva \
                          libva-android \
                          libva-tpi
endif

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

ifeq ($(ENABLE_BACKGROUND_MUSIC),true)
  LOCAL_CFLAGS += -DBGM_ENABLED
endif

ifeq ($(USE_INTEL_MDP),true)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/media_codecs

LOCAL_STATIC_LIBRARIES += \
        lib_stagefright_mdp_vp8dec \
        libmc_vp8_dec \
        libmc_codec_common \
        libmc_core
LOCAL_CPPFLAGS += -DUSE_INTEL_MDP
endif

ifeq ($(USE_INTEL_VA),true)
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libvavideodecoder
LOCAL_STATIC_LIBRARIES += libvavideodecoder
LOCAL_SHARED_LIBRARIES += libva \
                          libva-android \
                          libva-tpi \
                          libva_videodecoder \
                          libmixvbp
LOCAL_CPPFLAGS += -DUSE_INTEL_VA
endif

LOCAL_SRC_FILES += \
        chromium_http_stub.cpp
LOCAL_CPPFLAGS += -DCHROMIUM_AVAILABLE=1

ifeq ($(USE_INTEL_ASF_EXTRACTOR),true)

LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmix_asf_extractor

LOCAL_STATIC_LIBRARIES += libasfextractor
LOCAL_SHARED_LIBRARIES += libasfparser
LOCAL_CPPFLAGS += -DUSE_INTEL_ASF_EXTRACTOR
endif

ifeq ($(USE_INTEL_MDP),true)
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/media_codecs

LOCAL_STATIC_LIBRARIES += \
        lib_stagefright_mdp_mp3dec \
        libmc_mp3_dec \
        lib_stagefright_mdp_aacdec \
        libmc_aac_dec \
        lib_stagefright_mdp_aacenc \
        libmc_aac_enc \
        lib_stagefright_mdp_amrnbdec \
        libmc_gsmamr \
        lib_stagefright_mdp_amrnbenc \
        lib_stagefright_mdp_amrwbdec \
        lib_stagefright_mdp_amrwbenc \
        libmc_amrwb \
        libmc_amrcommon \
        lib_stagefright_mdp_vorbisdec \
        libmc_vorbis_dec \
        libmc_codec_common \
        libmc_core \
        lib_stagefright_mdp_wmadec \
        libmc_wma_dec \


LOCAL_CPPFLAGS += -DUSE_INTEL_MDP
endif

LOCAL_SHARED_LIBRARIES += libstlport
include external/stlport/libstlport.mk

LOCAL_SHARED_LIBRARIES += \
        libstagefright_enc_common \
        libstagefright_avc_common \
        libstagefright_foundation \
        libdl

LOCAL_CFLAGS += -Wno-multichar

ifeq ($(strip $(USE_INTEL_LVSE)),true)

LOCAL_STATIC_LIBRARIES += \
        libmusicbundle \
        libLVAudioSource \

endif

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/display
    LOCAL_SHARED_LIBRARIES += libmultidisplay
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
endif

LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
