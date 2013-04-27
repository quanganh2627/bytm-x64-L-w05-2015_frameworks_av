LOCAL_PATH:= $(call my-dir)

# Build for helper target
#########################

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES:= \
    AudioParameter.cpp
LOCAL_MODULE:= libmedia_helper
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

# Build for helper host test
############################

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES:= \
    AudioParameter.cpp
LOCAL_MODULE:= libmedia_helper_host
LOCAL_MODULE_TAGS := optional

include $(BUILD_HOST_STATIC_LIBRARY)

# Build for main lib target
###########################

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES:= \
    AudioTrack.cpp \
ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true) \
    AudioTrackOffload.cpp \
endif \
    IAudioFlinger.cpp \
    IAudioFlingerClient.cpp \
    IAudioTrack.cpp \
    IAudioRecord.cpp \
    ICrypto.cpp \
    IHDCP.cpp \
    AudioRecord.cpp \
    AudioSystem.cpp \
    mediaplayer.cpp \
    IMediaPlayerService.cpp \
    IMediaPlayerClient.cpp \
    IMediaRecorderClient.cpp \
    IMediaPlayer.cpp \
    IMediaRecorder.cpp \
    IRemoteDisplay.cpp \
    IRemoteDisplayClient.cpp \
    IStreamSource.cpp \
    Metadata.cpp \
    mediarecorder.cpp \
    IMediaMetadataRetriever.cpp \
    mediametadataretriever.cpp \
    ToneGenerator.cpp \
    JetPlayer.cpp \
    IOMX.cpp \
    IAudioPolicyService.cpp \
    MediaScanner.cpp \
    MediaScannerClient.cpp \
    autodetect.cpp \
    IMediaDeathNotifier.cpp \
    MediaProfiles.cpp \
    IEffect.cpp \
    IEffectClient.cpp \
    AudioEffect.cpp \
    Visualizer.cpp \
    MemoryLeakTrackUtil.cpp \
    SoundPool.cpp \
    SoundPoolThread.cpp

LOCAL_SHARED_LIBRARIES := \
    libui libcutils libutils libbinder libsonivox libicuuc libexpat \
    libcamera_client libstagefright_foundation \
    libgui libdl libaudioutils libmedia_native

LOCAL_WHOLE_STATIC_LIBRARY := libmedia_helper

LOCAL_MODULE:= libmedia

LOCAL_C_INCLUDES := \
    $(call include-path-for, graphics corecg) \
    $(TOP)/frameworks/native/include/media/openmax \
    external/icu4c/common \
    $(call include-path-for, audio-effects) \
    $(call include-path-for, audio-utils)

ifeq ($(USE_INTEL_SRC), true)
  LOCAL_CFLAGS += -DUSE_INTEL_SRC
  LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libaudioresample
  LOCAL_SHARED_LIBRARIES += libaudioresample
endif

include $(BUILD_SHARED_LIBRARY)
