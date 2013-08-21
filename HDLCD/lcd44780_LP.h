/*
 * lcd44780_LP.h
 *		Header file for the lcd44780_LP.c
 *		Version 1.00
 *      Author: NYH
 *      Reference: Robot Head to Toe Vol. 11 - pg 35-36
 */

#ifndef LCD44780_LP_H_
#define LCD44780_LP_H_

#define LCDPORT  		 GPIO_PORTB_BASE
#define LCDPORTENABLE    SYSCTL_PERIPH_GPIOB
#define RS 		 		 GPIO_PIN_0
#define E 				 GPIO_PIN_1
#define V0				 GPIO_PIN_2 // Contrast -> see .c file for hardcoded timer code
#define D4 				 GPIO_PIN_4
#define D5 				 GPIO_PIN_5
#define D6				 GPIO_PIN_6
#define D7 				 GPIO_PIN_7
#define CLKSPEED		 10000000 // acutally much higher, but this speeds up the LCD writing.

void initLCD();
void LCDCommand(unsigned char);
void LCDWrite(unsigned char);
void LCDWriteText(char*,unsigned char, unsigned char);

#endif /* LCD44780_LP_H_ */
