LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES := \
    ISchedulingPolicyService.cpp \
    SchedulingPolicyService.cpp

# FIXME Move this library to frameworks/native
LOCAL_MODULE := libscheduling_policy

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES:=               \
    AudioFlinger.cpp            \
    AudioMixer.cpp.arm          \
    AudioResampler.cpp.arm      \
    AudioPolicyService.cpp      \
    ServiceUtilities.cpp        \
	AudioResamplerCubic.cpp.arm \
    AudioResamplerSinc.cpp.arm

LOCAL_SRC_FILES += StateQueue.cpp

# uncomment for debugging timing problems related to StateQueue::push()
LOCAL_CFLAGS += -DSTATE_QUEUE_DUMP

LOCAL_C_INCLUDES := \
    $(call include-path-for, audio-effects) \
    $(call include-path-for, audio-utils)

# FIXME keep libmedia_native but remove libmedia after split
LOCAL_SHARED_LIBRARIES := \
    libaudioutils \
    libcommon_time_client \
    libcutils \
    libutils \
    libbinder \
    libmedia \
    libmedia_native \
    libnbaio \
    libhardware \
    libhardware_legacy \
    libeffects \
    libdl \
    libpowermanager

LOCAL_STATIC_LIBRARIES := \
    libscheduling_policy \
    libcpustats \
    libmedia_helper

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_MODULE:= libaudioflinger

LOCAL_SRC_FILES += FastMixer.cpp FastMixerState.cpp

LOCAL_CFLAGS += -DFAST_MIXER_STATISTICS

# uncomment to display CPU load adjusted for CPU frequency
# LOCAL_CFLAGS += -DCPU_FREQUENCY_STATISTICS

LOCAL_CFLAGS += -DSTATE_QUEUE_INSTANTIATIONS='"StateQueueInstantiations.cpp"'

LOCAL_CFLAGS += -UFAST_TRACKS_AT_NON_NATIVE_SAMPLE_RATE

# uncomment for systrace
# LOCAL_CFLAGS += -DATRACE_TAG=ATRACE_TAG_AUDIO

# uncomment for dumpsys to write most recent audio output to .wav file
# 47.5 seconds at 44.1 kHz, 8 megabytes
# LOCAL_CFLAGS += -DTEE_SINK_FRAMES=0x200000

# uncomment to enable the audio watchdog
# LOCAL_SRC_FILES += AudioWatchdog.cpp
# LOCAL_CFLAGS += -DAUDIO_WATCHDOG

ifeq ($(USE_INTEL_SRC),true)
  LOCAL_CFLAGS += -DUSE_INTEL_SRC
  LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libaudioresample
  LOCAL_SRC_FILES += AudioResamplerIA.cpp
  LOCAL_SHARED_LIBRARIES += libaudioresample
endif

ifeq ($(AUDIO_DUMP_ENABLE),true)
  LOCAL_C_INCLUDES += $(TOP)/frameworks/av/media/libstagefright/include
  LOCAL_STATIC_LIBRARIES += libaudiodumputil
  LOCAL_CFLAGS += -DAUDIO_DUMP_ENABLE
endif


include $(BUILD_SHARED_LIBRARY)

#
# build audio resampler test tool
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
	test-resample.cpp 			\
    AudioResampler.cpp.arm      \
	AudioResamplerCubic.cpp.arm \
    AudioResamplerSinc.cpp.arm

LOCAL_SHARED_LIBRARIES := \
	libdl \
    libcutils \
    libutils

LOCAL_MODULE:= test-resample

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)


include $(call all-makefiles-under,$(LOCAL_PATH))
