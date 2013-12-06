LOCAL_PATH:= $(call my-dir)

#
# libcameraservice
#

include $(CLEAR_VARS)

ifeq ($(strip $(INTEL_FEATURE_ASF)),true)
  LOCAL_CPPFLAGS += -DPLATFORM_ASF_VERSION=$(PLATFORM_ASF_VERSION)
else
  LOCAL_CPPFLAGS += -DPLATFORM_ASF_VERSION=0
endif

LOCAL_SRC_FILES:=               \
    CameraService.cpp \
    CameraDeviceFactory.cpp \
    common/Camera2ClientBase.cpp \
    common/CameraDeviceBase.cpp \
    common/FrameProcessorBase.cpp \
    api1/CameraClient.cpp \
    api1/Camera2Client.cpp \
    api1/client2/Parameters.cpp \
    api1/client2/FrameProcessor.cpp \
    api1/client2/StreamingProcessor.cpp \
    api1/client2/JpegProcessor.cpp \
    api1/client2/CallbackProcessor.cpp \
    api1/client2/ZslProcessor.cpp \
    api1/client2/BurstCapture.cpp \
    api1/client2/JpegCompressor.cpp \
    api1/client2/CaptureSequencer.cpp \
    api1/client2/ZslProcessor3.cpp \
    api2/CameraDeviceClient.cpp \
    api_pro/ProCamera2Client.cpp \
    device2/Camera2Device.cpp \
    device3/Camera3Device.cpp \
    device3/Camera3Stream.cpp \
    device3/Camera3IOStreamBase.cpp \
    device3/Camera3InputStream.cpp \
    device3/Camera3OutputStream.cpp \
    device3/Camera3ZslStream.cpp \
    device3/StatusTracker.cpp \
    gui/RingBufferConsumer.cpp \

LOCAL_SHARED_LIBRARIES:= \
    libui \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libmedia \
    libcamera_client \
    libgui \
    libhardware \
    libsync \
    libcamera_metadata \
    libjpeg

LOCAL_C_INCLUDES += \
    system/media/camera/include \
    external/jpeg

ifeq ($(strip $(INTEL_FEATURE_ASF)),true)
ifneq ($(strip $(PLATFORM_ASF_VERSION)),1)
ifneq ($(strip $(PLATFORM_ASF_VERSION)),0)
    LOCAL_SHARED_LIBRARIES += libsecuritydeviceserviceclient
    LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libsecuritydeviceserviceclient
endif
endif
endif

LOCAL_CFLAGS += -Wall -Wextra

LOCAL_MODULE:= libcameraservice

include $(BUILD_SHARED_LIBRARY)
