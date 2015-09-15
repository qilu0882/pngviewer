LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

LOCAL_STATIC_LIBRARIES := libcutils \
		libpng \
		libz \
		libc

LOCAL_MODULE := pngviewer

LOCAL_SRC_FILES := $(call all-subdir-c-files)

include $(BUILD_EXECUTABLE)
