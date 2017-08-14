# Most of this taken from:
#   http://www.nongnu.org/avr-libc/user-manual/group__demo__project.html
# Modified and shortened it up a bit though probably just need the .hex file
#
# $ PATH=/opt/avr/bin:$PATH make
# $ DYLD_LIBRARY_PATH=/opt/avr/compile/lib /opt/avr/bin/avrdude -c usbtiny -p t13 -U flash:w:lbo_led.hex -v
#

TARGET = lbo_led

MCU = attiny13a
OPTIMIZE = -Os

CC = avr-gcc
CPPFLAGS = -DF_CPU=9600000
CFLAGS = -g -Wall $(OPTIMIZE) -mmcu=$(MCU)
LDFLAGS = -Wl,-Map,$(TARGET).map

OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump

all: $(TARGET).elf lst text

$(TARGET).o: $(TARGET).c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $^

$(TARGET).elf: $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

lst: $(TARGET).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

text: hex bin srec

hex: $(TARGET).hex
bin: $(TARGET).bin
srec: $(TARGET).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

clean:
	rm -rf *.o *.elf *.lst *.map *.hex *.bin *.srec
