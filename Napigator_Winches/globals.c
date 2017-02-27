// DEFINITIONS of all global variables (anything you want to declare "extern") should go here and only here just to keep things straight.
//--------------------------------------------------------------------------------------

#include "includes.h"

STATE_FUNC				//  Creates a pointer called State to an instance of STATE_FUNC().
	*State;
unsigned char
	subState;			//  Keeps track of the minor states (sub states) the device can be in.
volatile unsigned long	// This counter keeps track of human time and is used by the softclock
	systemTicks;

