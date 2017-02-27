// Winch controls for my bed
// Todd Michael Bailey
// Wed Mar 12 20:39:14 EDT 2014
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

Receiver / Relay Units:
--------------------------------
Stay awake and receiving IR.
When we get a good packet, engage the right relay for the correct amount of time.
Each packet is has only one data byte.

Bitmask:
---------------------------
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

//#define		CONTROLLER_ID		CONTROLLER_0
#define		CONTROLLER_ID		CONTROLLER_1

#define		COMMAND_MOTOR_UP	0x02
#define		COMMAND_MOTOR_DOWN	0x01

#define		MOTOR_UP			true
#define		MOTOR_DOWN			false

#define		MOTOR_PAUSE_TIME	(SECOND/10)		// Time for the relays to fully switch off before we put them in a new state.
#define		MOTOR_RUN_TIME		(SECOND/3)		// Time we will run after getting a single IR packet.

enum
	{
		MOTOR_STATE_IDLE=0,
		MOTOR_STATE_GOING_UP,
		MOTOR_STATE_GOING_DOWN,
		MOTOR_STATE_PAUSE_BEFORE_UP,
		MOTOR_STATE_PAUSE_BEFORE_DOWN,
		MOTOR_STATE_PAUSE_BEFORE_IDLE,
	};

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Application Globals:
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

static unsigned char
	motorState;

//static unsigned char
//	keyState,
//	newKeys;

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

/*
static void InitSwitches(void)
{
	DDRC&=~(0x0F);		// Switches to inputs.
	PORTC|=(0x0F);		// Pullups on.	

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
		keyState=(PINC&0x0F);						// Read in our switches (mask off switch bits).
		keyState=((~keyState)&0x0F);				// Invert the keyState so that a pressed switch is a 1 (true).  Also mask off all the bits that aren't switches in this app.
	}

	newKeys=((keyState^lastKeyState)&(keyState));			// Flag the keys which have been pressed since the last test.  Keys are only "new" for one loop.
}
*/

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
// Motor and Relay Stuff
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

static void StopMotor(void)
// Turn all relays off right now.
{
	DDRC|=((1<<PC3)|(1<<PC4)|(1<<PC5));
	PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));	// Make sure motors are off
	motorState=MOTOR_STATE_IDLE;			// Stop the state machine
	ExpireTimer(TIMER_MOTOR);				// Reset motor timer
}

static void RunMotor(bool theDirection)
// Make the motor go in the passed direction (up or down).
// IF the motor changes direction, introduce a delay so we don't
// have a state where too many relays are engaged.
{
	if(motorState==MOTOR_STATE_IDLE)	// OK to immediately engage motor
	{
		if(theDirection==MOTOR_UP)
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			PORTC|=((1<<PC4)|(1<<PC5));						// UP is relays 1 and 2
			motorState=MOTOR_STATE_GOING_UP;
			SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);
		}
		else if(theDirection==MOTOR_DOWN)
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			PORTC|=((1<<PC3)|(1<<PC4));						// DOWN is relays 0 and 1
			motorState=MOTOR_STATE_GOING_DOWN;
			SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);		
		}
	}
	else if(motorState==MOTOR_STATE_GOING_UP)		// Already going up?
	{
		if(theDirection==MOTOR_UP)					// Just keep timer going	
		{
			SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);
		}
		else if(theDirection==MOTOR_DOWN)			// Direction change.
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_DOWN;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}		
	}
	else if(motorState==MOTOR_STATE_GOING_DOWN)		// Already going down?
	{
		if(theDirection==MOTOR_DOWN)					// Just keep timer going	
		{
			SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);
		}
		else if(theDirection==MOTOR_UP)			// Direction change.
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_UP;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}		
	}
	else if(motorState==MOTOR_STATE_PAUSE_BEFORE_UP)		// Waiting to go up?
	{
		if(theDirection==MOTOR_UP)					// Do nothing, keep waiting to go up.
		{
		}
		else if(theDirection==MOTOR_DOWN)			// Direction change.
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_DOWN;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}		
	}
	else if(motorState==MOTOR_STATE_PAUSE_BEFORE_DOWN)		// Waiting to go down?
	{
		if(theDirection==MOTOR_DOWN)					// Do nothing, keep waiting.
		{
		}
		else if(theDirection==MOTOR_UP)			// Direction change.
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_UP;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}		
	}
	else if(motorState==MOTOR_STATE_PAUSE_BEFORE_IDLE)		// Motors just stopped, wait to re-engage
	{
		if(theDirection==MOTOR_DOWN)					// Direction change
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_DOWN;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}
		else if(theDirection==MOTOR_UP)			// Direction change.
		{
			PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
			motorState=MOTOR_STATE_PAUSE_BEFORE_UP;
			SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
		}		
	}
}

static void UpdateMotor(void)
// Keep the motors going.  Keep tabs on delays.
// NOTE -- the Panasonic ALE75F05 (which got replaced by the ALE7P05) is non polar.
// It claims a worst case operate time of 25mS, WITH DIODE and EXCLUDING CONTACT BOUNCE.
// So really, we probably ought to give it significantly more than that as far as a turnaround delay.
// (While we're screwing around, the 5v relay consistently operates with a 3.5v input, but not below)
// (and it draws 40mA at 5v.  The HOLD voltage is really low though, only a volt or so)
// (NOTE: read the DS dummy, this is consistent)
{
	switch(motorState)
	{
		case MOTOR_STATE_GOING_UP:
		case MOTOR_STATE_GOING_DOWN:
			if(CheckTimer(TIMER_MOTOR))		// Just keep track of stopping motors when we run out of time.
			{
				StopMotor();
				motorState=MOTOR_STATE_PAUSE_BEFORE_IDLE;		// Allow decay time.  We COULD get a command immediately after this before a relay has finished switching.
				SetTimer(TIMER_MOTOR,MOTOR_PAUSE_TIME);		
			}
			break;
		case MOTOR_STATE_PAUSE_BEFORE_UP:	// We're stopped and waiting to go up.
			if(CheckTimer(TIMER_MOTOR))
			{
				PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
				PORTC|=((1<<PC4)|(1<<PC5));						// UP is relays 1 and 2
				motorState=MOTOR_STATE_GOING_UP;
				SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);				
			}
			break;
		case MOTOR_STATE_PAUSE_BEFORE_DOWN:	// Stopped and waiting for relays to disengage before we go down.
			if(CheckTimer(TIMER_MOTOR))
			{
				PORTC&=~((1<<PC3)|(1<<PC4)|(1<<PC5));			// Make sure motors are off
				PORTC|=((1<<PC3)|(1<<PC4));						// DOWN is relays 0 and 1
				motorState=MOTOR_STATE_GOING_DOWN;
				SetTimer(TIMER_MOTOR,MOTOR_RUN_TIME);		
			}
			break;
		case MOTOR_STATE_PAUSE_BEFORE_IDLE:
			if(CheckTimer(TIMER_MOTOR))		// Just keep track of stopping motors when we run out of time.
			{
				StopMotor();
				motorState=MOTOR_STATE_IDLE;
			}
			break;
		default:			// MOTOR_IDLE and anything buggy.
			StopMotor();
			break;
	}
}

static void InitMotor(void)
// Set up timers, pins, and state machines.
{
	StopMotor();
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
// Program main loop.
//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
unsigned char
	incomingDataLength;
unsigned char
	incomingDataBytes[16];

static void DoWinchControl(void)
// Handle the incoming IR and set the motors accordingly
{
	unsigned char
		incomingCommand;

	if(subState==SS_0)					// Allow readings to settle as power comes up
	{
		if(IrCheckRxFrame(&incomingDataLength))					// New frame received?  Datalength to ALL bytes received (not just data field)
		{
			IrRxFrame(incomingDataLength,incomingDataBytes);	// read in the frame

			if(CONTROLLER_ID==CONTROLLER_0)		// Should compile conditionally
			{
				incomingCommand=(incomingDataBytes[0]>>1)&0x03;	// Shift bits this controller cares about into position.
			}
			else
			{
				incomingCommand=(incomingDataBytes[0]>>5)&0x03;
			}

			switch(incomingCommand)
			{
				case COMMAND_MOTOR_UP:
					RunMotor(MOTOR_UP);
					break;
				case COMMAND_MOTOR_DOWN:
					RunMotor(MOTOR_DOWN);
					break;
				default:
					StopMotor();
					break;
			}
		}
	}
}

int main (void)
// Initialize this mess.
{
	PRR=0xFF;				// Power off everything, let the initialization routines turn on modules you need.
	MCUCR&=~(1<<PUD);		// Make sure we can turn pullups on.

	CLKPR=0x80;		// This two byte combination undoes the CLKDIV setting and sets the clock prescaler to 1,
	CLKPR=0x00;		// giving us a clock frequency of 8MHz (We boot at 1MHz).

	PORTD=0xBF;		// Pullup for less noise, IR LED off
	DDRD=(1<<PD6);	// PD6 is IR LED out

	PORTC=0xC7;		// Pull up for less noise on unused, and relays off
	DDRC=0x38;		// Port C -- bits 3, 4, 5 are relays

	DDRB=0x00;		// ISP pins, IR input
	PORTB=0xFE;		// Pullup for less noise on unused pins

	InitMotor();
	InitSoftclock();
//	InitSwitches();
	InitIr();

	SetState(DoWinchControl);
	
	sei();				// THE ONLY PLACE we should globally enable interrupts in this code.

	while(1)
	{
		HandleSoftclock();
//		HandleSwitches();
		UpdateMotor();
		State();
	}
	return(0);
}

