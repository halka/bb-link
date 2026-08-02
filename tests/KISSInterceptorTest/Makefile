# See https://github.com/bxparks/EpoxyDuino for documentation about this
# Makefile to compile and run Arduino programs natively on Linux or MacOS.

APP_SRC_PATH := $(CURDIR)/../../src/bb-link
APP_CPP_FILES := $(wildcard $(APP_SRC_PATH)/KISSInterceptor.cpp)
OBJS += $(APP_CPP_FILES:%.cpp=%.o)

APP_NAME := KISSInterceptorTest
DEPS += $(APP_SRC_PATH)/MockBluetoothSerial.h
ARDUINO_LIBS := AUnit ArduinoLog EpoxyEsp32
EPOXY_CORE := EPOXY_CORE_ESP32
EPOXYDUINO_DIR ?= $(HOME)/Documents/Arduino/libraries/EpoxyDuino
ifeq ($(wildcard $(EPOXYDUINO_DIR)/EpoxyDuino.mk),)
$(error EpoxyDuino not found; set EPOXYDUINO_DIR to its installation directory)
endif
include $(EPOXYDUINO_DIR)/EpoxyDuino.mk
