/*
 * ADC.h
 *
 * Created: 23/07/2026
 *  Author: migue
 */

#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);

#endif /* ADC_H_ */
