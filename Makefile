# Thanks to apmorton for helping me sort this:
# https://github.com/apmorton/teensy-template/blob/master/Makefile

TARGET = $(notdir $(CURDIR))

MCU = mk20dx256
F_CPU = 96000000
F_BUS = 48000000

TOOLSPATH = ./tools/teensy_loader_cli

GCCPATH = /opt/gcc-arm-none-eabi-6-2017-q2-update/bin
CC = $(abspath $(GCCPATH))/arm-none-eabi-gcc
CXX = $(abspath $(GCCPATH))/arm-none-eabi-g++
OBJCOPY = $(abspath $(GCCPATH))/arm-none-eabi-objcopy
SIZE = $(abspath $(GCCPATH))/arm-none-eabi-size

# Set this to neither, one or both for RTC compilation
# RTC_INIT - set only if first time using it
# RTC_VBAT - set if a coin cell battery is connected to Teensy
#RTC = -DRTC_INIT -DRTC_VBAT
RTC = -DRTC_VBAT
#RTC =

# Set this on first use.  Value should be one of 32, 64, 128, 512, 1024 or 2048
EEPROM_SIZE = -DEEPROM_SIZE=64

OPT = -O2
STD = gnu++11
PFLAGS = -mthumb -mcpu=cortex-m4
CFLAGS = $(OPT) -g -Wall $(PFLAGS) -ffunction-sections -fdata-sections -nostdlib -MMD
# Possible additional CFLAGS : -fsingle-precision-constant
CXXFLAGS = $(CFLAGS) -std=$(STD) -felide-constructors -fno-exceptions -fno-rtti
# Possible additional CXXFLAGS : -fstrict-enums -fno-threadsafe-statics
CPPFLAGS = -DF_CPU=$(F_CPU) -DF_BUS=$(F_BUS) $(RTC)
INCLUDES = -I.

LDSCRIPT = $(MCU).ld
LDFLAGS = $(OPT) -Wl,--gc-sections $(PFLAGS)
#LDFLAGS = $(OPT) -Wl,--gc-sections,--relax $(PFLAGS)
#LIBS = -lm -larm_cortexM4l_math

BUILDDIR = $(abspath $(CURDIR)/build)

CSRC := $(wildcard *.c)
CXXSRC := $(wildcard *.cpp)

SOURCES := $(CSRC:.c=.o) $(CXXSRC:.cpp=.o)
OBJS := $(foreach src, $(SOURCES), $(BUILDDIR)/$(src))

.PHONY : all
all : $(TARGET).hex

.PHONY : debug
debug : OPT = -O0
debug : $(TARGET).hex

.PHONY : upload
upload : $(TARGET).hex
	@-$(TOOLSPATH)/teensy_loader_cli --mcu=$(MCU) -w -v $(TARGET).hex

%.hex : %.elf
	@echo $(notdir $(OBJCOPY)) -O ihex -R .eeprom $(notdir $<) "$@"
	@$(SIZE) "$<"
	@$(OBJCOPY) -O ihex -R .eeprom "$<" "$@"

$(TARGET).elf : $(OBJS) $(LDSCRIPT)
	@echo $(notdir $(CC)) $(LDFLAGS) -T $(LDSCRIPT) -o $(notdir $@) $(notdir $(OBJS))
	@$(CC) $(LDFLAGS) -T $(LDSCRIPT) -o "$@" $(OBJS)

$(BUILDDIR)/%.o : %.c
	@echo $(notdir $(CC)) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o $(notdir $@) -c "$<"
	@mkdir -p "$(dir $@)"
	@$(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -o "$@" -c "$<"

$(BUILDDIR)/%.o : %.cpp
	@echo $(notdir $(CXX)) $(CXXFLAGS) $(CPPFLAGS) $(INCLUDES) -o $(notdir $@) -c "$<"
	@mkdir -p "$(dir $@)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(INCLUDES) -o "$@" -c "$<"

-include $(OBJS:.o=.d)

.PHONY : clean
clean:
	rm -rf "$(BUILDDIR)"
	rm -f "$(TARGET).elf" "$(TARGET).hex"
