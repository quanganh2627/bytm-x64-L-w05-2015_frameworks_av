LOCAL_PATH:= $(call my-dir)

# Effect factory library
include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
  LOCAL_CFLAGS += -DINTEL_MUSIC_OFFLOAD_FEATURE
endif

LOCAL_SRC_FILES:= \
	EffectsFactory.c

LOCAL_SHARED_LIBRARIES := \
	libcutils liblog

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE:= libeffects

LOCAL_SHARED_LIBRARIES += libdl

LOCAL_C_INCLUDES := \
    $(call include-path-for, audio-effects)

include $(BUILD_SHARED_LIBRARY)
