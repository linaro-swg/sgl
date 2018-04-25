LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  external/sedget/ta/include/optee/ \
  external/optee_client/public \
  $(LOCAL_PATH)/include \
  $(LOCAL_PATH)/src/include

LOCAL_SHARED_LIBRARIES := \
  liblog \
  libteec \
  libcutils

LOCAL_SRC_FILES := \
  src/memory/protected_mem.c \
  src/arm/mve_fw.c \
  src/optee/tee_service.c

LOCAL_MODULE := libsedget_video
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
