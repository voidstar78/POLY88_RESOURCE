/*
 * psyscall - System Calls for PolyMorphic emulator
 * 9/20/89 rdb
 */
#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include <dos.h>
#include <dir.h>
#include <conio.h>
#include <bios.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <mem.h>
#include "pmhdr.h"

void setrtc( char *flag );
void setpoly88( char *flag );


char diskname[8][80] =
	{
	"",
	"system.pm",
	"",
	"",
	"",
	"",
	"",
	"",
	};

char copymode = 'B';
int xfer_fh = -1;
char xfer_name[80];

#define MAXCOM 2			/* largest COM port supported (1..n) */
#define MAXLPT 3			/* largest LPT port supported */
S16 lpt_num = -1;			/* no LPT device */
S16 com_num = -1;			/* no COM port either */
char printer_fname[80];		/* for sending printer output to DOS file */
int printer_fh = -1;


/*
 * Perform a system call from the 8080 emulator.
 * System calls in RAM are opcodes of the form ED ED nn.
 * _syscall contains the type, nn.
 */
void systemcall( void )
	{

	if (breaksys_before == syscall)
		{
		cprintf("Break before system call %d\r\n", syscall);
		getch();
		exit(1);
		}

	switch (syscall)
		{
		case 1:		/* Dio */
			dio();
			ram[DONT] = 0;
			iflag = 1;
			break;

//		case 2:		/* printer driver */
//			sysprint();
//			break;

		case 3:		/* MOVE call */
			sysmove();
			break;

		case 4:		/* Ioret call */
			sysioret();
			break;

		case 5:		/* Display call */
			sysvti();
			break;

		case 6:
			sysnames();
			break;

		case 7:
			sysecho(1);
			break;

		case 8:
			syssize();
			break;

		case 9:
			sysconn();
			break;

		case 10:
			sysdos();
			break;

		case 11:
			sysprintnb();
			break;

		case 12:		/* warning msg, no auto-newline */
			sysecho(0);
			break;

		case 13:
			sysclrline();
			break;

		case 14:
			sysparse();
			break;

		case 15:
			systext();
			break;

		case 16:
			syswrit();
			break;

		case 17:
			sysclos();
			break;

		case 18:
			sysread();
			break;

		case 19:
			syspdev();
			break;

		case 20:
			sysrdprt();
			break;

		case 21:
			sysmathop();
			break;

//		case 22:
//			sysreadtape();
//			break;

		default:
			cprintf("Illegal system call: %d!\r\n", syscall);
			pcreg -= 3;
			dumpregs();
			exit(1);
			break;
		}

	if (breaksys_after == syscall)
		{
		cprintf("Break after system call %d\r\n", syscall);
		getch();
		exit(1);
		}



	syscall = 0;
	}


/*
 * Initialize device assignments from POLYDEV.DAT.
 * The file contains lines like:
 *		drive 1 system.pm
 *		printer LPT2
 */
void initdev( S16 dloaded )
	{
	FILE *fp;
	S16 drive, n, nargs, ok;
	char *p, inbuf[200], cmd[200], arg1[80], arg2[80];

	if ((fp = fopen("polydev.dat", "rt")) == NULL)
		return;		/* no disk assignments, use default */

	while (fgets(inbuf, sizeof(inbuf) - 2, fp) != NULL)
		{
		if ((p = strchr(inbuf, '\n')) != NULL)
			*p = 0;

		if (*inbuf == ';' || *inbuf == 0)
			continue;

		nargs = sscanf(inbuf, " %s %s %s", &cmd, &arg1, &arg2);
		if (nargs == 0)
			continue;

		ok = 0;
		if (stricmp(cmd, "drive") == 0 &&
			sscanf(arg1, " %d", &drive) == 1 &&
			0 < drive && drive < 8)
			{
			ok = 1;
			if (drive > dloaded)
				strcpy(diskname[drive], arg2);
			}
		else if (stricmp(cmd, "colors") == 0)
			{
			ok = setcolors(inbuf);
			}
		else if (stricmp(cmd, "font") == 0)
			{
			ok = 1;
			loadfont(arg1);
			}
		else if (stricmp(cmd, "fpb") == 0)
			{
			ok = 1;
			setfpb(arg1);
			}
		else if (strcmp(cmd, "match") == 0)
			{
			if (strcmp(arg1, "on") == 0)
				{
				ok = 1;
				match_flag = 1;
				match_auto = 1;
				match_delays = 15;		// initial try
				}
			else if (sscanf(arg1, "%ld", &match_delays) == 1)
				{
				ok = 1;
				match_flag = 1;
				match_auto = 0;
				}
			}
		else if (stricmp(cmd, "artc") == 0)
			{
			ok = 1;
			setrtc(arg1);
			}
		else if (stricmp(cmd, "poly88") == 0)
			{
			ok = 1;
			setpoly88(arg1);
			}
		else if (stricmp(cmd, "printer") == 0)
			{
			if (printer_fh > -1)
				{
				close(printer_fh);
				printer_fh = -1;
				}

			if (strnicmp(arg1, "NUL", 3) == 0)
				{
				ok = 1;
				lpt_num = -1;
				com_num = -1;
				}
			else if (strnicmp(arg1, "LPT", 3) == 0 &&
				(n = arg1[3] - '0') >= 1 && n <= MAXLPT)
				{
				ok = 1;
				lpt_num = n - 1;	/* LPT1 => 0 */
				com_num = -1;
				}
			else if (strnicmp(arg1, "COM", 3) == 0 &&
				(n = arg1[3] - '0') >= 1 && n <= MAXCOM)
				{
				ok = 1;
				com_num = n - 1;	/* COM1 => 0 */
				lpt_num = -1;
				}
			}

		if (!ok)
			cprintf("Bad line in POLYDEV.DAT: %s\r\n", inbuf);
		}

	fclose(fp);
	}


/*
 * Set the adaptive RTC on/off (default is on)
 */
void setrtc( char *flag )
	{

	if (stricmp(flag, "on") == 0)
		adaptive_rtc_flg = 1;
	else
		adaptive_rtc_flg = 0;
	}


/*
 * Set the Poly 88 flag on/off (default is off)
 */
void setpoly88( char *flag )
	{

	if (stricmp(flag, "on") == 0)
		poly88_flag = 1;
	else
		poly88_flag = 0;
	}




/*
 * Set colors according to four arguments.
 * inbuf is in the form:  colors pfg pbg sfg sbg
 * pfg, pbg are the Poly portion of the screen, foreground/background
 * sfg, sbg are the remainder of the screen, fg/bg
 */
S16 setcolors( char *inbuf )
	{
	char junk[20];
	S16 i, nargs, attr;
	static char cols[4][20] = {"white", "black", "white", "black"};
	static S16 intcols[4] = {7, 0, 7, 0};

	nargs = sscanf(inbuf, "%s %s %s %s %s", junk,
		cols[0], cols[1], cols[2], cols[3]) - 1;
	if (nargs < 1)
		return (0);		/* bad, no colors given */

	for (i = 0; i < nargs; ++i)
		if ((intcols[i] = parse_colors(cols[i])) < 0)
			return (0);

	attr = ((intcols[3] & 7) << 4) | intcols[2];	/* get main screen attr */
	set_win_attr(1, 1, 80, 25, attr);				/* force it to screen */
	textattr(attr);									/* remind turbo of it */
	attr = ((intcols[1] & 7) << 4) | intcols[0];	/* get Poly screen attr */
	set_win_attr(1, 1, 80, 18, attr);				/* force it too */
	return (1);
	}


/*
 * Set attributes for all cells in a window, (x1 y1) to (x2 y2), 1-based.
 */
void set_win_attr( S16 x1, S16 y1, S16 x2, S16 y2, S16 attr )
	{
	S16 x, y;
	U8 far *fp;

	--x1, --y1, --x2, --y2;		/* make 0-based */
	for (y = y1; y <= y2; ++y)
		{
		fp = MK_FP(videoseg, ((y * 80) + x1) * 2 + 1);
		for (x = x1; x <= x2; ++x)
			{
			*fp = attr;
			fp += 2;
			}
		}
	}


/*
 * Parse a color string.  Return 0-15, or -1 if not valid.
 */
S16 parse_colors( char *inbuf )
	{
	S16 i;
	static char color_names[16][10] =
		{
		"black", "blue", "green", "cyan",
		"red", "magenta", "yellow", "white",
		"b-black", "b-blue", "b-green", "b-cyan",
		"b-red", "b-magenta", "b-yellow", "b-white"
		};

	for (i = 0; i < 16; ++i)
		if (stricmp(inbuf, color_names[i]) == 0)
			return (i);

	return (-1);
	}


/*
 * Disk I/O - uses Poly registers as in/out variables.
 */
void dio( void )
	{
	int fh;
	U8 drive, cmd, nsec;
	U16 dnsec, cmdnbytes;
	S32 dpos, dnbytes;

	drive = creg;
	nsec = areg;
	cmdnbytes = nsec << 8;		/* # bytes to read/write */
	freg &= ~C_FLG;				/* clear carry, assume no error */

	cmd = breg;
	if (cmd != 5)
		cmd &= 1;	/* command must be 0, 1, or 5 */

	if (drive < 1 || drive > 7 || nsec == 0)
		{
		freg |= C_FLG;
		dereg = 0x0101;		/* bad parameters */
		return;
		}

	if ((fh = open(diskname[drive], O_RDWR | O_BINARY)) == -1)
		{
		freg |= C_FLG;
		dereg = 0x0106;		/* no disk, or door open */
		return;
		}

	dnbytes = filelength(fh);				/* # bytes on disk */
	dnsec = (unsigned int)(dnbytes >> 8);	/* # sectors on disk */

	if (cmd == 5)		/* disk size */
		{
		close(fh);
		hlreg = dnsec;
		return;
		}

	dpos = 256L * (long)hlreg;
	if (lseek(fh, dpos, SEEK_SET) != dpos || dpos + (long)cmdnbytes > dnbytes)
		{
		close(fh);
		freg |= C_FLG;
		dereg = 0x0101;		/* bad parameters, can't seek or disk too small */
		return;
		}

	if (cmd == 1)
		read(fh, &ram[dereg], cmdnbytes);		/* read */
	else
		write(fh, &ram[dereg], cmdnbytes);		/* write */

	close(fh);
	}


/*
 * Emulate the MOVE call at 100H.
 * Move -BC bytes from (HL)+ to (DE)+.
 */
void sysmove( void )
	{
	do	{
		ram[dereg] = ram[hlreg];
		++dereg;
		++hlreg;
		++bcreg;
		} while (bcreg != 0);
	}


/*
 * Emulate the Ioret call at 0064H.
 */
void sysioret( void )
	{

	hlreg = *(unsigned int *)(ram + spreg);		/* pop h */
	spreg += 2;

	dereg = *(unsigned int *)(ram + spreg);		/* pop d */
	spreg += 2;

	bcreg = *(unsigned int *)(ram + spreg);		/* pop b */
	spreg += 2;

	freg = ram[spreg++];		/* pop psw */
	areg = ram[spreg++];

	iflag = 1;				/* ei */

	pcreg = *(unsigned int *)(ram + spreg);		/* ret */
	spreg += 2;
	}


/*
 * Emulate the display call at 006A (called thru WH1).
 */
void sysvti( void )
	{
	U16 pos, to, from, nchrs, i;
	U8 ch, atmp;


	pos = *(U16 *)(ram + POS);
	ch = areg;

	if (wh1_fp != NULL)
		{
		fputc(ch, wh1_fp);
		if (ch == 0x0d)
			fputc('\n', wh1_fp);
		}


	if (ch == 0x18)		/* ^x ? */
		{
		cline(&pos);		/* clear line */
		ram[pos] = 0xff;	/* replace cursor */
		*(unsigned int *)(ram + POS) = pos;
		return;				/* we are done */
		}

	ram[pos] = 0x7f;	/* blank the cursor */

	if (ch == 0x7f)			/* DEL ? */
		--pos;
	else if (ch >= 0x20)
		ram[pos++] = ch | 0x80;		/* normal character */
	else if (ch == 0x0d)	/* CR ? */
		{
		pos &= 0xffc0;
		pos += 64;
		}
	else if (ch == 0x0c)	/* FF ? */
		{
		pos = *(unsigned int *)(ram + SCEND);
		atmp = pos & 0xff;
		pos &= 0xff00;

		do	{
			ram[pos++] = 0x7f;
			} while ((pos >> 8) != atmp);

		pos = *(unsigned int *)(ram + SCEND);
		pos &= 0xff00;
		}
	else if (ch == 0x0b)	/* VT ? */
		{
		pos = *(unsigned int *)(ram + SCEND);
		pos &= 0xff00;
		}
	else if (ch == 0x09)	/* tab ? */
		{
		pos &= 0xfff8;
		pos += 8;
		}
	else		/* unknown character */
		;

	/* scrl begins here
	 */
	atmp = ram[SCEND];
	if ((pos >> 8) == atmp)
		{
		to = ram[SCEND + 1] << 8;
		from = to + 64;
		nchrs = ((ram[SCEND] - ram[SCEND + 1]) << 8) - 64;
		for (i = 0; i < nchrs; ++i)
			ram[to++] = ram[from++];

		pos = (ram[SCEND] << 8) - 1;
		cline(&pos);
		}

	/* replace cursor and return
	 */
	ram[pos] = 0xff;
	*(unsigned int *)(ram + POS) = pos;
	}


/*
 * Clear the line the cursor's on.
 */
void cline( U16 *cp )
	{
	int n;
	U16 pos;

	pos = *cp;

	pos |= 0x3f;		/* set cursor to end of current line */
	n = 63;

	for (n = 63; n != 0; --n)
		ram[pos--] = 0x7f;

	*cp = pos;
	}


/*
 * Put drive names into buffer starting at hlreg.
 */
void sysnames( void )
	{
	char *p;
	S16 i;

	p = (char *)(ram + hlreg);
	for (i = 1; i <= 7; ++i)
		{
		strcpy(p, diskname[i]);
		p += strlen(p) + 1;
		}
	}


/*
 * Echo a string, to the lower window.  String is at (HL).
 * May end with CR or NUL, depending on calling program.
 * If newline == 1, go to a new line in the window.
 */
void sysecho( S16 newline )
	{
	char *p, c;
	S16 i = 0;

	p = (char *)(ram + hlreg);
	while (i < 78 && (c = *p++) >= ' ')
		{
		putch(c);
		++i;
		}

	if (newline)
		{
		putch('\r');
		putch('\n');
		}
	}


/*
 * Clear the current line in the lower window.
 */
void sysclrline( void )
	{

	putch('\r');
	clreol();
	}


/*
 * Print a character from the Poly printer driver (non-blocking version).
 * Returns immediately with carry clr if OK, set if not ready.
 * Handles printer-file redirection.
 */
void sysprintnb( void )
	{
	union REGS regs;

	freg &= ~C_FLG;			/* assume it will work */

	if (printer_fh != -1)	/* if redirecting to a file */
		{
		write(printer_fh, &areg, 1);	/* ignore errors */
		return;
		}

	if (lpt_num >= 0)		/* if going to an LPT port */
		{
		if ((biosprint(2, 0, lpt_num) & 0x90) == 0x90)	/* if ready */
			biosprint(0, areg, lpt_num);
		else
			freg |= C_FLG;		/* if not, return C set */

		return;
		}

	if (com_num >= 0)		/* if going to a COM port */
		{
		regs.h.ah = 3;					/* get COM port status */
		regs.x.dx = com_num;			/* from this port */
		int86(0x14, &regs, &regs);		/* call BIOS to do it */
		if (regs.h.ah & 0x20)			/* if ready for a char */
			{
			regs.h.ah = 1;				/* send the char */
			regs.x.dx = com_num;
			regs.h.al = areg;
			int86(0x14, &regs, &regs);
			}
		else
			freg |= C_FLG;			/* not ready, return C set */
		}
	 }


/*
 * Get a character from the Poly printer driver.
 * Returns immediately with carry clr if OK, set if not ready.
 * Only works when connected to a serial printer.
 */
void sysrdprt( void )
	{
	union REGS regs;

	freg &= ~C_FLG;			/* assume it will work */

	if (printer_fh != -1 || lpt_num != -1)	/* if using file or LPT */
		{
		freg |= C_FLG;
		return;
		}

	regs.h.ah = 3;					/* get COM port status */
	regs.x.dx = com_num;			/* from this port */
	int86(0x14, &regs, &regs);		/* call BIOS to do it */
	if (regs.h.ah & 0x01)			/* if there's a char ready */
		{
		regs.h.ah = 2;				/* get the char */
		regs.x.dx = com_num;
		int86(0x14, &regs, &regs);
		areg = regs.h.al;
		return;
		}
	else
		freg |= C_FLG;			/* not ready, return C set */
	 }


/*
 * Select IBM printer device.
 *
 * Entry: hlreg-> string describing printer device
 *			      (either LPTx, COMx, or a PC filename)
 *        dereg-> buffer for success/error message
 *
 * Exit:  error buffer contains a message
 */
void syspdev( void )
	{
	char *deptr, *hlptr, *p;
	S16 n;

	hlptr = (char *)(ram + hlreg);
	deptr = (char *)(ram + dereg);
	*deptr = '\0';

	/* If we were redirecting printout to a file, close the file.
	 */
	if (printer_fh != -1)
		{
		close(printer_fh);
		printer_fh = -1;
		}

	/* If he selected an LPT, use it.
	 */
	if (strnicmp(hlptr, "LPT", 3) == 0 &&
		(n = hlptr[3] - '0') >= 1 && n <= MAXLPT)
		{
		lpt_num = n - 1;	/* LPT1 => 0 */
		com_num = -1;
		sprintf(deptr, "PM printer connected to device LPT%d.\r", n);
		return;
		}

	/* If he selected a COM port, use that.
	 */
	if (strnicmp(hlptr, "COM", 3) == 0 &&
		(n = hlptr[3] - '0') >= 1 && n <= MAXCOM)
		{
		com_num = n - 1;	/* COM1 => 0 */
		lpt_num = -1;
		sprintf(deptr, "PM printer connected to device COM%d.\r", n);
		return;
		}

	/* NUL-terminate the device name.
	 */
	for (p = hlptr; *p > ' '; ++p)
		;
	*p = '\0';

	if (access(hlptr, 0) == 0)
		{
		sprintf(deptr, "IBM file \"%s\" already exists!\r", hlptr);
		return;
		}

	if ((printer_fh = open(hlptr, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,
		S_IREAD|S_IWRITE)) == -1)
		{
		sprintf(deptr, "Cannot direct printer output to IBM file \"%s\".\r", hlptr);
		strcpy(printer_fname, hlptr);
		return;
		}

	sprintf(deptr, "PM printer output redirected to IBM file \"%s\".\r", hlptr);
	}


/*
 * Change disk size.
 *
 * Entry: hlreg -> string in the form "1 300" (drive, # sectors)
 *
 * Exit:  dereg -> error message or success message
 *        carry flag set if error
 */
void syssize( void )
	{
	U16 nsec, drive;
	S32 dbytes, newdbytes;
	int fh;
	POLY_DIR pdir;
	char *errp;

	freg &= ~C_FLG;		/* no carry - assume no error yet */
	errp = (char *)(ram + dereg);	/* ptr to error msg buffer */

	if (sscanf((char *)(ram + hlreg), " %u %u", &drive, &nsec) != 2)
		{
		sprintf(errp, "Usage:   size <drive> <# sectors>\r");
		freg |= C_FLG;
		return;
		}

	if (drive < 1 || drive > 7)
		{
		sprintf(errp, "Drive %u is invalid... must be 1-7.\r", drive);
		freg |= C_FLG;
		return;
		}

	if ((fh = open(diskname[drive], O_RDWR|O_BINARY)) == -1)
		{
		sprintf(errp, "No disk assigned to drive %u!\r", drive);
		freg |= C_FLG;
		return;
		}

	if (read(fh, &pdir, sizeof(pdir)) != sizeof(pdir))
		{
		close(fh);
		sprintf(errp, "Can't read directory!\r");
		freg |= C_FLG;
		return;
		}

	if (nsec < pdir.nda)
		{
		close(fh);
		sprintf(errp,
			"Drive %u has %u sectors in use, can't go below that.\r",
				drive, pdir.nda);
		freg |= C_FLG;
		return;
		}

	dbytes = ((long)nsec) << 8;
	chsize(fh, dbytes);
	newdbytes = filelength(fh);
	close(fh);
	sprintf(errp, "Drive %u: disk size changed to %u sectors.\r",
		drive, (unsigned int)(newdbytes >> 8));
	}


/*
 * Connect drive # to new PC file.
 *
 * Entry: hlreg -> string in the form "2 filename"
 *
 * Exit:  dereg -> error message or success message
 *        carry flag set if error
 */
void sysconn( void )
	{
	U16 drive;
	char *errp, newname[80];

	freg &= ~C_FLG;		/* no carry - assume no error yet */
	errp = (char *)(ram + dereg);	/* ptr to error msg buffer */

	if (sscanf((char *)(ram + hlreg), " %u %s", &drive, newname) != 2)
		{
		sprintf(errp, "Usage:   connect <drive> <IBM filename>\r");
		freg |= C_FLG;
		return;
		}

	if (drive < 1 || drive > 7)
		{
		sprintf(errp, "Drive %u is invalid... must be 1-7.\r", drive);
		freg |= C_FLG;
		return;
		}

	if (ram[SYSRES] == drive)
		{
		sprintf(errp, "Can't re-connect the System drive!\r");
		freg |= C_FLG;
		return;
		}

	if (access(newname, 0) != 0)
		{
		sprintf(errp, "Can't find IBM file \"%s\".\r", newname);
		freg |= C_FLG;
		return;
		}

	if (ram[NFDIR] == drive)	/* if that drive's dir is cached */
		ram[NFDIR] = 0;			/* invalidate it */

	strcpy(diskname[drive], newname);
	sprintf(errp, "Drive %u connected to \"%s\".\r", drive, newname);
	}


/*
 * Exit to DOS temporarily.
 */
void sysdos( void )
	{
	U8 far *savebuf, *screen;
	char *got_cwd_ok, cur_dir[80];
	S16 x, y, cur_disk;

	screen = MK_FP(videoseg, 0);
	if ((savebuf = malloc(SCREENSIZE)) == NULL)
		{
		cprintf("Can't allocate memory for screen buffer!\r\n");
		return;
		}

	movmem(screen, savebuf, SCREENSIZE);	/* save screen contents */
	x = wherex();
	y = wherey();
	window(1, 1, 80, 25);
	clrscr();

	/* save the current drive/directory for later
	 */
	cur_disk = getdisk();
	got_cwd_ok = getcwd(cur_dir, sizeof(cur_dir));

	cprintf("Type EXIT to return to PM...\r\n");
	system("");

	/* restore current drive/directory
	 */
	if (got_cwd_ok != NULL)
		{
		setdisk(cur_disk);
		chdir(cur_dir);
		}

	movmem(savebuf, screen, SCREENSIZE);	/* restore screen, etc. */
	window(1, WINY0, 80, 25);
	gotoxy(x, y);
	free(savebuf);
	}


/*
 * Parse two filenames: one Poly, one PC.  (Used to copy between them.)
 *
 * Entry: hlreg-> filenames, whitespace-separated
 *        dereg-> buffer for Poly filename
 *
 * Exit:  C_FLG set if error, dereg->error message
 *        areg = 0 if to Poly, 1 if to IBM.
 */
void sysparse( void )
	{
	char c, *q, *deptr, name1[80], name2[80];
	S16 i, ibm_1 = 0, ibm_2 = 0, poly_1 = 0, poly_2 = 0;

	q = (char *)(ram + hlreg);

	freg &= ~C_FLG;			/* assume no error yet */
	deptr = (char *)(ram + dereg);
	*deptr = '\0';			/* clr error msg area */

	if (xfer_fh != -1)
		sysclos();		/* if the handle was left open earlier */

	i = 0;
	while ((c = *q) > ' ' && i < 79)
		{
		name1[i] = c;
		++i;
		++q;
		}
	name1[i] = '\0';

	while ((c = *q) == ' ' || c == '\t')	/* skip space between args */
		++q;

	i = 0;
	while ((c = *q) > ' ' && i < 79)
		{
		name2[i] = c;
		++i;
		++q;
		}
	name2[i] = '\0';

/*	cprintf("Filenames: \"%s\" and \"%s\"\r\n", name1, name2); */

	if (strpbrk(name1, "<>") != NULL)
		poly_1 = 1;
	if (strpbrk(name2, "<>") != NULL)
		poly_2 = 1;
	if (strpbrk(name1, "\\:") != NULL)
		ibm_1 = 1;
	if (strpbrk(name2, "\\:") != NULL)
		ibm_2 = 1;

	if ((poly_1 || ibm_2) && !ibm_1 && !poly_2)		/* Poly = source */
		{
		strcpy(deptr, name1);
		strcpy(xfer_name, name2);
/*		cprintf("Poly: \"%s\"  to IBM: \"%s\"\r\n", name1, name2); */
		areg = 1;
		if (access(name2, 0) == 0)
			{
			freg |= C_FLG;
			strcpy(deptr, "IBM file already exists!\r");
			}
		else if ((xfer_fh = open(name2,
			O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE)) == -1)
			{
			freg |= C_FLG;
			strcpy(deptr, "Can't create IBM file!\r");
			}
		}
	else if ((ibm_1 || poly_2) && !poly_1 && !ibm_2)		/* Poly = dest */
		{
		strcpy(xfer_name, name1);
		strcpy(deptr, name2);
/*		cprintf("IBM: \"%s\"  to Poly: \"%s\"\r\n", name1, name2); */
		areg = 0;
		if ((xfer_fh = open(name1, O_RDONLY|O_BINARY)) == -1)
			{
			freg |= C_FLG;
			strcpy(deptr, "IBM file does not exist!\r");
			}
		}
	else if (poly_1 && poly_2)
		{
		strcpy(deptr, "Can't copy between two Poly files!\r\n");
		freg |= C_FLG;
		}
	else if (ibm_1 && ibm_2)
		{
		strcpy(deptr, "Can't copy between two IBM files!\r\n");
		freg |= C_FLG;
		}
	else
		{
		strcpy(deptr, "Can't determine which file is Poly, which is IBM!\r");
		freg |= C_FLG;
		}
	}


/*
 * Set text/binary copy mode ('T' or 'B' in areg).
 */
void systext( void )
	{

	copymode = areg;
	}


/*
 * Write 256 bytes to xfer_fh.
 * If binary, do it straight, else convert \r to \r\n and supress \0.
 */
void syswrit( void )
	{
	S16 j, textmode, err = 0;
	char c, *buf, *deptr;

	textmode = (copymode == 'T');
	buf = (char *)(ram + hlreg);
	deptr = (char *)(ram + dereg);
	*deptr = '\0';
	freg &= ~C_FLG;

	for (j = 0; j < SECSIZE; ++j)
		{
		c = buf[j];
		if (c == '\r' && textmode)		/* if \r, in text mode, */
			{
			writerr(c, &err);
			c = '\n';					/* chase with \n */
			writerr(c, &err);
			}
		else if (c != '\0' || !textmode)
			writerr(c, &err);			/* supress NULs in text mode */

		if (err)
			{
			freg |= C_FLG;
			strcpy(deptr, "Can't write to IBM file!\r");
			break;
			}
		}
	}


/*
 * Write one byte to xfer_fh, with error checking.
 * OR *errp with 1 if it fails.
 */
void writerr( char c, S16 *errp )
	{

	if (write(xfer_fh, &c, 1) != 1)
		*errp |= 1;
	}


/*
 * Close the xfer_fh.
 */
void sysclos( void )
	{

	close(xfer_fh);
	xfer_fh = -1;
	}


/*
 * Read 256 bytes from the PC file, put into the buffer at hlreg.
 * If error, set C_FLG and put message at dereg.
 * If EOF, set areg = 1, else areg = 0.
 */
void sysread( void )
	{
	S16 i, r, textmode;
	char c, *buf, *deptr;

	textmode = (copymode == 'T');
	buf = (char *)(ram + hlreg);
	deptr = (char *)(ram + dereg);
	*deptr = '\0';
	freg &= ~C_FLG;
	areg = 0;		/* assume no EOF yet */

	memset(buf, 0, SECSIZE);	/* prepare a new sector */

	if (!textmode)		/* if in binary mode */
		{
		if ((r = read(xfer_fh, buf, SECSIZE)) < 0)
			{
			freg |= C_FLG;
			strcpy(deptr, "Can't read from IBM file!\r");
			}
		else if (r == 0)
			areg = 1;	/* end of file reached */

		return;			/* if binary, we are done */
		}

	/* Do text mode read, a character at a time.
	 */
	for (i = 0; i < SECSIZE; ++i)
		{
		r = read(xfer_fh, &c, 1);

		if (r < 0)
			{
			freg |= C_FLG;
			strcpy(deptr, "Can't read from IBM file!\r");
			return;
			}
		else if (r == 0)
			{
			if (i == 0)
				areg = 1;	/* return EOF at the start of this sector */
			return;
			}

		if (c != 'Z' - '@')			/* store char unless ^Z */
			buf[i] = c;

		if (c == '\r')				/* if it's a CR, */
			read(xfer_fh, &c, 1);	/* skip next byte, assume it's LF */
		}
	}


