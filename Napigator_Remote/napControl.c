// Remote control for my bed
// Todd Michael Bailey
// Fri Mar 14 20:30:47 EDT 2014
// For the lulz


#include	"includes.h"

//=============================
// Atmel AVR Atmega168a MCU, 5v operation.
// Internal oscillator
// avr-gcc (AVR_8_bit_GNU_Toolchain_3.4.2_939) 4.7.2
//==============================

/*
Description:
==============================================================================
Lifts my bed up to the ceiling via some bootleg Harbor Freight winches.

This project is three units -- one transmitter / remote and two receivers which control relays (one per winch).
The idea right now is that the receivers are pretty dumb and the transmitter does the thinking.
This is so we can make adjustments to the functionality without climbing up a ladder, although it does mean that
inconsistent IR will make jittery operation.



REMOTE:
--------------------------------
Sends IR to the receivers.
The Remote is responsible for timing the operation and generally doing all the thinking.
This makes it easy to change up/down timings without climbing a ladder but does mean interrupting the IR will stop the bed.

6 buttons:
Jog Front Motor Up
Jog Front Motor Down
Jog Rear Motor Up
Jog Rear Motor Down
RUN THE JEWELS
DROP THE BASS


Bitmask:
(Each packet is has only one data byte)
------------------------------------------
Bit 7:		unused
Bit 6:		Controller 1 command MSb
Bit 5:		Controller 1 command LSb
Bit 4:		unused
Bit 3:		unused
Bit 2:		Controller 0 command MSb
Bit 1:		Controller 0 command LSb
Bit 0:		unused

Commands:
-----------
0x03	Stop motor
0x02	Motor up for 1/4 sec
0x01	Motor down for 1/4 sec
0x00	Stop motor

*/

// NOTES:
// ==============================================================================
// In case you forgot, here's the command line for grepping through old projects
// and sorting them by date:
//
// grep -ilr 168p ./* | xargs ls -altr
// This will return files that have "168p" in them, with the most recent at the bottom.
// 
// find ./* -name eeprom.c | xargs ls -altr
// This will find files named eeprom.c in a similar way.
// 
// Fuse bits:
// EByte:  Don't change.
// HByte:  Was 0xDF, make 0xDE.  This adds a low level BOD -- we could make this 0xD6 if we didn't want to erase user settings in eeprom
// LByte:  Was 0xD6. Keep it there.



//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
//Application Defines:
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

// The two controllers care about different bit fields in the command byte.
// Could do this with an IFDEF but we do it this way.

#define		CONTROLLER_0		0
#define		CONTROLLER_1		1

#define		COMMAND_MOTOR_UP	0x02
#define		COMMAND_MOTOR_DOWN	0x01

#define		MOTOR_UP			true
#define		MOTOR_DOWN			false


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Application Globals:
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

static unsigned char
	keyState,
	newKeys;

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Interrupt Service Routines:
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Variables messed with in the ISRs AND mainline code should be declared volatile.
// The functions declared up here should _ONLY_ ever be called from an interrupt.

ISR(__vector_default)
{
    //  This means a bug happened.  Some interrupt that shouldn't have generated an interrupt went here, the default interrupt vector.
	//	printf("Buggy Interrupt Generated!  Flags = ");
	//  printf("*****put interrupt register values here****");
}

ISR(PCINT1_vect)
// The pin change interrupt vector.  Used to wake the device from sleep.
{

}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// State Machine Functions.
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

static void SetState(STATE_FUNC *newState)		// Sets the device to a new state, assumes it should begin at the first minor sub-state.
{
	State=newState;
	subState=SS_0;
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Switch Functions.
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


static void InitSwitches(void)
{
	DDRC&=~(0x3F);		// Switches to inputs.
	PORTC|=(0x3F);		// Pullups on.	

	newKeys=0;			// Nothing special happening yet.
	keyState=0;
	SetTimer(TIMER_DEBOUNCE,(SECOND/32));		// Reset debounce timer.	
}

static void HandleSwitches(void)
// Get us info about which user switches are being pressed and which ones are new presses.
// Run this from the main loop.
{
	static unsigned char
		lastKeyState;

	lastKeyState=keyState;

	if(CheckTimer(TIMER_DEBOUNCE))
	{
		SetTimer(TIMER_DEBOUNCE,(SECOND/32));		// Reset debounce timer.
		keyState=(PINC&0x3F);						// Read in our switches (mask off switch bits).
		keyState=((~keyState)&0x3F);				// Invert the keyState so that a pressed switch is a 1 (true).  Also mask off all the bits that aren't switches in this app.
	}

	newKeys=((keyState^lastKeyState)&(keyState));			// Flag the keys which have been pressed since the last test.  Keys are only "new" for one loop.
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Local Software Clock stuff.
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

void HandleSoftclock(void)
{
	if(TIFR2&(1<<TOV2))		// Got a timer overflow flag?
	{
		TIFR2 |= (1<<TOV2);		// Reset the flag (by writing a one).
		systemTicks++;			// Increment the system ticks.
	}
}

static void InitSoftclock(void)
// The timer in this application makes system ticks by polling an overflow flag on a hardware timer.
// This is potentially more troublesome than an interrupt (we might miss if we hang in the code for more than a tick time) but it does not steal cycles from other more important ISRs
// At 8MHz, 8 bit counter overflow, prescaler=1/128, a tick is about 4.1mSecs.  This is over 200 days for a 32-bit tick counter.
{
	PRR&=~(1<<PRTIM2);	// Turn the TMR2 power on.
	TIMSK2=0x00;		// Disable all Timer 0 associated interrupts.
	TCCR2A=0;			// Normal Ports.
	TCNT2=0;			// Initialize the counter to 0.
	TIFR2=0xFF;			// Clear the interrupt flags by writing ones.
	systemTicks=0;
	TCCR2B=0x05;		// Start the timer in Normal mode, prescaler at 1/128
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Outgoing IR stuff
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

#define		RUN_FULL_UP_TIME		(SECOND*36)
#define		RUN_FULL_DOWN_TIME		(SECOND*36)

static unsigned char
	irOutState;

enum		// Things the IR can be doing
	{
		IR_OUT_IDLE=0,
		SEND_FULL_UP,
		SEND_FULL_DOWN,
		SEND_JOG_FRONT_UP,
		SEND_JOG_FRONT_DOWN,
		SEND_JOG_REAR_UP,
		SEND_JOG_REAR_DOWN,	
	};

static void UpdateOutgoingIr(void)
// Keep the IR going out as needed.
// Since the IR LED takes a TON of power, keep it transmitting on a clock.
// Each IR packet takes about 33mS, right now we send one every 83mS or so.
// We could just blaze them out if necessary.
// The winches run for 1/3sec every time we get a packet so we can miss a few and still get consistent behavior.
{
	unsigned char
		outgoingFrame;

	if(CheckTimer(TIMER_IR_OUTPUT))		// Send out packets on a clock.
	{
		ResetTimer(TIMER_IR_OUTPUT);
		
		switch(irOutState)
		{
			case SEND_FULL_UP:
				if(CheckTimer(TIMER_MOTOR_RUN))		// Done moving?
				{
					irOutState=IR_OUT_IDLE;
				}
				else
				{
					outgoingFrame=0x44;				// Both motors up.  See codes above.
					IrTxFrame(1,&outgoingFrame);
				}
				break;
			
			case SEND_FULL_DOWN:
				if(CheckTimer(TIMER_MOTOR_RUN))		// Done moving?
				{
					irOutState=IR_OUT_IDLE;
				}
				else
				{
					outgoingFrame=0x22;				// Both motors down.  See codes above.
					IrTxFrame(1,&outgoingFrame);
				}
				break;
			
			case SEND_JOG_FRONT_UP:
				outgoingFrame=0x40;					// First motor up.  See codes above.
				IrTxFrame(1,&outgoingFrame);
				irOutState=IR_OUT_IDLE;				// And don't do it again unless a switch is held.
				break;
			
			case SEND_JOG_FRONT_DOWN:
				outgoingFrame=0x20;					// First motor down.  See codes above.
				IrTxFrame(1,&outgoingFrame);
				irOutState=IR_OUT_IDLE;				// And don't do it again unless a switch is held.
				break;

			case SEND_JOG_REAR_UP:
				outgoingFrame=0x04;					// Second motor up.  See codes above.
				IrTxFrame(1,&outgoingFrame);
				irOutState=IR_OUT_IDLE;				// And don't do it again unless a switch is held.
				break;
			
			case SEND_JOG_REAR_DOWN:
				outgoingFrame=0x02;					// Second motor down.  See codes above.
				IrTxFrame(1,&outgoingFrame);
				irOutState=IR_OUT_IDLE;				// And don't do it again unless a switch is held.
				break;

			case IR_OUT_IDLE:						// Don't do anything.
			default:
				PORTD&=~(1<<PD3);					// Blingy LED off
				break;
		}
	}
}

static void InitOutgoingIr(void)
// Set up timers, pins, and state machines.
{
	irOutState=IR_OUT_IDLE;
	ExpireTimer(TIMER_MOTOR_RUN);
	SetTimer(TIMER_IR_OUTPUT,(SECOND/12));
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Program main loop.
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

static void DoRemote(void)
// Handle the incoming IR and set the motors accordingly
{
	if(newKeys&Im_FULL_UP)
	{
		SetTimer(TIMER_MOTOR_RUN,(RUN_FULL_UP_TIME));
		irOutState=SEND_FULL_UP;
		PORTD|=(1<<PD3);		// Blingy LED on
	}
	else if(newKeys&Im_FULL_DOWN)
	{
		SetTimer(TIMER_MOTOR_RUN,(RUN_FULL_DOWN_TIME));
		irOutState=SEND_FULL_DOWN;	
		PORTD|=(1<<PD3);
	}
	else if(keyState&Im_JOG_FRONT_UP)
	{
		ExpireTimer(TIMER_MOTOR_RUN);
		irOutState=SEND_JOG_FRONT_UP;	
		PORTD|=(1<<PD3);
	}
	else if(keyState&Im_JOG_FRONT_DOWN)
	{
		ExpireTimer(TIMER_MOTOR_RUN);
		irOutState=SEND_JOG_FRONT_DOWN;	
		PORTD|=(1<<PD3);
	}
	else if(keyState&Im_JOG_REAR_UP)
	{
		ExpireTimer(TIMER_MOTOR_RUN);
		irOutState=SEND_JOG_REAR_UP;	
		PORTD|=(1<<PD3);
	}
	else if(keyState&Im_JOG_REAR_DOWN)
	{
		ExpireTimer(TIMER_MOTOR_RUN);
		irOutState=SEND_JOG_REAR_DOWN;	
		PORTD|=(1<<PD3);
	}
}

static void DoSleep(void)
// Put the part to sleep here and wait until a pin change interrupt to wake up.
// Pins we care about are PC0-PC5 (PCINT 8-13)
{		
	unsigned char
		sreg;
		
	sreg=SREG;	// Store global interrupt state
	cli();		// Clear global interrupts.
	
	// Un init IR, turn off blingy LED
	UnInitIr();
	PORTD&=~(1<<PD3);		// Blingy LED off
	DDRD|=(1<<PD3);			// Blingy LED to output

	// Setup PCINTs and put the part to sleep.
	// PC0-PC5 are PCINT8-13 (PCIE1) 
	
	PCICR=(1<<PCIE1);		// Enable the pin change interrupt for PORTC (PCINTS 8-14)
	PCMSK1=0x3F;			// PC0-PC5 generate interrupts, please (PCINTS 8-13)

	sei();					// Enable interrupts so we can wake from sleep.
				
	SMCR=0x05;					// Enable sleep and set the sleep mode to "Power Down".
	asm volatile("sleep"::);	// Go to sleep.  We'll wake up on a level change on a switch pin.

	// If we get here the part has woken up due to a pin change.

	InitIr();
	InitOutgoingIr();

	PCICR=0;			// No global PCINTS.
	PCMSK1=0;			// No PORTC interrupts enabled.
	SMCR=0;				// Disable sleep.				

	SREG=sreg;							// Restore interrupts.
}

int main (void)
// Initialize this mess.
{
	PRR=0xFF;				// Power off everything, let the initialization routines turn on modules you need.
	MCUCR&=~(1<<PUD);		// Make sure we can turn pullups on.

	CLKPR=0x80;		// This two byte combination undoes the CLKDIV setting and sets the clock prescaler to 1,
	CLKPR=0x00;		// giving us a clock frequency of 8MHz (We boot at 1MHz).

	PORTD=0xBF;				// Pullup for less noise, IR LED off
	DDRD=(1<<PD6);			// PD6 is IR LED out

	PORTD&=~(1<<PD3);		// Blingy LED off
	DDRD|=(1<<PD3);			// Blingy LED to output

	DDRC=0x00;		
	PORTC=0xFF;

	DDRB=0x00;		// ISP pins
	PORTB=0xFF;		// Pullup for less noise on unusused inputs

	InitSoftclock();
	InitSwitches();
	InitIr();
	InitOutgoingIr();

	SetTimer(TIMER_INACTIVITY,(SECOND*45));		// Turn off clocks and go to sleep if there isn't any user input.
	
	SetState(DoRemote);
	
	sei();				// THE ONLY PLACE we should globally enable interrupts in this code.

	while(1)
	{
		HandleSoftclock();
		HandleSwitches();
		UpdateOutgoingIr();
		State();

		if(newKeys)				// Go to sleep if nobody does anything for awhile
		{
			ResetTimer(TIMER_INACTIVITY);
		}
		else
		{
			if(CheckTimer(TIMER_INACTIVITY))
			{
				DoSleep();			
				ResetTimer(TIMER_INACTIVITY);		// Woke because of keypress, reset inactivity timer.
			}
		}
	}
	return(0);
}

