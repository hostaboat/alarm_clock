#!/usr/bin/env python

# The memory map has two 32-MB alias regions that map to two 1-MB bit-band regions:
#  * Accesses to the 32-MB SRAM alias region map to the 1-MB SRAM bit-band region.
#  * Accesses to the 32-MB peripheral alias region map to the 1-MB peripheral bit-band region.
# A mapping formula shows how to reference each word in the alias region to a corresponding
# bit, or target bit, in the bit-band region. The mapping formula is:
# 
# bit_word_offset = (byte_offset x 32) + (bit_number × 4)
# bit_word_addr = bit_band_base + bit_word_offset
#  * bit_word_offset is the position of the target bit in the bit-band memory region.
#  * bit_word_addr is the address of the word in the alias memory region that maps to the targeted bit.
#  * bit_band_base is the starting address of the alias region.
#  * byte_offset is the number of the byte in the bit-band region that contains the targeted bit.
#  * bit_number is the bit position, 0 to 7, of the targeted bit.

# Port registers
PORTA_PCR = 0x40049000
PORTB_PCR = 0x4004A000
PORTC_PCR = 0x4004B000
PORTD_PCR = 0x4004C000
PORTE_PCR = 0x4004D000

# GPIO registers
GPIOA_PDOR = 0x400FF000 # Port Data Output Register
GPIOA_PSOR = 0x400FF004 # Port Set Output Register
GPIOA_PCOR = 0x400FF008 # Port Clear Output Register
GPIOA_PTOR = 0x400FF00C # Port Toggle Output Register
GPIOA_PDIR = 0x400FF010 # Port Data Input Register
GPIOA_PDDR = 0x400FF014 # Port Data Direction Register
GPIOB_PDOR = 0x400FF040
GPIOB_PSOR = 0x400FF044
GPIOB_PCOR = 0x400FF048
GPIOB_PTOR = 0x400FF04C
GPIOB_PDIR = 0x400FF050
GPIOB_PDDR = 0x400FF054
GPIOC_PDOR = 0x400FF080
GPIOC_PSOR = 0x400FF084
GPIOC_PCOR = 0x400FF088
GPIOC_PTOR = 0x400FF08C
GPIOC_PDIR = 0x400FF090
GPIOC_PDDR = 0x400FF094
GPIOD_PDOR = 0x400FF0C0
GPIOD_PSOR = 0x400FF0C4
GPIOD_PCOR = 0x400FF0C8
GPIOD_PTOR = 0x400FF0CC
GPIOD_PDIR = 0x400FF0D0
GPIOD_PDDR = 0x400FF0D4
GPIOE_PDOR = 0x400FF100
GPIOE_PSOR = 0x400FF104
GPIOE_PCOR = 0x400FF108
GPIOE_PTOR = 0x400FF10C
GPIOE_PDIR = 0x400FF110
GPIOE_PDDR = 0x400FF114

# ISF registers
PORTA_ISFR = 0x400490A0
PORTB_ISFR = 0x4004A0A0
PORTC_ISFR = 0x4004B0A0
PORTD_ISFR = 0x4004C0A0
PORTE_ISFR = 0x4004D0A0

pin_bits = [
    16,
    17,
    0,
    12,
    13,
    7,
    4,
    2,
    3,
    3,
    4,
    6,
    7,
    5,
    1,
    0,
    0,
    1,
    3,
    2,
    5,
    6,
    1,
    2,
    5,
    19
]

port_pcr = [
    PORTB_PCR,
    PORTB_PCR,
    PORTD_PCR,
    PORTA_PCR,
    PORTA_PCR,
    PORTD_PCR,
    PORTD_PCR,
    PORTD_PCR,
    PORTD_PCR,
    PORTC_PCR,
    PORTC_PCR,
    PORTC_PCR,
    PORTC_PCR,
    PORTC_PCR,
    PORTD_PCR,
    PORTC_PCR,
    PORTB_PCR,
    PORTB_PCR,
    PORTB_PCR,
    PORTB_PCR,
    PORTD_PCR,
    PORTD_PCR,
    PORTC_PCR,
    PORTC_PCR,
    PORTA_PCR,
    PORTB_PCR,
]

gpio_pdor = [
    GPIOB_PDOR,
    GPIOB_PDOR,
    GPIOD_PDOR,
    GPIOA_PDOR,
    GPIOA_PDOR,
    GPIOD_PDOR,
    GPIOD_PDOR,
    GPIOD_PDOR,
    GPIOD_PDOR,
    GPIOC_PDOR,
    GPIOC_PDOR,
    GPIOC_PDOR,
    GPIOC_PDOR,
    GPIOC_PDOR,
    GPIOD_PDOR,
    GPIOC_PDOR,
    GPIOB_PDOR,
    GPIOB_PDOR,
    GPIOB_PDOR,
    GPIOB_PDOR,
    GPIOD_PDOR,
    GPIOD_PDOR,
    GPIOC_PDOR,
    GPIOC_PDOR,
    GPIOA_PDOR,
    GPIOB_PDOR,
]

gpio_psor = [
    GPIOB_PSOR,
    GPIOB_PSOR,
    GPIOD_PSOR,
    GPIOA_PSOR,
    GPIOA_PSOR,
    GPIOD_PSOR,
    GPIOD_PSOR,
    GPIOD_PSOR,
    GPIOD_PSOR,
    GPIOC_PSOR,
    GPIOC_PSOR,
    GPIOC_PSOR,
    GPIOC_PSOR,
    GPIOC_PSOR,
    GPIOD_PSOR,
    GPIOC_PSOR,
    GPIOB_PSOR,
    GPIOB_PSOR,
    GPIOB_PSOR,
    GPIOB_PSOR,
    GPIOD_PSOR,
    GPIOD_PSOR,
    GPIOC_PSOR,
    GPIOC_PSOR,
    GPIOA_PSOR,
    GPIOB_PSOR,
]

gpio_pcor = [
    GPIOB_PCOR,
    GPIOB_PCOR,
    GPIOD_PCOR,
    GPIOA_PCOR,
    GPIOA_PCOR,
    GPIOD_PCOR,
    GPIOD_PCOR,
    GPIOD_PCOR,
    GPIOD_PCOR,
    GPIOC_PCOR,
    GPIOC_PCOR,
    GPIOC_PCOR,
    GPIOC_PCOR,
    GPIOC_PCOR,
    GPIOD_PCOR,
    GPIOC_PCOR,
    GPIOB_PCOR,
    GPIOB_PCOR,
    GPIOB_PCOR,
    GPIOB_PCOR,
    GPIOD_PCOR,
    GPIOD_PCOR,
    GPIOC_PCOR,
    GPIOC_PCOR,
    GPIOA_PCOR,
    GPIOB_PCOR,
]

gpio_ptor = [
    GPIOB_PTOR,
    GPIOB_PTOR,
    GPIOD_PTOR,
    GPIOA_PTOR,
    GPIOA_PTOR,
    GPIOD_PTOR,
    GPIOD_PTOR,
    GPIOD_PTOR,
    GPIOD_PTOR,
    GPIOC_PTOR,
    GPIOC_PTOR,
    GPIOC_PTOR,
    GPIOC_PTOR,
    GPIOC_PTOR,
    GPIOD_PTOR,
    GPIOC_PTOR,
    GPIOB_PTOR,
    GPIOB_PTOR,
    GPIOB_PTOR,
    GPIOB_PTOR,
    GPIOD_PTOR,
    GPIOD_PTOR,
    GPIOC_PTOR,
    GPIOC_PTOR,
    GPIOA_PTOR,
    GPIOB_PTOR,
]

gpio_pdir = [
    GPIOB_PDIR,
    GPIOB_PDIR,
    GPIOD_PDIR,
    GPIOA_PDIR,
    GPIOA_PDIR,
    GPIOD_PDIR,
    GPIOD_PDIR,
    GPIOD_PDIR,
    GPIOD_PDIR,
    GPIOC_PDIR,
    GPIOC_PDIR,
    GPIOC_PDIR,
    GPIOC_PDIR,
    GPIOC_PDIR,
    GPIOD_PDIR,
    GPIOC_PDIR,
    GPIOB_PDIR,
    GPIOB_PDIR,
    GPIOB_PDIR,
    GPIOB_PDIR,
    GPIOD_PDIR,
    GPIOD_PDIR,
    GPIOC_PDIR,
    GPIOC_PDIR,
    GPIOA_PDIR,
    GPIOB_PDIR,
]

gpio_pddr = [
    GPIOB_PDDR,
    GPIOB_PDDR,
    GPIOD_PDDR,
    GPIOA_PDDR,
    GPIOA_PDDR,
    GPIOD_PDDR,
    GPIOD_PDDR,
    GPIOD_PDDR,
    GPIOD_PDDR,
    GPIOC_PDDR,
    GPIOC_PDDR,
    GPIOC_PDDR,
    GPIOC_PDDR,
    GPIOC_PDDR,
    GPIOD_PDDR,
    GPIOC_PDDR,
    GPIOB_PDDR,
    GPIOB_PDDR,
    GPIOB_PDDR,
    GPIOB_PDDR,
    GPIOD_PDDR,
    GPIOD_PDDR,
    GPIOC_PDDR,
    GPIOC_PDDR,
    GPIOA_PDDR,
    GPIOB_PDDR,
]

port_isfr = [
    PORTB_ISFR,
    PORTB_ISFR,
    PORTD_ISFR,
    PORTA_ISFR,
    PORTA_ISFR,
    PORTD_ISFR,
    PORTD_ISFR,
    PORTD_ISFR,
    PORTD_ISFR,
    PORTC_ISFR,
    PORTC_ISFR,
    PORTC_ISFR,
    PORTC_ISFR,
    PORTC_ISFR,
    PORTD_ISFR,
    PORTC_ISFR,
    PORTB_ISFR,
    PORTB_ISFR,
    PORTB_ISFR,
    PORTB_ISFR,
    PORTD_ISFR,
    PORTD_ISFR,
    PORTC_ISFR,
    PORTC_ISFR,
    PORTA_ISFR,
    PORTB_ISFR,
]

#for i in range(0, len(pin_bits)):
#    print "#define PIN{0}_PCR  (reg32){1}".format(i, hex(port_pcr[i] + (pin_bits[i] * 4)))
#print
#
#for i in range(0, len(pin_bits)):
#    print "#define PIN{0}_PDOR  (reg32){1}".format(i, hex(((gpio_pdor[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print "#define PIN{0}_PSOR  (reg32){1}".format(i, hex(((gpio_psor[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print "#define PIN{0}_PCOR  (reg32){1}".format(i, hex(((gpio_pcor[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print "#define PIN{0}_PTOR  (reg32){1}".format(i, hex(((gpio_ptor[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print "#define PIN{0}_PDIR  (reg32){1}".format(i, hex(((gpio_pdir[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print "#define PIN{0}_PDDR  (reg32){1}".format(i, hex(((gpio_pddr[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))
#    print
#
#for i in range(0, len(pin_bits)):
#    print "#define PIN{0}_ISFR  (reg32){1}".format(i, hex(((port_isfr[i] - 0x40000000) * 32) + (pin_bits[i] * 4) + 0x42000000))

# PORT PCRs
PORTA_PCR = 0x40049000
PORTB_PCR = 0x4004A000
PORTC_PCR = 0x4004B000
PORTD_PCR = 0x4004C000
PORTE_PCR = 0x4004D000

port_pcrs = {
    "A" : PORTA_PCR,
    "B" : PORTB_PCR,
    "C" : PORTC_PCR,
    "D" : PORTD_PCR,
    "E" : PORTE_PCR,
}

pcr_defs = {}
print "/" * 80
print "// PORT PCR"
print "/" * 80
print
for k, v in sorted(port_pcrs.items()):
    if k not in pcr_defs.keys():
        pcr_defs[k] = []
    for i in range(0, 32):
        if i < 10: space = 3
        else: space = 2 
        k1 = "PORT{0}_PCR{1}".format(k,i)
        pcr_defs[k].append(k1)
        print "#define {0}{1}(reg32)0x{2:X}".format(k1, ' '*space, v + (i * 4))
    print

# PORT ISFRs
PORTA_ISFR = 0x400490A0
PORTB_ISFR = 0x4004A0A0
PORTC_ISFR = 0x4004B0A0
PORTD_ISFR = 0x4004C0A0
PORTE_ISFR = 0x4004D0A0

port_isfrs = {
    "A" : PORTA_ISFR,
    "B" : PORTB_ISFR,
    "C" : PORTC_ISFR,
    "D" : PORTD_ISFR,
    "E" : PORTE_ISFR,
}

isfr_defs = {}
print "/" * 80
print "// PORT ISFR"
print "/" * 80
print
for k, v in sorted(port_isfrs.items()):
    print "#define PORT{0}_ISFR  (reg32)0x{1:X}".format(k, v)
print
print "// Aliased to PORT ISFR bit-band region"
print
for k, v in sorted(port_isfrs.items()):
    if k not in isfr_defs.keys():
        isfr_defs[k] = []
    for i in range(0, 32):
        if i < 10: space = 3
        else: space = 2 
        k1 = "PORT{0}_ISFR{1}".format(k, i)
        isfr_defs[k].append(k1)
        print "#define {0}{1}(reg32)0x{2:X}".format(k1, ' '*space, ((((v - 0x40000000) * 32) + i * 4) + 0x42000000))
    print

# GPIO registers
GPIOA_PDOR = 0x400FF000 # Port Data Output Register
GPIOA_PSOR = 0x400FF004 # Port Set Output Register
GPIOA_PCOR = 0x400FF008 # Port Clear Output Register
GPIOA_PTOR = 0x400FF00C # Port Toggle Output Register
GPIOA_PDIR = 0x400FF010 # Port Data Input Register
GPIOA_PDDR = 0x400FF014 # Port Data Direction Register
GPIOB_PDOR = 0x400FF040
GPIOB_PSOR = 0x400FF044
GPIOB_PCOR = 0x400FF048
GPIOB_PTOR = 0x400FF04C
GPIOB_PDIR = 0x400FF050
GPIOB_PDDR = 0x400FF054
GPIOC_PDOR = 0x400FF080
GPIOC_PSOR = 0x400FF084
GPIOC_PCOR = 0x400FF088
GPIOC_PTOR = 0x400FF08C
GPIOC_PDIR = 0x400FF090
GPIOC_PDDR = 0x400FF094
GPIOD_PDOR = 0x400FF0C0
GPIOD_PSOR = 0x400FF0C4
GPIOD_PCOR = 0x400FF0C8
GPIOD_PTOR = 0x400FF0CC
GPIOD_PDIR = 0x400FF0D0
GPIOD_PDDR = 0x400FF0D4
GPIOE_PDOR = 0x400FF100
GPIOE_PSOR = 0x400FF104
GPIOE_PCOR = 0x400FF108
GPIOE_PTOR = 0x400FF10C
GPIOE_PDIR = 0x400FF110
GPIOE_PDDR = 0x400FF114

gpio_regs = {
    "GPIOA" : {
        GPIOA_PDOR : "PDOR",
        GPIOA_PSOR : "PSOR",
        GPIOA_PCOR : "PCOR",
        GPIOA_PTOR : "PTOR",
        GPIOA_PDIR : "PDIR",
        GPIOA_PDDR : "PDDR",
    },

    "GPIOB" : {
        GPIOB_PDOR : "PDOR",
        GPIOB_PSOR : "PSOR",
        GPIOB_PCOR : "PCOR",
        GPIOB_PTOR : "PTOR",
        GPIOB_PDIR : "PDIR",
        GPIOB_PDDR : "PDDR",
    },

    "GPIOC" : {
        GPIOC_PDOR : "PDOR",
        GPIOC_PSOR : "PSOR",
        GPIOC_PCOR : "PCOR",
        GPIOC_PTOR : "PTOR",
        GPIOC_PDIR : "PDIR",
        GPIOC_PDDR : "PDDR",
    },

    "GPIOD" : {
        GPIOD_PDOR : "PDOR",
        GPIOD_PSOR : "PSOR",
        GPIOD_PCOR : "PCOR",
        GPIOD_PTOR : "PTOR",
        GPIOD_PDIR : "PDIR",
        GPIOD_PDDR : "PDDR",
    },

    "GPIOE" : {
        GPIOE_PDOR : "PDOR",
        GPIOE_PSOR : "PSOR",
        GPIOE_PCOR : "PCOR",
        GPIOE_PTOR : "PTOR",
        GPIOE_PDIR : "PDIR",
        GPIOE_PDDR : "PDDR",
    },
}

gpio_defs = []
for i in range(0, 6):
    gpio_defs.append({})

for k0, v0 in sorted(gpio_regs.items()):
    print "/" * 80
    print "// {0}".format(k0)
    print "/" * 80
    print
    for k1, v1 in sorted(v0.items()):
        print "#define {0}_{1}  (reg32)0x{2:X}".format(k0, v1, k1)
    print
    print "// Aliased to {0} bit-band region".format(k0)
    print
    gpio_reg = 0
    for k1, v1 in sorted(v0.items()):
        if k0 not in gpio_defs[gpio_reg].keys():
            gpio_defs[gpio_reg][k0] = []
        for i in range(0, 32):
            if i < 10: space = 3
            else: space = 2 
            v2 = "{0}_{1}{2}".format(k0, v1, i)
            print "#define {0}{1}(reg32)0x{2:X}".format(v2, ' '*space, ((((k1 - 0x40000000) * 32) + i * 4) + 0x42000000))

            gpio_defs[gpio_reg][k0].append(v2)
        gpio_reg += 1
        print

print "reg32 PORT::_s_port_pcr[5][32] ="
print "{"
for k, v in sorted(pcr_defs.items()):
    print "    {"
    for i in range(0, 32):
        if i < 10: space = 2
        else: space = 1 
        if i != 0 and i % 4 == 0: print
        if i % 4 == 0: indent = 8
        else: indent = 0
        print "{2}{0},{1}".format(v[i], ' '*space, ' '*indent),
    print
    print "    },"
print "};"
print

print "reg32 PORT::_s_port_isfr[5][32] ="
print "{"
for k, v in sorted(isfr_defs.items()):
    print "    {"
    for i in range(0, 32):
        if i < 10: space = 2
        else: space = 1 
        if i != 0 and i % 4 == 0: print
        if i % 4 == 0: indent = 8
        else: indent = 0
        print "{2}{0},{1}".format(v[i], ' '*space, ' '*indent),
    print
    print "    },"
print "};"
print

gpio_names = [ "PDOR", "PSOR", "PCOR", "PTOR", "PDIR", "PDDR" ]
    
for i in range(0,len(gpio_defs)):
    print "reg32 GPIO::_s_gpio_{0}[5][32] =".format(gpio_names[i].lower())
    print "{"
    for k, v in sorted(gpio_defs[i].items()):
        print "    {"
        for j in range(0, 32):
            if j < 10: space = 2
            else: space = 1 
            if j != 0 and j % 4 == 0: print
            if j % 4 == 0: indent = 8
            else: indent = 0
            print "{2}{0},{1}".format(v[j], ' '*space, ' '*indent),
            #print "{4}{0}_{1}{2},{3}".format(k, v1, i, ' '*space, ' '*indent),
        print
        print "    },"
    print "};"
    print

