#!/usr/bin/env python

#define SPI_CTAR_DBR       (1 << 31)            // Double Baud Rate
#define SPI_CTAR_FMSZ(n)   (((n) & 0xF) << 27)  // Frame Size (+1)
#define SPI_CTAR_CPOL      (1 << 26)            // Clock Polarity
#define SPI_CTAR_CPHA      (1 << 25)            // Clock Phase
#define SPI_CTAR_LSBFE     (1 << 24)            // LSB First
#define SPI_CTAR_PCSSCK(n) (((n) & 0x3) << 22)  // PCS to SCK Delay Prescaler
#define SPI_CTAR_PASC(n)   (((n) & 0x3) << 20)  // After SCK Delay Prescaler
#define SPI_CTAR_PDT(n)    (((n) & 0x3) << 18)  // Delay after Transfer Prescaler
#define SPI_CTAR_PBR(n)    (((n) & 0x3) << 16)  // Baud Rate Prescaler
#define SPI_CTAR_CSSCK(n)  (((n) & 0xF) << 12)  // PCS to SCK Delay Scaler
#define SPI_CTAR_ASC(n)    (((n) & 0xF) << 8)   // After SCK Delay Scaler
#define SPI_CTAR_DT(n)     (((n) & 0xF) << 4)   // Delay After Transfer Scaler
#define SPI_CTAR_BR(n)     (((n) & 0xF) << 0)   // Baud Rate Scaler


DBR = {
    0: 0,
    1: 1,
}

PBR = {
    2: 0,
    3: 1,
    5: 2,
    7: 3,
}

BR = {
        2:  0,
        4:  1,
        6:  2,
        8:  3,
       16:  4,
       32:  5,
       64:  6,
      128:  7,
      256:  8,
      512:  9,
     1024: 10,
     2048: 11,
     4096: 12,
     8192: 13,
    16384: 14,
    32768: 15,
}

F_BUS = [
    48000000,
    36000000,
]

clocks = {}

for fbus in sorted(F_BUS):
    clocks[fbus] = {}
    for br in sorted(BR.keys()):  # Baud Rate Scaler
        for pbr in sorted(PBR.keys()):  # Baud Rate Prescaler
            for dbr in sorted(DBR.keys()):  # Double Baud Rate
                if dbr == 1 and pbr != 2:
                    continue
                #print "**********************************************************************"
                #print "F_BUS: {0}".format(fbus)
                #print "  DBR: {0}".format(dbr)
                #print "  PBR: {0}, {1}".format(pbr, PBR[pbr]);
                #print "   BR: {0}, {1}".format(br, BR[br]);
                #baud_rate = (fbus/pbr) * ((1 + dbr)/br)
                baud_rate = (fbus * (1 + dbr)) / (1.0 * pbr * br)
                baud_rate = int(baud_rate)
                #print "  SCK: {0}".format(baud_rate)
                ctar = "SPI_CTAR_BR({0}) | SPI_CTAR_PBR({1})".format(BR[br], PBR[pbr])
                if (dbr == 1):
                    ctar += " | SPI_CTAR_DBR"
                #print ctar
                #print "**********************************************************************"
                #if (baud_rate >= 1000000):
                #    baud_rate_mhz = baud_rate / 1000000.0
                #    macro = "# define SPI_SCK_{0}MHz  {1}".format(baud_rate_mhz, ctar)
                #elif (baud_rate >= 1000):
                #    baud_rate_khz = baud_rate / 1000.0
                #    macro = "# define SPI_SCK_{0}kHz  {1}".format(baud_rate_khz, ctar)
                #else:
                #    macro = "# define SPI_SCK_{0}Hz   {1}".format(baud_rate, ctar)
                #clocks[fbus][baud_rate] = macro
                if baud_rate not in clocks[fbus].keys():
                    clocks[fbus][baud_rate] = ctar

CSSCK = {
        2:  0,
        4:  1,
        8:  2,
       16:  3,
       32:  4,
       64:  5,
      128:  6,
      256:  7,
      512:  8,
     1024:  9,
     2048: 10,
     4096: 11,
     8192: 12,
    16384: 13,
    32768: 14,
    65536: 15,
}

PCSSCK = {
    1: 0,
    3: 1,
    5: 2,
    7: 3,
}

pcs_to_sck = {}
for fbus in sorted(F_BUS):
    pcs_to_sck[fbus] = {}
    for cssck in sorted(CSSCK.keys()):
        for pcssck in sorted(PCSSCK.keys()):
            tcsc = ((cssck * pcssck) / (fbus * 1.0)) * 1000000000
            tcsc = int(tcsc)
            ctar = "SPI_CTAR_CSSCK({0}) | SPI_CTAR_PCSSCK({1})".format(CSSCK[cssck], PCSSCK[pcssck])
            if tcsc not in pcs_to_sck[fbus].keys():
                pcs_to_sck[fbus][tcsc] = ctar

delay_after_transfer = {}
for fbus in sorted(F_BUS):
    delay_after_transfer[fbus] = {}
    for cssck in sorted(CSSCK.keys()):
        for pcssck in sorted(PCSSCK.keys()):
            tdt = ((cssck * pcssck) / (fbus * 1.0)) * 1000000000
            tdt = int(tdt)
            ctar = "SPI_CTAR_DT({0}) | SPI_CTAR_PDT({1})".format(CSSCK[cssck], PCSSCK[pcssck])
            if tdt not in delay_after_transfer[fbus].keys():
                delay_after_transfer[fbus][tdt] = ctar

delay_after_sck = {}
for fbus in sorted(F_BUS):
    delay_after_sck[fbus] = {}
    for cssck in sorted(CSSCK.keys()):
        for pcssck in sorted(PCSSCK.keys()):
            tdt = ((cssck * pcssck) / (fbus * 1.0)) * 1000000000
            tdt = int(tdt)
            ctar = "SPI_CTAR_ASC({0}) | SPI_CTAR_PASC({1})".format(CSSCK[cssck], PCSSCK[pcssck])
            if tdt not in delay_after_sck[fbus].keys():
                delay_after_sck[fbus][tdt] = ctar

print "#include \"types.h\""
print "#include \"spi.h\""
print "#include \"spi_cta.h\""
print
print "#ifndef F_BUS"
print "# error \"F_BUS not defined\""
print "#endif"
print
print "/"*80
print "// Clock and Transfer Attributes"
print "/"*80
print "// uint32_t sck   - frequency in Hz"
print "// uint32_t t_csc - the delay between assertion of PCS and the first edge of SCK"
print "// uint32_t t_asc - the delay between the last edge of SCK and negation of PCS"
print "// uint32_t t_dt  - the time between negation of PCS and next assertion of PCS"
print "// This function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_cta(uint32_t sck, uint32_t t_csc, uint32_t t_asc, uint32_t t_dt);"
print
print "/"*80
print "// SCK baud rate to be used for SPI transfers."
print "/"*80
print "// Given the frequency in Hz argument passed in, this function"
print "// returns the relevant bits for use in the CTAR field."
print "uint32_t spi_sck(uint32_t frequency);"
print
print "/"*80
print "// This is the delay between the assertion of PCS and the first edge of SCK."
print "/"*80
print "// Given the nanosecond PCS to SCK delay argument passed in, this function"
print "// returns the relevant bits for use in the CTAR field."
print "uint32_t spi_pcs_to_sck_delay(uint32_t nanoseconds);"
print
print "/"*80
print "// This is the delay between the last edge of SCK and the negation of PCS."
print "/"*80
print "// Given the nanosecond Delay After SCK argument passed in, this"
print "// function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_delay_after_sck(uint32_t nanoseconds);"
print
print "/"*80
print "// This is the time between the negation of the PCS signal at the end of"
print "// a frame and the assertion of PCS at the beginning of the next frame."
print "/"*80
print "// Given the nanosecond Delay After Transfer argument passed in, this"
print "// function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_delay_after_transfer(uint32_t nanoseconds);"
print

print "/"*80
print "// Clock and Transfer Attributes"
print "/"*80
print "// uint32_t sck   - frequency in Hz"
print "// uint32_t t_csc - the delay between assertion of PCS and the first edge of SCK"
print "// uint32_t t_asc - the delay between the last edge of SCK and negation of PCS"
print "// uint32_t t_dt  - the time between negation of PCS and next assertion of PCS"
print "// This function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_cta(uint32_t sck, uint32_t t_csc, uint32_t t_asc, uint32_t t_dt)"
print "{"
print "    return (spi_sck(sck)"
print "            | spi_pcs_to_sck_delay(t_csc)"
print "            | spi_delay_after_sck(t_asc)"
print "            | spi_delay_after_transfer(t_dt));"
print "}"
print

print "/"*80
print "// SCK baud rate to be used for SPI transfers."
print "// Given the frequency in Hz argument passed in, this function"
print "// returns the relevant bits for use in the CTAR field."
print "uint32_t spi_sck(uint32_t frequency)"
print "{"
print "    uint32_t sck;"
print
first_bus = True
for fbus in sorted(clocks.keys(), reverse=True):
    if first_bus:
        print "#if F_BUS == {0}".format(fbus)
        first_bus = False
    else:
        print "#elif F_BUS == {0}".format(fbus)
    baud_rates = sorted(clocks[fbus].keys(), reverse=True)
    count = len(baud_rates)
    for baud_rate in baud_rates:
        if count == len(baud_rates):
            print "    if (frequency >= {0})".format(baud_rate)
        elif count > 1:
            print "    else if (frequency >= {0})".format(baud_rate)
        else:
            print "    else"
        print "        sck = {0};".format(clocks[fbus][baud_rate])
        count -= 1
print "#else"
print "# error \"F_BUS not valid\""
print "#endif"
print
print "    return sck;"
print "}"
print

print "/"*80
print "// This is the delay between the assertion of PCS and the first edge of SCK."
print "/"*80
print "// Given the nanosecond PCS to SCK delay argument passed in, this function"
print "// returns the relevant bits for use in the CTAR field."
print "uint32_t spi_pcs_to_sck_delay(uint32_t nanoseconds)"
print "{"
print "    uint32_t t_csc;"
print
first_bus = True
for fbus in sorted(pcs_to_sck.keys(), reverse=True):
    if first_bus:
        print "#if F_BUS == {0}".format(fbus)
        first_bus = False
    else:
        print "#elif F_BUS == {0}".format(fbus)
    tcscs = sorted(pcs_to_sck[fbus].keys())
    count = len(tcscs)
    for tcsc in tcscs:
        if count == len(tcscs):
            print "    if (nanoseconds <= {0})".format(tcsc)
        elif count > 1:
            print "    else if (nanoseconds <= {0})".format(tcsc)
        else:
            print "    else"
        print "        t_csc = {0};".format(pcs_to_sck[fbus][tcsc])
        count -= 1
print "#else"
print "# error \"F_BUS not valid\""
print "#endif"
print
print "    return t_csc;"
print "}"
print

print "/"*80
print "// This is the delay between the last edge of SCK and the negation of PCS."
print "/"*80
print "// Given the nanosecond Delay After SCK argument passed in, this"
print "// function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_delay_after_sck(uint32_t nanoseconds)"
print "{"
print "    uint32_t t_asc;"
print
first_bus = True
for fbus in sorted(delay_after_sck.keys(), reverse=True):
    if first_bus:
        print "#if F_BUS == {0}".format(fbus)
        first_bus = False
    else:
        print "#elif F_BUS == {0}".format(fbus)
    tascs = sorted(delay_after_sck[fbus].keys())
    count = len(tascs)
    for tasc in tascs:
        if count == len(tascs):
            print "    if (nanoseconds <= {0})".format(tasc)
        elif count > 1:
            print "    else if (nanoseconds <= {0})".format(tasc)
        else:
            print "    else"
        print "        t_asc = {0};".format(delay_after_sck[fbus][tasc])
        count -= 1
print "#else"
print "# error \"F_BUS not valid\""
print "#endif"
print
print "    return t_asc;"
print "}"
print

print "/"*80
print "// This is the time between the negation of the PCS signal at the end of"
print "// a frame and the assertion of PCS at the beginning of the next frame."
print "/"*80
print "// Given the nanosecond Delay After Transfer argument passed in, this"
print "// function returns the relevant bits for use in the CTAR field."
print "uint32_t spi_delay_after_transfer(uint32_t nanoseconds)"
print "{"
print "    uint32_t t_dt;"
print
first_bus = True
for fbus in sorted(delay_after_transfer.keys(), reverse=True):
    if first_bus:
        print "#if F_BUS == {0}".format(fbus)
        first_bus = False
    else:
        print "#elif F_BUS == {0}".format(fbus)
    tdts = sorted(delay_after_transfer[fbus].keys())
    count = len(tdts)
    for tdt in tdts:
        if count == len(tdts):
            print "    if (nanoseconds <= {0})".format(tdt)
        elif count > 1:
            print "    else if (nanoseconds <= {0})".format(tdt)
        else:
            print "    else"
        print "        t_dt = {0};".format(delay_after_transfer[fbus][tdt])
        count -= 1
print "#else"
print "# error \"F_BUS not valid\""
print "#endif"
print
print "    return t_dt;"
print "}"
print


