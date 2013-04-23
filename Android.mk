LOCAL_PATH := $(call my-dir)

#######################################################
#
#######################################################


include $(CLEAR_VARS)
LOCAL_MODULE := ppmap
LOCAL_SRC_FILES := main.c sock.c
LOCAL_CFLAGS := -Wall -DANDROID_NDK
LOCAL_LDFLAGS :=
LOCAL_C_INCLUDES += $(NDK_PROJECT_PATH)
include $(BUILD_EXECUTABLE)
