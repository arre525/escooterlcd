/*
 * lcd44780_LP.c
 *		Basic HD44780 driver for Stellaris Launchpad
 *      Version 1.00
 *      Author: NYH
 *      Reference: Robot Head to Toe Vol. 11 - pg 35-36
 *      Note: One full port must be used for this LCD. In this driver PORTB is used.
 */

#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_timer.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "lcd44780_LP.h"

void initLCD() {

	SysCtlPeripheralEnable(LCDPORTENABLE);
	GPIOPinTypeGPIOOutput(LCDPORT,
				0xff);

	// Please refer to the HD44780 datasheet for how these initializations work!
	SysCtlDelay((500e-3)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, RS,  0x00 );

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7,  0x30 );
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((50e-3)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7,  0x30 );
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3);GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((50e-3)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7,  0x30 );
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((10e-3)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7,  0x20 );
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((10e-3)*CLKSPEED/3);

	LCDCommand(0x01);	// Clear the screen.
	LCDCommand(0x06);	// Cursor moves right.
	LCDCommand(0x0f);	// Cursor blinking, turn on LCD.

	// Now change contrast, by using PWM output on GPIO pin V0 (required configuring timer)
	// You could also use the pwm.h functions instead?
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
	GPIOPinConfigure(GPIO_PB2_T3CCP0);
	GPIOPinTypeTimer(GPIO_PORTB_BASE, GPIO_PIN_2);


	//GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_2);
	//GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_2,0);

	unsigned long ulPeriod = (SysCtlClockGet() / 50000)/2;
	unsigned long dutyCycle1 = (unsigned long)(ulPeriod-1)*0.7;

	TimerConfigure(TIMER3_BASE, (TIMER_CFG_SPLIT_PAIR|TIMER_CFG_A_PWM|TIMER_CFG_B_PWM));
	TimerControlLevel(TIMER3_BASE, TIMER_BOTH, 0);
	TimerLoadSet(TIMER3_BASE, TIMER_A, ulPeriod -1);
	TimerMatchSet(TIMER3_BASE, TIMER_A, dutyCycle1);
	TimerEnable(TIMER3_BASE, TIMER_A);

	LCDCommand(0x0c);
}


void LCDCommand(unsigned char command) {

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7, (command & 0xf0) );
	GPIOPinWrite(LCDPORT, RS, 0x00);
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((100e-6)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7, (command & 0x0f) << 4 );
	GPIOPinWrite(LCDPORT, RS, 0x00);
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((5e-3)*CLKSPEED/3);

}

void LCDWrite(unsigned char inputData) {

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7, (inputData & 0xf0) );
	GPIOPinWrite(LCDPORT, RS, 0x01);
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((100e-6)*CLKSPEED/3);

	GPIOPinWrite(LCDPORT, D4 | D5 | D6 | D7, (inputData & 0x0f) << 4 );
	GPIOPinWrite(LCDPORT, RS, 0x01);
	GPIOPinWrite(LCDPORT, E, 0x02);SysCtlDelay((20e-6)*CLKSPEED/3); GPIOPinWrite(LCDPORT, E, 0x00);

	SysCtlDelay((5e-3)*CLKSPEED/3);

}

void LCDWriteText(char* inputText,unsigned char row, unsigned char col) {
	unsigned char address_d = 0;		// address of the data in the screen.
	switch(row)
	{
	case 0: address_d = 0x80 + col;		// at zeroth row
	break;
	case 1: address_d = 0xC0 + col;		// at first row
	break;
	case 2: address_d = 0x94 + col;		// at second row
	break;
	case 3: address_d = 0xD4 + col;		// at third row
	break;
	default: address_d = 0x80 + col;	// returns to first row if invalid row number is detected
	break;
	}

	LCDCommand(address_d);

	while(*inputText)					// Place a string, letter by letter.
		LCDWrite(*inputText++);
}
