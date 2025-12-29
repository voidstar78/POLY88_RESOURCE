/*
 * New clock() function, to replace Borland's.
 * This one avoids calling INT 1A, so does not disturb the
 * interaction between DOS and BIOS clocks.
 *
 * Bob Bybee, 5/94
 */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#include <dos.h>

#define TICKS_PER_DAY 0x1800b0L
#define BIOS_TICKS (*(long *)MK_FP(0x40, 0x6c))

long clock( void )
	{
	long ticks;
	static long day_ticks = 0, start_ticks = -1L, last_ticks = 0;

	ticks = BIOS_TICKS;
	if (start_ticks == -1L)
		start_ticks = ticks;

	if (ticks < last_ticks)
		{
		day_ticks += TICKS_PER_DAY;
		_AH = 0x2a;				/* get DOS date to force DOS */
		geninterrupt(0x21);		/*  to recognize date change */
		}

	last_ticks = ticks;
	return (ticks + day_ticks - start_ticks);
	}

#pragma startup clock	/* call him at startup, to set start_ticks */
