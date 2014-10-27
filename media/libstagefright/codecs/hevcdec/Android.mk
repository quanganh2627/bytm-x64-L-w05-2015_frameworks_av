LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE            := libstagefright_soft_hevcdec
LOCAL_MODULE_TAGS       := optional

LOCAL_STATIC_LIBRARIES  := libhevcdec
LOCAL_SRC_FILES         := SoftHEVC.cpp

LOCAL_C_INCLUDES := $(TOP)/hardware/intel/vpu/libhevc/decoder
LOCAL_C_INCLUDES += $(TOP)/hardware/intel/vpu/libhevc/common
LOCAL_C_INCLUDES += $(TOP)/frameworks/av/media/libstagefright/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include/media/openmax

LOCAL_SHARED_LIBRARIES  := libstagefright
LOCAL_SHARED_LIBRARIES  += libstagefright_omx
LOCAL_SHARED_LIBRARIES  += libstagefright_foundation
LOCAL_SHARED_LIBRARIES  += libutils
LOCAL_SHARED_LIBRARIES  += liblog


include $(BUILD_SHARED_LIBRARY)
