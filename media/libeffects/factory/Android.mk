LOCAL_PATH:= $(call my-dir)

# Effect factory library

ifeq ($(strip $(USE_INTEL_LVSE)),true)
LOCAL_CFLAGS += -DLVSE
endif

include $(CLEAR_VARS)

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
