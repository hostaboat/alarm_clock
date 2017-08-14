#ifndef _SPI_CTA_H_
#define _SPI_CTA_H_

#include "types.h"

////////////////////////////////////////////////////////////////////////////////
// Clock and Transfer Attributes
////////////////////////////////////////////////////////////////////////////////
// uint32_t sck   - frequency in Hz
// uint32_t t_csc - the delay between assertion of PCS and the first edge of SCK
// uint32_t t_asc - the delay between the last edge of SCK and negation of PCS
// uint32_t t_dt  - the time between negation of PCS and next assertion of PCS
// This function returns the relevant bits for use in the CTAR field.
uint32_t spi_cta(uint32_t sck, uint32_t t_csc, uint32_t t_asc, uint32_t t_dt);

////////////////////////////////////////////////////////////////////////////////
// SCK baud rate to be used for SPI transfers.
////////////////////////////////////////////////////////////////////////////////
// Given the frequency in Hz argument passed in, this function
// returns the relevant bits for use in the CTAR field.
uint32_t spi_sck(uint32_t frequency);

////////////////////////////////////////////////////////////////////////////////
// This is the delay between the assertion of PCS and the first edge of SCK.
////////////////////////////////////////////////////////////////////////////////
// Given the nanosecond PCS to SCK delay argument passed in, this function
// returns the relevant bits for use in the CTAR field.
uint32_t spi_pcs_to_sck_delay(uint32_t nanoseconds);

////////////////////////////////////////////////////////////////////////////////
// This is the delay between the last edge of SCK and the negation of PCS.
////////////////////////////////////////////////////////////////////////////////
// Given the nanosecond Delay After SCK argument passed in, this
// function returns the relevant bits for use in the CTAR field.
uint32_t spi_delay_after_sck(uint32_t nanoseconds);

////////////////////////////////////////////////////////////////////////////////
// This is the time between the negation of the PCS signal at the end of
// a frame and the assertion of PCS at the beginning of the next frame.
////////////////////////////////////////////////////////////////////////////////
// Given the nanosecond Delay After Transfer argument passed in, this
// function returns the relevant bits for use in the CTAR field.
uint32_t spi_delay_after_transfer(uint32_t nanoseconds);

#endif
