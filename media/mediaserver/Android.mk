LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main_mediaserver.cpp 

LOCAL_SHARED_LIBRARIES := \
	libaudioflinger \
	libcameraservice \
	libmediaplayerservice \
	libutils \
	libbinder

# FIXME The duplicate audioflinger is temporary
LOCAL_C_INCLUDES := \
    frameworks/av/media/libmediaplayerservice \
    frameworks/av/services/audioflinger \
    frameworks/av/services/camera/libcameraservice \
    frameworks/native/services/audioflinger

LOCAL_MODULE:= mediaserver

include $(BUILD_EXECUTABLE)
