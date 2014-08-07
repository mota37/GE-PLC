#include "inc/lm4f120h5qr.h"
#include <inc/hw_ints.h>
#include <stdint.h>
//#include "common.h"
//#include "enc28j60.h"
//#include "spi.h"
//#include <uip/uip.h>
//#include <uip/uip_arp.h>
//#include <driverlib/systick.h>
//#include <driverlib/interrupt.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
//#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "utils/uartstdio.h"


#define SET_RUN_ON GPIOPinWrite(GPIO_PORTB_BASE, 0x10, 0x10);
#define SET_RUN_OFF GPIOPinWrite(GPIO_PORTB_BASE, 0x10, 0);
#define SET_INPUT {GPIO_PORTF_DIR_R = 0x00;}
#define SET_OUTPUT {GPIO_PORTF_DIR_R = 0x01;}

//Stucture to be used with the timer fcn.
struct Timer{
  unsigned char en;
  unsigned char dn;
  unsigned long setTime;
  unsigned long long endTime;
}timers[8];

//Completes a time delay
//SetTime is only loaded on the transition of enable from 0 to 1
//Updates after enable goes high will be inored until the next transition
void timer(struct Timer * cTimer,char enable, unsigned long setTime);

//Sets up timer hardware
void timerSetup();

void setupIO();

//Sets the bit number on the selected card
//Should not be used directly
void setBitN(unsigned char bitN);

//Read a single input bit
//Input the bit in hex format
//0xXY where X is the slot and Y is the bit
unsigned char readBit(unsigned char bit);

//Write a single output bit
//Input the bit in hex format
//0xXY where X is the slot and Y is the bit
void writeBit(unsigned char bit,unsigned char data);

/*
 * main.c
 */
int main(void) {
    setupIO();
    timerSetup();
    unsigned char toggle = 0;
    unsigned char input1,input2;

    while(1){
    	SET_RUN_ON;

    	input1 = readBit(0x00);
    	input2 = readBit(0x07);

    	writeBit(0x20,readBit(0x00));

    	writeBit(0x30,1);
    	timer(&timers[0],!timers[0].dn,5000);

    	if(timers[0].dn) toggle ^= 1;

    	writeBit(0x021,toggle);
    	timer(&timers[1],toggle,3000);
    	writeBit(0x32,timers[1].dn);
    }

	return 0;
}

void timerSetup(){
	//Set speed to 66MHZ
	 SysCtlClockSet(SYSCTL_SYSDIV_3|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);
	 SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER0);
	 SysCtlDelay(10);
	 TimerConfigure(WTIMER0_BASE,TIMER_CFG_ONE_SHOT_UP);
	 TimerDisable(WTIMER0_BASE,TIMER_BOTH);
	 //Set the timer to the max value
	 TimerLoadSet64(WTIMER0_BASE, ~0);//Timer isn't 64 bit but this sets both timers at once.
	 TimerEnable(WTIMER0_BASE, TIMER_A);
}

void setupIO()
{
	volatile unsigned long ulLoop;
	//Enable io ports
	SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOF + SYSCTL_RCGC2_GPIOD + SYSCTL_RCGC2_GPIOB
	  		+ SYSCTL_RCGC2_GPIOA;

	//This just uses a bit of time
	ulLoop = SYSCTL_RCGC2_R;

    //Setup io ports
	//Data IO
    GPIO_PORTF_DIR_R = 0x01;//Set as output
	GPIO_PORTF_LOCK_R = 0x4C4F434B;//Unlock the port conf
    GPIO_PORTF_CR_R = 0x00000001;
    GPIO_PORTF_DEN_R = 0x0d;
    GPIO_PORTF_PUR_R = 0x01;
    GPIO_PORTF_DR8R_R = 0x01;
    GPIO_PORTF_LOCK_R = 0;
    GPIO_PORTF_DATA_R = 0x0c;

    //Card select bits
    GPIO_PORTD_DATA_R = 0;
    GPIO_PORTD_LOCK_R = 0x4C4F434B;//Unlock the port conf
    GPIO_PORTD_CR_R =  0x0f;
    GPIO_PORTD_DIR_R = 0x0f;
    GPIO_PORTD_DEN_R = 0x0f;
    GPIO_PORTD_PUR_R = 0x0f;
    GPIO_PORTD_LOCK_R = 0;


    //Bit select and Run bit
    GPIO_PORTB_DATA_R = 0;
    GPIO_PORTB_DIR_R = 0x17;
    GPIO_PORTB_DEN_R = 0x17;
    GPIO_PORTB_PUR_R = 0x17;

	//Chip select b
    GPIO_PORTA_DIR_R = 0xe0;
    GPIO_PORTA_DEN_R = 0xe0;
    GPIO_PORTA_PUR_R = 0xe0;
    GPIO_PORTA_DATA_R = 0;

    GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0xC0);
}

void setBitN(unsigned char bitN)
{
	GPIOPinWrite(GPIO_PORTB_BASE, 0x07, ~bitN);
}

unsigned char readBit(unsigned char bit)
{
	unsigned char data;
	unsigned char slot = bit >> 4;
	GPIOPinWrite(GPIO_PORTF_BASE, 0x1, 0);

	SET_INPUT;
	GPIO_PORTD_DATA_R = slot;
	setBitN(bit);

	//Decide which slots to read
	//This may need to be expanded to include WRc and WRd
    if(slot < 8){
    	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0x80);//Active LOW
    }else{
    	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0x40);//Active LOW
    }


	SysCtlDelay(10);//Delay 30 cycles to all input to settle in 125nS
	data = GPIO_PORTF_DATA_R & 1;
	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0xC0);
	SET_OUTPUT;

	return !data;
}

void writeBit(unsigned char bit, unsigned char data)
{
	unsigned char slot = bit >> 4;
	SET_OUTPUT;
	GPIO_PORTD_DATA_R = slot;
	setBitN(bit);

	//Set output before bit is selected.
	GPIOPinWrite(GPIO_PORTF_BASE, 0x1, data);

	//Decide which slot to write and enable write
    if(slot < 8){
    	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0x80);
    }else{
    	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0x41);
    }

	//GPIO_PORTF_DATA_R = data;
	SysCtlDelay(10);//Delay 30 cycles
	GPIOPinWrite(GPIO_PORTA_BASE, 0xC0, 0xC0);//Disable write
	//SET_INPUT;
}

void timer(struct Timer *ctimer, char enable,unsigned long setTime)
{
  unsigned long long ctime = TimerValueGet64(WTIMER0_BASE);

  if(enable && !ctimer->en){//First time after enable goes high
	ctimer->setTime = setTime;
	ctimer->endTime = SysCtlClockGet();
	ctimer->endTime *= ctimer->setTime;
	ctimer->endTime /= 1000;
	ctimer->endTime += ctime;

	ctimer->en = 1;//Mark timer as enabled
	ctimer->dn = 0;
  }else if(enable && ctimer->en){//Timer is already running.
	if((ctimer->endTime <= ctime) && !ctimer->dn){
	   ctimer->dn = 1;//set done
	}
  }else{
	  ctimer->en = 0;
	  ctimer->dn = 0;
  }
}
