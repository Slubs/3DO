# Copyright (C) 2023-2024
# Modified for multi-disc support and audio improvements

LOCAL_PATH := $(call my-dir)

# libretro-common
include $(CLEAR_VARS)
LOCAL_MODULE    := retro-common
LOCAL_SRC_FILES := ../libretro-common/file/file_path.c \
                   ../libretro-common/streams/file_stream.c \
                   ../libretro-common/streams/chd_stream.c \
                   ../libretro-common/string/stdstring.c \
                   ../libretro-common/memmap/memalign.c \
                   ../libretro-common/encodings/encoding_utf.c
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include
include $(BUILD_STATIC_LIBRARY)

# libopera
include $(CLEAR_VARS)
LOCAL_MODULE    := libopera
LOCAL_SRC_FILES := ../libopera/opera_3do.c \
                   ../libopera/opera_arm.c \
                   ../libopera/opera_bios.c \
                   ../libopera/opera_cdrom.c \
                   ../libopera/opera_clock.c \
                   ../libopera/opera_dsp.c \
                   ../libopera/opera_fixedpoint_math.c \
                   ../libopera/opera_log.c \
                   ../libopera/opera_madam.c \
                   ../libopera/opera_mem.c \
                   ../libopera/opera_nvram.c \
                   ../libopera/opera_pbus.c \
                   ../libopera/opera_region.c \
                   ../libopera/opera_sport.c \
                   ../libopera/opera_state.c \
                   ../libopera/opera_vdlp.c \
                   ../libopera/opera_xbus.c \
                   ../libopera/opera_xbus_cdrom_plugin.c \
                   ../libopera/opera_clio.c \
                   ../libopera/opera_diag_port.c \
                   ../libopera/prng16.c \
                   ../libopera/prng32.c \
                   ../libopera/opera_bitop.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include \
                    $(LOCAL_PATH)/../libopera
include $(BUILD_STATIC_LIBRARY)

# cuefile
include $(CLEAR_VARS)
LOCAL_MODULE    := cuefile
LOCAL_SRC_FILES := ../cuefile.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include \
                    $(LOCAL_PATH)/../libopera
include $(BUILD_STATIC_LIBRARY)

# lr_input
include $(CLEAR_VARS)
LOCAL_MODULE    := lr_input
LOCAL_SRC_FILES := ../lr_input.c \
                   ../lr_input_descs.c \
                   ../lr_input_crosshair.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include \
                    $(LOCAL_PATH)/../libopera
include $(BUILD_STATIC_LIBRARY)

# opera_lr
include $(CLEAR_VARS)
LOCAL_MODULE    := opera_lr
LOCAL_SRC_FILES := ../opera_lr_callbacks.c \
                   ../opera_lr_dsp.c \
                   ../opera_lr_nvram.c \
                   ../opera_lr_opts.c \
                   ../retro_cdimage.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include \
                    $(LOCAL_PATH)/../libopera
include $(BUILD_STATIC_LIBRARY)

# Main libretro core
include $(CLEAR_VARS)
LOCAL_MODULE    := opera_libretro
LOCAL_SRC_FILES := ../libretro.c \
                   ../libretro_core_options.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../libretro-common/include \
                    $(LOCAL_PATH)/../libopera
LOCAL_STATIC_LIBRARIES := retro-common libopera cuefile lr_input opera_lr
LOCAL_LDLIBS := -llog -landroid
include $(BUILD_SHARED_LIBRARY)