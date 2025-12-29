/*
 * downloadable character routines for Poly emulator
 *
 * v5.0:  moved all video BIOS call stuff
 *        into inline assembler in this file.
 */
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <alloc.h>
#include <ctype.h>
#include "pmhdr.h"

U16 cdseg, cdoff;
U8 far *cdptr;
S16 nrows;
S16 vgaused = 0;


/*
 * externals
 */
extern U8 grtbl[];


/*
 * Load a font from the named file.
 */
void loadfont( char *file )
	{
	FILE *fp;
	char inbuf[80];
	U8 map[256], v, far *p;
	U16 bytecount, maxbytes;
	S16 i;

	if ((fp = fopen(file, "rt")) == NULL)
		{
		cprintf("Can't open font file %s\r\n", file);
		return;
		}

	if (fgets(inbuf, sizeof(inbuf), fp) == NULL ||
		sscanf(inbuf, "%d", &nrows) != 1)
		{
		cprintf("Can't read # rows from font file!\r\n");
		fclose(fp);
		return;
		}

	if (nrows != 14 && nrows != 16)
		{
		cprintf("Invalid # of rows in font file: %d\r\n", nrows);
		fclose(fp);
		return;
		}

	for (i = 0; i < 256; i += 16)
		{
		if (fgets(inbuf, sizeof(inbuf), fp) == NULL ||
			get16args(inbuf, map + i) != 16)
			{
			cprintf("Can't get character map from font file!\r\n");
			fclose(fp);
			return;
			}
		}

	maxbytes = nrows * 256;
	cdptr = farcalloc(maxbytes, 1);
	p = cdptr;

	/* Read the rest of the file (font definitions) and convert to bytes.
	 */
	bytecount = 0;
	while (fgets(inbuf, sizeof(inbuf), fp) != NULL)
		{
		if (strlen(inbuf) <= 1)
			continue;		/* empty line */

		if (++bytecount > maxbytes)
			{
			cprintf("Too many lines in font file!\r\n");
			farfree(cdptr);
			fclose(fp);
			return;
			}

		v = 0;
		for (i = 0; i < 8; ++i)
			if (inbuf[i] == 'X')
				v |= 1 << (7 - i);
		*p++ = v;
		}

	/* Load characters into VGA.
	 */
	cdseg = FP_SEG(cdptr);
	cdoff = FP_OFF(cdptr);
//	vgachar();
	vgaused = 1;

	asm {
		mov		bx,1000h	// rows, table #
		push	bp
		mov		ax,1100h	// fcn/subfcn
		mov		cx,256		// how many to write
		mov		dx,0		// what ASCII code
		mov		es,[cdseg]		// point es:bp to bitmap
		mov		bp,[cdoff]
		int		10h
		pop		bp
	}

	/* Load map into grtbl.
	 * This array maps the byte value in memory into the proper
	 * value to display on screen.  When a VGA card is in use,
	 * it's basically just flipping the high bit.
	 */
	memcpy((char *)grtbl, map, 256);

	/* Cleanup and return.
	 */
	atexit(undofont);
	farfree(cdptr);
	fclose(fp);
	}


/*
 * Scan 16 args from inbuf, store as UCHARs at *mapp++.
 */
S16 get16args( char *inbuf, U8 *mapp )
	{
	S16 nargs = 0, i, n;

	for (i = 0; i < 16; ++i)
		{
		if (sscanf(inbuf, "%x", &n) == 1)
			{
			*mapp++ = n;
			++nargs;
			while (isxdigit(*inbuf))
				++inbuf;
			while (isspace(*inbuf))
				++inbuf;
			}
		else
			break;
		}

	return (nargs);
	}


/*
 * Undo a font, if we did one.
 */
void undofont( void )
	{

	if (vgaused)
		{
		asm {
		// Reload the ROM-based characters into table 0.
			mov		ax,1114h	// fcn/subfcn
			mov		bx,0
			int		10h

		// Set the display into video mode 3, normal 80x25 text.
		// This cleans up any weirdness left by setting/unsetting
		// the characters.

			mov		ax,0003h	// set mode 3
			int		10h
			}
		}
		//oldvga();

	}
