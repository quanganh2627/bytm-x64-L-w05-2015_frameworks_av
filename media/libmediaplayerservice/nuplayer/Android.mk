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

ifeq ($(TARGET_HAS_MULTIPLE_DISPLAY),true)
    LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/display
    LOCAL_SHARED_LIBRARIES += libmultidisplay
    LOCAL_CFLAGS += -DTARGET_HAS_MULTIPLE_DISPLAY
endif

LOCAL_SHARED_LIBRARIES += libstagefright
LOCAL_MODULE:= libstagefright_nuplayer

LOCAL_MODULE_TAGS := eng

include $(BUILD_STATIC_LIBRARY)

