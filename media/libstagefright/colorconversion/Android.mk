LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                     \
        ColorConverter.cpp            \
        SoftwareRenderer.cpp

LOCAL_C_INCLUDES := \
        $(TOP)/frameworks/native/include/media/openmax \
        $(TOP)/hardware/msm7k

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
endif
ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP -DGFX_BUF_EXT
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmedia_utils_vpp \
        $(TARGET_OUT_HEADERS)/libva
endif

LOCAL_MODULE:= libstagefright_color_conversion

include $(BUILD_STATIC_LIBRARY)
