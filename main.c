#define BAUD 9600
#define BAUD_TOL 2

#include <avr/io.h>
#include "spi3.h"
#include <stdint.h>
#include <stddef.h>
#include <util/delay.h>
#include "ADClib.h"
#include "stdio_setup.h"
#include <stdio.h>
#include <math.h>
#include <avr/interrupt.h>
#include "MCP4921.h"


uint16_t WFMFREQ = 0; // increases max LFO speed at the expense of DAC resolution
uint16_t CV = 0;

volatile uint16_t c = 0; // counter in ISR--for DAC
volatile uint8_t waveselect = 2; // counter in ISR -- debounce wave switch
//void SPI_TransferTx16(uint8_t a, uint8_t b);  // debug with stdio_setup.h printf to ser

 
void ramp(uint16_t freq);
void tri(uint16_t freq);
void squarewave(uint16_t freq);
uint16_t count = 0; // global counter for creating waveforms 
int16_t temp = 0;
uint8_t speed = 0;

 

//ISR routines have to go BEFORE main{}

ISR (TIMER0_COMPA_vect)  // timer0 overflow interrupt
{
	c++;
	
}

ISR (TIMER2_COMPA_vect)  // timer2 overflow interrupt
{
	if ((PIND & 0b00000010) == 0)
	{
	_delay_ms(500);
	waveselect++;
	
	}
	if (waveselect == 3)
	{
	waveselect = 0;	
	
	}
	
	
}




int main(void)
{
	//*  pin D0 for course speed change we will change OCR0A value *//
	//* pin D1 for waveform select NOTE!! for AVR GPIO 1 is output, 0 is input You knew that right?*//
	
	DDRD = 0b011100000;      //make an input D7, D6, D5 outputs all others are inputs
	PORTD = 0b00000011;    //enable pull-up for D0 and D1; this seems to trump I/O designation for DDRD register?
	
    /*  higher freq, less rez DEBUG, trying things out.....
	WFMFREQ adds to c count, more speed but less resolution for DAC.
	//WFMFREQ = 300; // for triangle you only get 4 steps at 300 WFMFREQ!!!
    //WFMFREQ = 20;
    //WFMFREQ = 10;
	//WFMFREQ = 5;  */
	
	WFMFREQ = 0;	
	//set up stdio_setup
	//UartInit();
	ADC_init();
    //initialize SPI
    
	init_spi_master();
    spi_mode(0);
	DESELECT();
	SELECT();

	//********TIMER--FREQ************
	
	// Set the Timer 0 Mode to CTC
	TCCR0A |= (1 << WGM01);

	// Set the value that you want to count to
	OCR0A = 0xFF;

	TIMSK0 |= (1 << OCIE0A);    //Interrupt fires when counter matches OPR0A value above.

	sei();         //enable interrupts  
	// in atmel s7 sei() is flagged as red squiggle due to 
	//"intellisence" beautifying,but will still compile. Doh!


    //TCCR0B  0 0 0 turns off clock, don't use!
	// CS2  CS1   CS0
	// 0     0     1  no prescale
	// 0     1     0  divide by 8
	// 0     1     1  divide by 64
	
    TCCR0B &= ~(1 << CS02); //CS2 to 0
	TCCR0B &= ~(1 << CS01);
	TCCR0B |= (1 << CS00);
	// set preschooler to no divide.
	
    //********END TIMER--FREQ************
	
	
	//********TIMER--WAVEFORM************
	//Use this to debounce momentary switch we will use for waveform select
	
	// Set the Timer 2 Mode to CTC
	TCCR2A |= (1 << WGM01);

	// Set the value that you want to count to
	OCR2A = 0xFF;

	TIMSK2 |= (1 << OCIE2A);    //Interrupt fires when counter matches OPR2A value above.

	sei();         //enable interrupts  
	// in atmel s7 sei() is flagged as red squiggle due to 
	//"intellisence" beautifying,but will still compile. Doh!


    //TCCR2B  0 0 0 turns off clock, don't use!
	// CS2  CS1   CS0
	// 0     0     1  no prescale
	// 0     1     0  divide by 8
	// 1     0     0  divide by 64
	
    TCCR2B &= ~(1 << CS02); //CS2 to 0
	TCCR2B &= ~(1 << CS01);
	TCCR2B |=  (1 << CS00);
	// set preschooler to 64
		
   // ********END TIMER--WAVEFORM************	


// boot up fun with panel LEDs
    PORTD &= ~(1 << 5);     // LED1 off
    PORTD &= ~(1 << 6);    // LED2 off
    PORTD &= ~(1 << 7);    // LED3 off


    PORTD |= (1 << 5);     // turn on LEDs and wait
    _delay_ms(500);
	PORTD |= (1 << 6);
	_delay_ms(500);
	PORTD |= (1 << 7);
	_delay_ms(500);	
	
	
while(1)
		{
	   speed = PIND;
	   if ((speed & 0b00000001) == 0x00)
		  {
		  OCR0A = 0xFF;
		  }
	   else
		  {
		  OCR0A = 0x01;
		  } 
	    
		CV = analogRead10bit();	    
			//ramp(CV);
			//tri(CV);
		
		if (waveselect == 0)
				{
				squarewave(CV);
				}
		
		if (waveselect == 1)
        		{
				ramp(CV);
        		}
		if (waveselect == 2)	 	
				{
				tri(CV);	
				}
		}  // end while(1)
} // end main


void ramp(uint16_t freq)
{
    PORTD &= ~(1 << 5);     // LED1 off
	PORTD |= (1 << 6);    // LED2 on
	PORTD &= ~(1 << 7);    // LED3 off
	uint16_t rate = 0;

	rate = 1024-freq;

	

		if (c >= rate)
		  {
			 WFMFREQ = 12*(freq/100);
			   //next 2 lines for debug   
              //printf("ramp rate: %d",rate);
             // printf("ramp ccccc: %d",c); 	      
       
			 count = count + WFMFREQ;
	         uint8_t CMSB = count >> 8;
			 uint8_t CLSB = count & 0xFF;
		     write4921(CMSB,CLSB);
			 count++;
			 
			 
			 //next 2 lines for debug
			// printf("ramp count: %d \n\r",count);
			// printf("ramp bytes %x %x \n\r",CMSB,CLSB);
		     c = 0; 
             if (count > 4095)
             {
	             count = 0;
             }		 	  
		  }
		
	  
}

void squarewave(uint16_t freq)
{
		 PORTD &= ~(1 << 5);     // LED1 off
		 PORTD &= ~(1 << 6);    // LED2 off
		 PORTD |= (1 << 7);    // LED3 on
	uint16_t rate = 0;

	rate = 1024-freq;

	WFMFREQ = freq/3;

	if (c >= rate)
	{
		
		
		count = count + WFMFREQ;
		if (count > 2047)
		{

		write4921(0,0);
		
		}
		
		else
		{
		write4921(0x0F,0xFF);	
		}
		count++;
		//next 2 lines for debug
		// printf("sqwave count: %d \n\r",count);
		// printf("sqwave bytes %x %x \n\r",CMSB,CLSB);
		c = 0;
		if (count > 4096-WFMFREQ)
		{
			count = 0;
		}
	}
	
	
	if (c >= 25000)
	{
		c = 0;
	}

	
}

 void tri(uint16_t freq)
 {
	    // set pin 3 of Port B as output
	 PORTD |= (1 << 5);     // LED1 on
	 PORTD &= ~(1 << 6);    // LED2 off
	 PORTD &= ~(1 << 7);    // LED3 off
	 uint16_t rate = 0;
     
	 rate = 1024-freq;
     WFMFREQ = 8*(freq/100);

	 if (c >= rate)
	 {
	 
		 //next 2 lines for debug
		 //printf("input rate: %d",rate);
		 // printf("tri ccccc: %d",c);
		 
		 if (count < 4096 + WFMFREQ)
		    {
		    count = count + WFMFREQ;	
			uint8_t CMSB = count >> 8;
			uint8_t CLSB = count & 0xFF;
			write4921(CMSB,CLSB);
			count++;
			  	
			}
		

		 if (count >= 4096 + WFMFREQ)
			{		     
            
			 count = count + WFMFREQ;
			  
			 uint16_t triout = 4096 - (count - 4096);
			 uint8_t CMSB = triout >> 8;
			 uint8_t CLSB = triout & 0xFF;
			 write4921(CMSB,CLSB);
             count++;
    			
		    			}
         if (count > 8192 + WFMFREQ)
		 {
			
	         count=0;
	         
		 }
		 c = 0;

	 }
	 
 }
 