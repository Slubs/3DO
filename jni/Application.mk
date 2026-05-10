# Application.mk for Opera libretro core
# Supports multi-disc and improved audio buffering

APP_ABI := armeabi-v7a arm64-v8a
APP_PLATFORM := android-21
APP_OPTIM := release
APP_THIN_ARCHIVE := true
APP_PIE := true
APP_STL := c++_static
APP_CPPFLAGS := -frtti -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
APP_CFLAGS := -O2 -DRETRO -DANDROID