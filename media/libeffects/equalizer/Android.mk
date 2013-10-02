LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
ifeq ($(strip $(INTEL_MUSIC_OFFLOAD_FEATURE)),true)
LOCAL_PREBUILT_LIBS := lib/libgeq.so
include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE:= libswwrapper
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/soundfx
LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_SRC_FILES := \
       EffectEQ.cpp

LOCAL_SHARED_LIBRARIES := libcutils libdl libeffects libgeq
LOCAL_C_INCLUDES := \
        $(call include-path-for, audio-effects) \
        $(LOCAL_PATH)/inc \
        $(TOP)/vendor/intel/hardware/libcodec_offload/hweffect/include

include $(BUILD_SHARED_LIBRARY)
endif
