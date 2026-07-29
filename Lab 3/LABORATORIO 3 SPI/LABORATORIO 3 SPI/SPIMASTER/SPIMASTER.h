/*
 * SPIMASTER.h
 *
 * Created: 23/07/2026
 *  Author: migue
 */

#ifndef SPIMASTER_H_
#define SPIMASTER_H_

#include <stdint.h>

#define SPI_CMD_POT1_HIGH 0x10
#define SPI_CMD_POT1_LOW  0x11
#define SPI_CMD_POT2_HIGH 0x12
#define SPI_CMD_POT2_LOW  0x13
#define SPI_CMD_SET_LEDS  0x20
#define SPI_DUMMY_BYTE    0xFF

void SPIMASTER_Init(void);
uint8_t SPIMASTER_Transfer(uint8_t data);
uint8_t SPIMASTER_ReadByte(uint8_t command);
uint16_t SPIMASTER_ReadPot(uint8_t pot);
void SPIMASTER_SendValue(uint8_t value);

#endif /* SPIMASTER_H_ */
