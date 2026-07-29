/*
 * SPISLAVE.h
 *
 * Created: 23/07/2026 05:43:49 p. m.
 *  Author: migue
 */

#ifndef SPISLAVE_H_
#define SPISLAVE_H_

#include <stdint.h>

#define SPI_CMD_POT1_HIGH 0x10
#define SPI_CMD_POT1_LOW  0x11
#define SPI_CMD_POT2_HIGH 0x12
#define SPI_CMD_POT2_LOW  0x13
#define SPI_CMD_SET_LEDS  0x20

typedef enum
{
    SPI_SLAVE_MODE        = 0x00,
    SPI_MASTER_OSC_DIV2  = 0x10,
    SPI_MASTER_OSC_DIV4  = 0x11,
    SPI_MASTER_OSC_DIV8  = 0x12,
    SPI_MASTER_OSC_DIV16 = 0x13,
    SPI_MASTER_OSC_DIV32 = 0x14,
    SPI_MASTER_OSC_DIV64 = 0x15,
    SPI_MASTER_OSC_DIV128 = 0x16
} Spi_type;

typedef enum
{
    SPI_DATA_ORDER_MSB = 0x00,
    SPI_DATA_ORDER_LSB = 0x20
} SPI_DATA_ORDER;

typedef enum
{
    SPI_CLOCK_POLARITY_LOW = 0x00,
    SPI_CLOCK_POLARITY_HIGH = 0x08
} Spi_Clock_Polarity;

typedef enum
{
    SPI_CLOCK_PHASE_LEADING = 0x00,
    SPI_CLOCK_PHASE_TRAILING = 0x04
} Spi_Clock_Phase;

void spiInit(Spi_type stype, SPI_DATA_ORDER sDataOrder, Spi_Clock_Polarity sClockPolarity, Spi_Clock_Phase sClockPhase);
void spiWrite(uint8_t dat);
unsigned spiDataReady(void);
char spiRead(void);
uint8_t spiTransfer(uint8_t dat);

void SPISLAVE_Init(void);
void SPISLAVE_SetReadings(uint16_t pot1, uint16_t pot2);
uint8_t SPISLAVE_GetReceivedValue(void);

#endif /* SPISLAVE_H_ */
