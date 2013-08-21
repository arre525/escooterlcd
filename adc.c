/*
 *  Arnout Diels, 2013
 */


#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
//#include "driverlib/rom.h"
#include "driverlib/pin_map.h"
#include "driverlib/hibernate.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "adc.h"

/* variables also used in main.c for LCD printing */
extern long temperature;
extern long voltage;
extern long voltage1;
extern long voltage2;
extern long voltage3;
extern long voltage4;
extern long current;
extern long power;
extern long spower;
extern long powerfactor;
extern long milljoule;
extern long smilljoule;
extern long interrupts_per_second_adc;

/* variables updated at ADC trigger speed */
long temperaturesum = 0;
long voltagesum = 0;
long voltage1sum = 0;
long voltage2sum = 0;
long voltage3sum = 0;
long voltage4sum = 0;
long currentsum = 0;
long powersum = 0;

/* Some stats */
long triggeredintsadc0 = 0;
long triggeredintsadc1 = 0;
long triggeredintstim1 = 0;
long triggeredintsadc1_looper = 0;

#define AVGCALCPERSEC 10 // The amount of "averaging" calculations done per second (see below)

#define CLKFREQ 50000000 //50 Mhz


/*
 * ADC initialization:
 *
 * We use both ADC0 and ADC1.
 * ADC0 is just used for the temperature sensor.
 * ADC1 measures the current sensor and all voltages of the battery, using a 'sequence'
 *
 * Both are configured to trigger interrupts to functions below.
 * ADC0 however is configured to trigger at a timer event. (to only sample at certain moments)
 * ADC1 is configured to trigger the ISR at maximum speed (taking into account small HW averaging factor).
 *
 * So note that we do sample almost full speed, even though this requires of of averaging in software.
 * One might think this is a bit overkill, since the LCD can only change its display at a few Hz. (vs 1MSPS)
 * This is done deliberately however, since avgV * avgI != avgP.
 * Hence this method allows to actually measure reactive (high frequency) power fluctuations, if there were any.
 *
 *
 * For the ISR of ADC1:
 * When writing the code, I was not sure that I'd get a very steady samplerate (or interrupt fire-rate)
 * To make sure that I calculated averages per SECOND correctly, I added another timer to average out the
 * current samples over an exact time-interval.
 *
 * Later measurements however revealed that the ISR trigger rate was quite stable, so in the end, the extra timer was
 * probably not needed:)
 *
 * (Note though if the ISR fire-rate had been too erratic, the results would have been wrong even with the timer,
 *  but at least it would have been the best one could do.)
 *

 */
void initadc() {

	//////////////////////////////// TEMP SENSOR ////////////////////////////////////

	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_1MSPS); // Full speed. We average out in software.

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF); // GPIO F is connected internally to temperature sensor

	/* Configure a slow timer to trigger ADC0 (we don't want to loose too many CPU cycles on temperature measurement) */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); // full width (only A needed for confs)
	TimerControlTrigger(TIMER0_BASE,TIMER_A,1);
	TimerLoadSet(TIMER0_BASE, TIMER_A, CLKFREQ/AVGCALCPERSEC);
	TimerEnable(TIMER0_BASE, TIMER_A);

	/* Enable and Initialize the ADC0 */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
	ADCHardwareOversampleConfigure(ADC0_BASE, 64);
	ADCSequenceDisable(ADC0_BASE, 1);
	ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_TIMER, 0); // timer triggered (instead of processor/always/..)
	ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_TS);
	ADCSequenceStepConfigure(ADC0_BASE, 1, 3, ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END); // Take 4 samples in 1 sequence
	ADCSequenceEnable(ADC0_BASE, 1);

	// If we work via interrupts, we have to register & enable them.
	ADCIntRegister(ADC0_BASE,1,&parseadc0_int0);
	ADCIntEnable(ADC0_BASE,1);

	///////////////////////////////// V's and I /////////////////////////////////////////

	// Actual measurements
	// Done by continuously letting ADC1 trigger and causing an interrupt
	// But, every 0.1 second precisely, a timer also causes another interrupt, calculating the average values.
	// Using the timer instead of just x ADC interrupts is more accurate, since we don't know if the number of ints/s is constant.

	/* configure the timer (not directly related to ADC1) */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
	TimerLoadSet(TIMER1_BASE, TIMER_A, CLKFREQ/AVGCALCPERSEC);
	TimerIntRegister(TIMER1_BASE,TIMER_A,&timer1_int);
	TimerIntEnable(TIMER1_BASE,TIMER_TIMA_TIMEOUT);
	TimerEnable(TIMER1_BASE, TIMER_A);

	/* Configure the GPIO's to allow sampling */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_2); // not connected, use E3. (legacy code)
	GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_3); // see schematics for connections to voltage dividers. (PE4 = smallest voltage, 1 2 3 follow)
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_4);
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_1);
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_2);
	GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);

	/* Now actually configure the ADC1 */

	// Now enable ADC reading of the following pins via ADC1: (beware of evalboard connections! E.g. PD0 & 1 are connected!)
	// PD2 GPIO pin (channel 5)
	// PD3 GPIO pin (channel 4)
	// PE1, PE2, PE3, PE4 (channels 2 1 0 9)
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);
	ADCHardwareOversampleConfigure(ADC1_BASE, 4); // ideally, we want each sample separate, since e.g. [10 0]V * [0 10]A is 0W, and not 5W.
												  // However, this gives too many interrupts, so we average a little bit.
	ADCSequenceDisable(ADC1_BASE, 0);
	ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // max speed
	ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH5);
	ADCSequenceStepConfigure(ADC1_BASE, 0, 1, ADC_CTL_CH4);
	ADCSequenceStepConfigure(ADC1_BASE, 0, 2, ADC_CTL_CH2);
	ADCSequenceStepConfigure(ADC1_BASE, 0, 3, ADC_CTL_CH1);
	ADCSequenceStepConfigure(ADC1_BASE, 0, 4, ADC_CTL_CH0);
	ADCSequenceStepConfigure(ADC1_BASE, 0, 5, ADC_CTL_CH9 | ADC_CTL_IE | ADC_CTL_END);
	ADCSequenceEnable(ADC1_BASE, 0);

	// If we work via interrupts, we have to register & enable them.
	ADCIntRegister(ADC1_BASE,0,&parseadc1_int0);
	ADCIntEnable(ADC1_BASE,0);

}

/*
 * Temperature sensor ADC ISR (triggered at slow speed)
 */
void parseadc0_int0 () {
	static unsigned long ulADC0Value[4];
	static volatile long lADC_Sum;
	ADCIntClear(ADC0_BASE, 1); // Don't forget the clear the flag
	ADCSequenceDataGet(ADC0_BASE, 1, ulADC0Value);
	// Averaging code copied from nemetila (http://forum.stellarisiti.com/topic/471-using-stellaris-launchpad-as-a-thermometer/)
	lADC_Sum = (ulADC0Value[0] + ulADC0Value[1] + ulADC0Value[2] + ulADC0Value[2] + 2);
	temperaturesum +=  ( (long)(147.5*100 * 4) + 2 - ( (long)(247.5*100)*(lADC_Sum) + 2048)/4095 )>>2;
	triggeredintsadc0++;
	temperature = temperaturesum;
	temperaturesum = 0;
}

/*
 * High-speed ADC1 trigger
 * Add all millivolt measurements to a sum, and divide this later in the timer intterupt.
 * Note that the integer logic (+ avoid overflow) makes this a bit more complex than needed
 */
void parseadc1_int0 () {
	static unsigned long ulADC1Value[12];
	static long voltage1_mvmeasurement;
	static long voltage2_mvmeasurement;
	static long voltage3_mvmeasurement;
	static long voltage4_mvmeasurement;
	static long current_mvmeasurement;
	static long mvoltage_measurement;
	static long mvoltage1_measurement;
	static long mvoltage2_measurement;
	static long mvoltage3_measurement;
	static long mvoltage4_measurement;
	static long mcurrent_measurement;
	ADCIntClear(ADC1_BASE, 0);
	ADCSequenceDataGet(ADC1_BASE, 0, ulADC1Value); // holds D2 D3 E1 E2 E3 E4 => Vtot (not used) I V2 V3 V4 V1
	// convert to millivolts
	// 4096 (12 bit) from 0 to 3.3V => *3300/4096 mv
	voltage1_mvmeasurement = ((ulADC1Value[5]*3300)/4096);
	voltage2_mvmeasurement = ((ulADC1Value[2]*3300)/4096);
	voltage3_mvmeasurement = ((ulADC1Value[3]*3300)/4096);
	voltage4_mvmeasurement = ((ulADC1Value[4]*3300)/4096);
	current_mvmeasurement = ((ulADC1Value[1]*3300)/4096);

	/*
	 * Acutal R ratio's:
	 *
	 *  1.47/27.8 = 1/18.91 => 3.3V equals 62.4V
	 *  1/12.8   = 1/12.8 => 3.3V equals 42V
	 *  1.4/13.1 => 1/9.357
	 *  10.0/42.4 = 1/4.24 => 3.3V equals 14V
	 */

	mvoltage_measurement =  (voltage4_mvmeasurement*1891)/100 ;
	mvoltage1_measurement = (voltage1_mvmeasurement*424)/100 ;
	mvoltage2_measurement = (voltage2_mvmeasurement*9357)/1000 - mvoltage1_measurement ;
	mvoltage3_measurement = (voltage3_mvmeasurement*1280)/100 - mvoltage1_measurement - mvoltage2_measurement ;
	mvoltage4_measurement = (voltage4_mvmeasurement*1891)/100 - mvoltage1_measurement - mvoltage2_measurement - mvoltage3_measurement;
	mcurrent_measurement = -((current_mvmeasurement-1620)*408)/10; // negative is discharging, but display as positive

	// sums are in milli, except for power, which is in watt

	voltagesum += mvoltage_measurement;
	voltage1sum += mvoltage1_measurement;
	voltage2sum += mvoltage2_measurement;
	voltage3sum += mvoltage3_measurement;
	voltage4sum += mvoltage4_measurement;
	currentsum += mcurrent_measurement;

	// Watch out for overflow!
	powersum += ((mvoltage_measurement/100) * (mcurrent_measurement / 100))/100;


	triggeredintsadc1++;
	triggeredintsadc1_looper++;
	// Timer 1 will calculate the averages on time
}

/*
 * Timer interrupt to calculate energy per second relatively accurate
 */
void timer1_int () {
	static long mvoltage;
	static long mcurrent;
	//static long mpower;
	static long mspower;
	TimerIntClear(TIMER1_BASE,0xff);
	triggeredintstim1++;

	// Watch out for overflow, especially on mpower calc!

	// Calculate the averages:
	mvoltage = (voltagesum / (triggeredintsadc1_looper)); // quite accurate
	voltage = mvoltage / 1000;
	voltage1 =  voltage1sum / (triggeredintsadc1_looper * 1000);
	voltage2 =  voltage2sum / (triggeredintsadc1_looper * 1000);
	voltage3 =  voltage3sum / (triggeredintsadc1_looper * 1000);
	voltage4 =  voltage4sum / (triggeredintsadc1_looper * 1000);
	mcurrent = (currentsum) / (triggeredintsadc1_looper); // quite accurate
	current = mcurrent / 1000;
	//mpower = (powersum ) / (triggeredintsadc1_looper); // quite accurate (and can be big!)
	power = (powersum ) / (triggeredintsadc1_looper);
	mspower =  ((mvoltage/10) * (mcurrent/10))/10;
	spower = mspower / 1000;
	milljoule += ((power*1000)/AVGCALCPERSEC); // quite accurate (should be!)
	smilljoule += (mspower/AVGCALCPERSEC);
	powerfactor = (power*100/(spower));
	voltagesum = 0;
	voltage1sum = 0;
	voltage2sum = 0;
	voltage3sum = 0;
	voltage4sum = 0;
	currentsum = 0;
	powersum = 0;
	interrupts_per_second_adc = triggeredintsadc1_looper * AVGCALCPERSEC;
	triggeredintsadc1_looper = 0;
}

