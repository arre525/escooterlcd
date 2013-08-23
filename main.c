/*
 *  Arnout Diels, 2013
 */

#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "lcd44780_LP.h"
#include "utils/ustdlib.h"
#include "adc.h"

#define NUMOFSANMESS 5
struct sannemess {
	char firstline[20] ;
	char secondline[20];
} ;

// Ti's compiler does not support named literals apparently. We have to do it this way
struct sannemess sanmess[NUMOFSANMESS] = {	{"Sanne,          ", "Ik zie u graag!!"},\
											{"Ik hou zo veel  ", "van je, Sanne!! "},\
											{"Sanne, je maakt ", "me zo gelukkig! "},\
											{"Blijf altijd bij", "me, Sanne, schat"},\
											{"Sanne,je bent mn", "allerliefste..  "},\
};

/* Variables shared between adc.c and main */
long temperature = 0;
long voltage = 0;
long voltage1 = 0;
long voltage2 = 0;
long voltage3 = 0;
long voltage4 = 0;
long current = 0;
long power = 0;
long spower = 0;
long milljoule = 0;
long smilljoule = 0;
int powerfactor = 0;
long interrupts_per_second_adc = 0;

#define CLKFREQ 50000000 //50 Mhz

int main(void) {
	int i;
	int messlooper = 0;
	char LCDbuffer[20];


	// configure system clock to run at 50 MHz
	// use external crystal (16 MHz) and PLL (to boost freq to 400Mhz, then scaling by 4 * 2
	SysCtlClockSet(SYSCTL_SYSDIV_4|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

	// Init the ADC
	initadc();
	// Init the LCD
	initLCD();

	// Write a start-up message
	LCDWriteText(sanmess[messlooper].firstline, 0, 0);
	LCDWriteText(sanmess[messlooper].secondline, 1, 0);
	messlooper ++;
	messlooper = messlooper % NUMOFSANMESS;
	SysCtlDelay(CLKFREQ/9);

	// Now loop continuously, printing voltage, current, power, ..
	i = 0;
	while (1) {

		// Printing the numbers at the correct place requires some tricks with
		// the libc-based usnprintf from stellarisware.
		usnprintf(LCDbuffer,16,"%d                      ", voltage1);
		usnprintf(LCDbuffer+2,16,"V %d                  ", voltage2);
		usnprintf(LCDbuffer+6,16,"V %d                ", voltage3);
		usnprintf(LCDbuffer+10,16,"V %d               ", voltage4);
		usnprintf(LCDbuffer+14,16,"V                ", voltage4);

		LCDWriteText(LCDbuffer, 0, 0);

		// Current and power can be negative. Make sure we print the minus if required.
		usnprintf(LCDbuffer,16,"%d                   ",current);
		if (current > 0) {
			usnprintf(LCDbuffer+2,16," A                   ");
		} else {
			usnprintf(LCDbuffer+3,16,"A                   ");
		}
		usnprintf(LCDbuffer+5,16,"%d                   ",power);
		if (power > 0) {
			usnprintf(LCDbuffer+9,16,"W                   ");
		} else {
			usnprintf(LCDbuffer+10,16,"W                   ");
		}


		usnprintf(LCDbuffer + 12,16,"%d                 ",voltage);
		usnprintf(LCDbuffer + 14,16," V                    ");
		LCDWriteText(LCDbuffer, 1, 0);

		i++;
		if (i > 200) {
			/*
			LCDWriteText(sanmess[messlooper].firstline, 0, 0);
			LCDWriteText(sanmess[messlooper].secondline, 1, 0);
			messlooper ++;
			messlooper = messlooper % NUMOFSANMESS;
			for(i = 0; i < 5000000; i++);
			i = 0;
			*/

			// Every so often, we also change the screen to display total used power (and some other stats)
			usnprintf(LCDbuffer,16,"%d Wh                      ",milljoule/(1000*3600)); // overflow after +- 1Wh
			usnprintf(LCDbuffer+8,16,"%d SWh             ",(smilljoule/(1000*3600)));  // not relevant if not calculated correctly!
			LCDWriteText(LCDbuffer, 0, 0);
			usnprintf(LCDbuffer,18,"%d sps                    ",(interrupts_per_second_adc));

			/*
			usnprintf(LCDbuffer+10,16,"PF:%d               ",powerfactor); // not relevant if not calculated correctly!
			*/
			usnprintf(LCDbuffer + 12,16,"%d                 ",temperature);
			usnprintf(LCDbuffer + 14,16," C                    ");

			LCDWriteText(LCDbuffer, 1, 0);
			SysCtlDelay(CLKFREQ/9);
			i = 0;
		}
	}
}
