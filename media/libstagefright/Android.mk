#
# This file was modified by Dolby Laboratories, Inc. The portions of the
# code that are surrounded by "DOLBY..." are copyrighted and
# licensed separately, as follows:
#
#  (C) 2012-2013 Dolby Laboratories, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Build AudioDumpUtil
#########################
ifeq ($(AUDIO_DUMP_ENABLE),true)
LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/include \

LOCAL_SRC_FILES:= \
    AudioDumpUtils.cpp \

LOCAL_MODULE:= libaudiodumputil

include $(BUILD_STATIC_LIBRARY)
endif
##########################


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
        HTTPBase.cpp                      \
        JPEGSource.cpp                    \
        MP3Extractor.cpp                  \
        MPEG2TSWriter.cpp                 \
        MPEG4Extractor.cpp                \
        MPEG4Writer.cpp                   \
        MediaAdapter.cpp                  \
        MediaBuffer.cpp                   \
        MediaBufferGroup.cpp              \
        MediaCodec.cpp                    \
        MediaCodecList.cpp                \
        MediaDefs.cpp                     \
        MediaExtractor.cpp                \
        MediaMuxer.cpp                    \
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
        $(TOP)/frameworks/native/services/connectivitymanager \
        $(TOP)/external/flac/include \
        $(TOP)/external/tremolo \
        $(TOP)/external/openssl/include \
        $(TOP)/frameworks/av/media/libstagefright/omx \

LOCAL_SHARED_LIBRARIES := \
        libbinder \
        libcamera_client \
        libconnectivitymanager \
        libcutils \
        libdl \
        libdrmframework \
        libexpat \
        libgui \
        libicui18n \
        libicuuc \
        liblog \
        libmedia \
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
        libstagefright_timedtext \
        libvpx \
        libwebm \
        libstagefright_mpeg2ts \
        libstagefright_id3 \
        libFLAC \
        libmedia_helper

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP -DGFX_BUF_EXT
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmedia_utils_vpp \
        $(TARGET_OUT_HEADERS)/libva
LOCAL_STATIC_LIBRARIES += libvpp
LOCAL_SHARED_LIBRARIES += libva \
                          libva-android \
                          libva-tpi \
                          libvpp_setting
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
          libmc_wma_dec


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

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
ifeq ($(USE_MDS_LEGACY),true)
    LOCAL_CFLAGS += -DUSE_MDS_LEGACY
endif
    LOCAL_SHARED_LIBRARIES += libmultidisplay
endif

ifeq ($(AUDIO_DUMP_ENABLE),true)
  LOCAL_C_INCLUDES += $(TOP)/frameworks/av/media/libstagefright/include
  LOCAL_STATIC_LIBRARIES += libaudiodumputil
  LOCAL_CFLAGS += -DAUDIO_DUMP_ENABLE
endif

ifdef DOLBY_UDC
  LOCAL_CFLAGS += -DDOLBY_UDC
endif #DOLBY_UDC
ifdef DOLBY_DAP
    ifdef DOLBY_DAP_OPENSLES
        LOCAL_CFLAGS += -DDOLBY_DAP_OPENSLES
    endif
endif #DOLBY_END
LOCAL_MODULE:= libstagefright

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
