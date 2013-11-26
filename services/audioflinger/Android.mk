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

LOCAL_SRC_FILES := \
    ISchedulingPolicyService.cpp \
    SchedulingPolicyService.cpp

# FIXME Move this library to frameworks/native
LOCAL_MODULE := libscheduling_policy

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
    AudioFlinger.cpp            \
    Threads.cpp                 \
    Tracks.cpp                  \
    Effects.cpp                 \
    AudioMixer.cpp.arm          \
    AudioResampler.cpp.arm      \
    AudioPolicyService.cpp      \
    ServiceUtilities.cpp        \
    AudioResamplerCubic.cpp.arm \
    AudioResamplerSinc.cpp.arm

LOCAL_SRC_FILES += StateQueue.cpp

LOCAL_C_INCLUDES := \
    $(call include-path-for, audio-effects) \
    $(call include-path-for, audio-utils)

LOCAL_SHARED_LIBRARIES := \
    libaudioutils \
    libcommon_time_client \
    libcutils \
    libutils \
    liblog \
    libbinder \
    libmedia \
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

LOCAL_MODULE:= libaudioflinger

LOCAL_SRC_FILES += FastMixer.cpp FastMixerState.cpp AudioWatchdog.cpp

LOCAL_CFLAGS += -DSTATE_QUEUE_INSTANTIATIONS='"StateQueueInstantiations.cpp"'

# Define ANDROID_SMP appropriately. Used to get inline tracing fast-path.
ifeq ($(TARGET_CPU_SMP),true)
    LOCAL_CFLAGS += -DANDROID_SMP=1
else
    LOCAL_CFLAGS += -DANDROID_SMP=0
endif

LOCAL_CFLAGS += -fvisibility=hidden

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

ifdef DOLBY_DAP
    # DAP log level for AudioFlinger service: 0=1=Off, 2=Intense, 3=Verbose
    LOCAL_CFLAGS += -DDOLBY_DAP_LOG_LEVEL_AUDIOFLINGER=3
    # DAP compilation switch to suspend DAP if system sound is present
ifdef DOLBY_DAP_BYPASS_SOUND_TYPES
    LOCAL_CFLAGS += -DDOLBY_DAP_BYPASS_SOUND_TYPES
endif
ifdef DOLBY_DAP_OPENSLES
    LOCAL_CFLAGS += -DDOLBY_DAP_OPENSLES
    # DAP compilation switch for applying the pregain
    # Note: Keep this definition consistent with Android.mk in DS effect
    LOCAL_CFLAGS += -DDOLBY_DAP_OPENSLES_PREGAIN
    LOCAL_C_INCLUDES += $(TOP)/vendor/intel/PRIVATE/dolby_ds1/libds/include
    LOCAL_C_INCLUDES += $(TOP)/frameworks/av/include/media

ifdef DOLBY_DAP_OPENSLES_LPA
    LOCAL_CFLAGS += -DDOLBY_DAP_OPENSLES_LPA
endif

ifdef DOLBY_DAP_OPENSLES_LPA_TEST
    LOCAL_CFLAGS += -DDOLBY_DAP_OPENSLES_LPA_TEST
endif

endif
endif # DOLBY_DAP

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
    libutils \
    liblog

LOCAL_MODULE:= test-resample

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
