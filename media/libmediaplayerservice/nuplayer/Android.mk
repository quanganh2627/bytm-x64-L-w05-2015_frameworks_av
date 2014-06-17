LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                       \
        GenericSource.cpp               \
        HTTPLiveSource.cpp              \
        NuPlayer.cpp                    \
        NuPlayerDecoder.cpp             \
        NuPlayerDriver.cpp              \
        NuPlayerRenderer.cpp            \
        NuPlayerStreamListener.cpp      \
        RTSPSource.cpp                  \
        StreamingSource.cpp             \
        mp4/MP4Source.cpp               \

LOCAL_C_INCLUDES := \
	$(TOP)/frameworks/av/media/libstagefright/httplive            \
	$(TOP)/frameworks/av/media/libstagefright/include             \
	$(TOP)/frameworks/av/media/libstagefright/mpeg2ts             \
	$(TOP)/frameworks/av/media/libstagefright/rtsp                \
	$(TOP)/frameworks/native/include/media/openmax

ifeq ($(TARGET_HAS_VPP),true)
LOCAL_CFLAGS += -DTARGET_HAS_VPP -DGFX_BUF_EXT
LOCAL_C_INCLUDES += \
        $(TARGET_OUT_HEADERS)/libmedia_utils_vpp \
        $(TARGET_OUT_HEADERS)/libva	\
LOCAL_SHARED_LIBRARIES += libva \
                          libva-android \
                          libva-tpi \
                          libvpp_setting
ifeq ($(TARGET_VPP_USE_IVP), true)
LOCAL_LDFLAGS += -L$(TARGET_OUT_SHARED_LIBRARIES) -livp
endif
endif

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_SHARED_LIBRARIES += libmultidisplay
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
endif

#slow motion support
ifeq ($(TARGET_HAS_FRC_SLOW_MOTION), true)
    LOCAL_CFLAGS += -DTARGET_HAS_FRC_SLOW_MOTION
endif

ifeq ($(HDMI_EXTEND_MODE_VPP_FRC_ENABLE),true)
  LOCAL_CFLAGS += -DHDMI_EXTEND_MODE_VPP_FRC_ENABLE
endif

#3p
ifeq ($(TARGET_HAS_3P), true)
	LOCAL_CFLAGS += -DTARGET_HAS_3P
endif

LOCAL_SHARED_LIBRARIES += libstagefright

LOCAL_SHARED_LIBRARIES += libstagefright
LOCAL_MODULE:= libstagefright_nuplayer

LOCAL_MODULE_TAGS := eng

include $(BUILD_STATIC_LIBRARY)

