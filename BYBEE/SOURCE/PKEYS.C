/*
 * PM - keyboard routines
 * 9/20/89 rdb
 */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <bios.h>
#include "pmhdr.h"
#include "keycodes.h"



/*
 * IBM-to-Poly keyboard translation
 */
U16 polykeys[][2] =
	{
	{KEY_DELETE,	0x7f},		/* map DEL and BS to Poly DEL */
	{KEY_BS,		0x7f},

	{KEY_UP,		0x11},		/* the 4 cursor keys */
	{KEY_DOWN,		0x12},
	{KEY_LEFT,		0x14},
	{KEY_RIGHT,		0x13},

	{KEY_PGUP,		0x10},		/* ^p for editor */
	{KEY_PGDN,		0x0e},		/* ^n for editor */
	{KEY_CPGUP,		0x02},		/* ^b for editor */
	{KEY_CPGDN,		0x05},		/* ^e for editor */

	{KEY_F1,		0x1c},		/* Poly key I   = ^\ */
	{KEY_F2,		0x1d},		/* Poly key II  = ^] */
	{KEY_F3,		0x1e},		/* Poly key III = ^^ */
	{KEY_F4,		0x1f},		/* Poly key IV  = ^_ */
	{KEY_F5,		0x0a},		/* line-feed */
	{KEY_F6,		0x08},		/* backspace */

	{0, 0}
	};


/*
 * Check for a keyboard hit.
 */
void check_keyb( void )
	{
	U16 key, pk, i;

	if (bioskey(1) == 0)
		return;

	gotkbhit = 0;
	key = bioskey(0);
	if (key == KEY_AF1)		/* if Alt-F1 hit, break emulation */
		{
		running = 0;
		return;
		}

	for (i = 0; ; ++i)
		{
		if ((pk = polykeys[i][0]) == 0)
			break;

		if (key == pk)
			{
			key = polykeys[i][1] | 0x0100;
			break;
			}
		}

	if ((key & 0xff00) != 0)
		{
		kbport = key;		/* pass lo byte as key hit */
		setimask(INT_KB);	/* tell emulator to interrupt */
		}
	}
