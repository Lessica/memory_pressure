export TARGET := iphone:clang:16.5:14.0
export ARCHS := arm64
export GO_EASY_ON_ME := 1

include $(THEOS)/makefiles/common.mk

TOOL_NAME += memory_pressure
memory_pressure_FILES += memory_pressure.c
memory_pressure_CFLAGS += -Iinclude
memory_pressure_CFLAGS += -Wno-unused-but-set-variable
memory_pressure_CODESIGN_FLAGS = -Sent.xml
memory_pressure_INSTALL_PATH = /usr/local/bin

include $(THEOS_MAKE_PATH)/tool.mk