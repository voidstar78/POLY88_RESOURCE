/*
 * PM - PolyMorphic emulator for MS-DOS
 * 9/20/89 rdb
 *
 * version 2.1, 5/30/2001 - Gawd, whoever thought it would be this date!
 * corrected "r" going into undefined memory during startup.
 *
 * version 3.0, 9/13/2004 - runs under Win98 or XP.
 *
 * version 3.1, 2/13/2005 - added -88 flag to boot as a Poly 88.
 *
 * version 4.0, 11/7/2012 - more portable, removed some assembly code
 * (wrote the processor emulation in C)
 * changed to U8/U16/S8 etc. everywhere
 *
 * version 5.0, 7/2025 - completed removing separate ASM files
 *
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <alloc.h>
#include <dos.h>
#include <time.h>
#include <conio.h>
#include <bios.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#include "pmhdr.h"


S16 match_flag, match_auto;
S32 match_delays;
S16 update_screen;
S16 poly88_flag = 0;
S16 step_flag = 0;
S16 tbasic_flag = 0;
U16 sstart = SSTART_8813;
U16 ssend;
FILE *cass_fp = NULL;
FILE *wh1_fp = NULL;
U8 last_port1 = 0x00;

U8 far *ram;
S16 norunflag = 0;
S16 breaksys_before = -1, breaksys_after = -1;
S16 adaptive_rtc_flg = 1;	// 1=adaptive RTC, 0=18.2 Hz RTC
S16 ops_flag = 0;
S16 fpb_flag = 0;			// default to no N* FPB
U8 fpb_byte[2] = {0xff, 0x00};

U16 videoseg = 0xb800;
U8 far *scrnaddrs[SSIZE];
char far *videotick;
S16 running = 0;
S16 gotkbhit = 0;
char notice1[] =
	"          "
	"PM - PolyMorphic Emulator  v5.0  (c) 1990-2025 by Bob Bybee\r\n";

char notice2[] =
	"          "
	"Type: 'run' to begin... Alt-F1 and 'quit' to end.\r\n";

char notice3[] =
	"          "
	"Type:  Alt-F1 and 'quit' to end.\r\n";



void main( S16 argc, char **argv )
	{
	char inbuf[80], ifile[80], *inp;
	U16 gotopc, addr0, addr1;
	S16 n, dnum, displayregs;

	for (++argv, --argc; **argv == '-'; ++argv, --argc)
		{
		if (strcmp(*argv, "-norun") == 0)
			{
			norunflag = 1;
			}
		else if (strcmp(*argv, "-ops") == 0)
			{
			ops_flag = 1;
			}
		else if (strcmp(*argv, "-match") == 0)
			{
			match_flag = 1;
			match_auto = 1;
			match_delays = 15;		// initial try
			}
		else if (sscanf(*argv, "-match%ld", &match_delays) == 1)
			{
			match_flag = 1;
			match_auto = 0;		// should already be
//			cprintf("match_flag= %d, match_delays=%ld\n", match_flag, match_delays);
//			getch();
			}
		else if (strcmp(*argv, "-step") == 0)
			{
			step_flag = 1;
			}
		else if (strcmp(*argv, "-capture") == 0)
			{
			wh1_fp = fopen("wh1.out", "wb");
			}
		else if (strcmp(*argv, "-88") == 0)
			{
			poly88_flag = 1;
			}
		else if (strcmp(*argv, "-tbasic") == 0)
			{
			tbasic_flag = 1;
			}
		else if (strcmp(*argv, "-disrom") == 0)
			{
			disasmrom();
			exit(1);
			}
		else if (sscanf(*argv, "-bsb%d", &n) == 1)
			breaksys_before = n;
		else if (sscanf(*argv, "-bsa%d", &n) == 1)
			breaksys_after = n;
		else
			{
			cprintf("Unknown option: %s\r\n", *argv);
			exit(1);
			}
		}

	// see if vdisk names were entered.
	dnum = 1;
	while (argc > 0)
		{
		if (access(*argv, 0) != 0)
			{
			cprintf("Can't locate file: %s\r\n", *argv);
			exit(1);
			}
		strcpy(diskname[dnum++], *argv);
		++argv, --argc;
		}

	setpcvideo();

	initram();
	initdev(dnum - 1);

	cprintf(notice1);
	if (poly88_flag)
		cprintf("          " "Emulating a Poly-88.\r\n");
	else
		cprintf("          " "Emulating a PolyMorphic Systems 8813.\r\n");

	if (norunflag)
		cprintf(notice2);
	else
		cprintf(notice3);

	if (match_flag)
		{
		if (match_auto)
			cprintf("Matching original Poly speed (automatic).\r\n");
		else
			cprintf("Matching original Poly speed (manual= %ld).\r\n", match_delays);
		}



	border();
	bootsys();
	delay(1000);
//	rtctest();

	while (1)
		{
		displayregs = 0;

		if (norunflag)
			{
			fgupscreen();
			cprintf("\r[pm] ");
			inbuf[0] = sizeof(inbuf) - 2;
			inp = cgets(inbuf);
			cprintf("\r");
			}
		else
			{
			inp = inbuf;
			inp[0] = 'r';
			}

		gotkbhit = 0;
		norunflag = 1;
		switch(tolower(inp[0]))
			{
			case 'b':			/* boot the system */
				bootsys();
								/* and fall into "run" */
			case 'r':
				putch('\n');
				running = 1;
				while (running)
					{
					emu(0);
					check_keyb();
					check_rt();
					}
				break;

			case '1':
				emu(1);
				displayregs = 1;
				break;

			case 'g':
				putch('\n');
				sscanf(inp + 1, " %x", &gotopc);
				running = 2;
				while (running && pcreg != gotopc)
					{
					emu(1);
					check_keyb();
					check_rt();
					}
				displayregs = 1;
				break;

			case 'x':
				displayregs = 1;
				break;

			case 't':
				gotopc = 0xffff;
				sscanf(inp + 1, " %x", &gotopc);
				do
					{
					emu(1);
					check_rt();
					dumpregs();
					} while (gotopc != 0xffff && pcreg != gotopc);
				break;

			case 'i':
				sscanf(inp + 1, " %x %s", &addr0, ifile);
				cprintf("\r\n%x %s\r\n", addr0, ifile);
				loadfile(addr0, ifile);
				break;

			case 'd':
				putch('\n');
				addr1 = 0x08;
				sscanf(inp + 1, " %x %x", &addr0, &addr1);
				dumpmem(addr0, addr1);
				break;

			case 'u':
				putch('\n');
				addr0 = pcreg;
				addr1 = pcreg + 3;
				sscanf(inp + 1, " %x %x", &addr0, &addr1);
				unasmmem(addr0, addr1);
				break;

			case 'q':
				if (wh1_fp != NULL)
					fclose(wh1_fp);
				exit(0);
				break;

			case '\0':
				cprintf("\r\n");
				break;

			case 'w':
				cprintf("cass_fp %p offset: %lx\r\n", cass_fp, ftell(cass_fp));
				break;

			default:
				cprintf("\r\nBad command.\r\n");
				break;
			}

		if (displayregs)
			dumpregs();
		}
	}


/*
 * Execute instructions in the emulator.
 * If once == 1, only do one call.
 * Handle system calls too.
 */
void emu( S16 once )
	{
	if (once)
		startemu(1);
	else
		startemu(100);

	if (update_screen)
		{
		update_screen = 0;
		fgupscreen();
		}

	if (syscall != 0)
		{
		systemcall();
		fgupscreen();
		}

	}




/*
 * Dump the 8080 registers.
 */
void dumpregs( void )
	{
	U8 tmp;
	S16 i;
	static char *flaglist[] = {"C ", "", "PE ", "", "A ", "", "Z ", "M "};
	static char *imasklist[8] =
		{"ss1 ", "ss2 ", "ei1 ", "ei2 ", "us ", "kb ", "rt ", "ss "};

	cprintf("BC %04X   DE %04X   AF %02X%02X   im=%02x ", bcreg, dereg, areg, freg, imask);

	tmp = freg;
	for (i = 0; i < 8; ++i)
		{
		if (tmp & 1)
			cprintf(flaglist[i]);
		tmp >>= 1;
		}

	cprintf(iflag ? " ei  " : " di  ");

	tmp = imask;
	for (i = 0; i < 8; ++i)
		{
		if (tmp & 1)
			cprintf(imasklist[i]);
		tmp >>= 1;
		}

	cprintf("\r\nHL %04X   SP %04X   PC %04X   ", hlreg, spreg, pcreg);
	disasm(pcreg);
	}


/*
 * Dump the memory area pointed to by val, label it with regname.
 */
void regmem( char *regname, U16 val )
	{
	S16 i;

	cprintf("%s  %04X  ", regname, val);
	for (i = 0; i < 6; ++i)
		cprintf("%02X ", ram[val++]);
	}


/*
 * Dump memory from addr0 to addr1.
 */
void dumpmem( U16 addr0, U16 addr1 )
	{
	char buf[80], *p;
	U16 i, lim;
	U8 byte;

	if (addr1 < addr0)
		addr1 += addr0;

	for (; addr0 <= addr1; addr0 += 16)
		{
		lim = addr0 + 16;
		p = buf;
		p += sprintf(p, "%04X  ", addr0);

		for (i = addr0; i < lim; i++)
			{
			byte = ram[i];
			p += sprintf(p, "%02X ", byte);
			if (i == addr0 + 7)
				*p++ = ' ';
			}

		*p++ = ' ';
		for (i = addr0; i < lim; i++)
			{
			byte = ram[i];
			if (' ' <= byte && byte < 0x7f)
				*p++ = byte;
			else
				*p++ = '.';
			}
		*p++ = '\r';
		*p++ = '\n';
		*p = '\0';
		cprintf(buf);
		}
	}


/*
 * Unassemble addr0 - addr1.
 */
void unasmmem( U16 addr0, U16 addr1 )
	{

	if (addr1 < addr0)
		addr1 += addr0;

	for (; addr0 <= addr1; )
		{
		cprintf("%04X  ", addr0);
		addr0 += disasm(addr0);
		}
	}


/*
 * Set up screen stuff for emulation.
 */
void setpcvideo( void )
	{
	struct text_info ti;
	char far *fp;
	S16 i;

	gettextinfo(&ti);
	if (ti.currmode == 7)
		videoseg = 0xb000;	/* monochrome */
	else
		videoseg = 0xb800;	/* color */
	videotick = MK_FP(videoseg, 79 * 2);
	clrscr();
	fp = MK_FP(videoseg, 0);
	for (i = 18 * 80; i > 0; --i)
		{
		*fp++ = ' ';
		*fp++ = 0x07;		/* and normal attribute */
		}
	mkscrnmap();

	window(9, 2, 72, 17);
	clrscr();
	gotoxy(1, 25);
	window(1, WINY0, 80, 25);
	}


/*
 * Draw a border around window.
 */

#define BDR_UPLEFT	218
#define BDR_HORIZ	196
#define BDR_VERT	179
#define BDR_UPRIGHT 191
#define BDR_LORIGHT 217
#define BDR_LOLEFT	192

int bdr_horiz_top, bdr_horiz_bot;
int bdr_vert_left, bdr_vert_right;
int bdr_upright, bdr_loright;
int bdr_loleft, bdr_upleft;


void border( void )
	{
	S16 i;
	U8 far *fp;
	S16 x1 = 7, y1 = 0, x2 = 7 + 64 + 1, y2 = 16 + 1;

	if (vgaused)
		{
		bdr_upleft = 0xa5;
		bdr_horiz_top = 0xad;
		bdr_horiz_bot = 0xad;
		bdr_vert_left = 0x87;
		bdr_vert_right = 0xb8;
		bdr_upright = 0xac;
		bdr_loright = 0xa9;
		bdr_loleft = 0x8d;
		}
	else
		{
		bdr_upleft = BDR_UPLEFT;
		bdr_horiz_top = BDR_HORIZ;
		bdr_horiz_bot = BDR_HORIZ;
		bdr_vert_left = BDR_VERT;
		bdr_vert_right = BDR_VERT;
		bdr_upright = BDR_UPRIGHT;
		bdr_loright = BDR_LORIGHT;
		bdr_loleft = BDR_LOLEFT;
		}

	fp = MK_FP(videoseg, ((y1 * 80) + x1) * 2);
	*fp = bdr_upleft;
	fp += 2;
	for (i = x1 + 1; i < x2; ++i)
		{
		*fp = bdr_horiz_top;
		fp += 2;
		}
	*fp = bdr_upright;
	fp += 80 * 2;
	for (i = y1 + 1; i < y2; ++i)
		{
		*fp = bdr_vert_right;
		fp += 80 * 2;
		}
	*fp = bdr_loright;
	fp -= 2;
	for (i = x2; i > x1 + 1; --i)
		{
		*fp = bdr_horiz_bot;
		fp -= 2;
		}
	*fp = bdr_loleft;
	fp -= 80 * 2;
	for (i = y2; i > y1 + 1; --i)
		{
		*fp = bdr_vert_left;
		fp -= 80 * 2;
		}
	}


/*
 * Init the ram and its associated variables.
 */
void initram( void )
	{
	if ((ram = farcalloc(1, 65536L + 16L)) == NULL)
		{
		cprintf("Can't get memory!\r\n");
		exit(1);
		}

	ramseg = FP_SEG(ram) + 1;
	ram = MK_FP(ramseg, 0);
	}


/*
 * Cold-boot the system (press LOAD).
 */
void bootsys( void )
	{
	S16 i;

	bootrom();
	pcreg = 0;		/* start running at 0000 */
	iflag = 0;		/* disable interrupts */
	imask = 0;

	if (poly88_flag)
		sstart = SSTART_88;

	/* fill screen memory with graphics blanks
	 */
	for (i = sstart; i < sstart + SSIZE; ++i)
		{
		memw(i, 0x7f);
		//ram[i] = 0x7f;
		}

	fill_fpb();		/* arrange North Star FPB area */
	}


/*
 * Load a file into memory.
 */
void loadfile( U16 addr, char *file )
	{
	FILE *fp;
	S16 c;

	if ((fp = fopen(file, "rb")) == NULL)
		{
		cprintf("Can't open %s\n", file);
		return;
		}

	cprintf("Loading %s at address %x\r\n", file, addr);
	while ((c = fgetc(fp)) != EOF)
		ram[addr++] = c;

	cprintf("Loaded.\r\n");
	fclose(fp);
	}


/*
 * Set the North Star floating-point board presence flag and fill its
 * memory space accordingly.
 */
void setfpb( char *flag )
	{
	if (stricmp(flag, "on") == 0)
		fpb_flag = 1;
	else
		fpb_flag = 0;

	fill_fpb();
	}


/*
 * Set the North Star floating-point board area to FF
 * (if fpb_flag is ON) or to 00 (if OFF).
 */
void fill_fpb( void )
	{
	U16 i;
	U8 b;

	b = fpb_byte[fpb_flag];
	for (i = FPSTART; i < FPSTART + FPSIZE; ++i)
		ram[i] = b;
	}



/****************************************************************
 * Poly 88 cassette I/O ("interrupt" driven)
 ****************************************************************/


/*
 * Handle USART control (port 1) according to the A register.
 */
void usart_control( void )
	{
	char *inp, inbuf[80];

	if (last_port1 == 0 && areg == 0x96)	// turning on the cassette
		{
		close_cassfp();

	again:
		cprintf("Cassette file (q to quit): ");
		memset(inbuf, 0, sizeof(inbuf));
		inbuf[0] = sizeof(inbuf) - 2;
		inp = cgets(inbuf);
		cprintf("\r\n");

		if (stricmp(inp, "q") == 0)
			exit(1);

		if (strchr(inp, '.') == NULL)
			strcat(inp, ".cas");

		if ((cass_fp = fopen(inp, "rb")) == NULL)
			{
			cprintf("Can't open %s\r\n", inp);
			goto again;
			}
		}
	else if (areg == 0)		// if it's an IDLE command
		{
		close_cassfp();
		}

	last_port1 = areg;
	usart_opcount = 0;
	}


void close_cassfp( void )
	{

	if (cass_fp != NULL)
		{
		fclose(cass_fp);
		cass_fp = NULL;
		}
	}


S16 cass_data_avail( void )
	{

	if (cass_fp == NULL)
		return (0);

	if (feof(cass_fp))
		return (0);

	return (1);
	}


/*
 * Copy the Poly screen to the real screen (foreground code)
 */
U8 grtbl[] =
	{
	0xDB,0xDB,0xDB,0xDD,0xDB,0xDD,0xDD,0xDD,		 //00-07
	0xDB,0xDF,0xDF,0xDF,0xDB,0xDF,0xDD,0xDD,		 //08-0F
	0xDB,0xDF,0xDB,0xDF,0xDC,0xDB,0xDC,0xDD,		 //10-17
	0xDE,0xDF,0xDF,0xDF,0xDE,0xDF,0xDF,0x20,		 //18-1F
	0xDB,0xDB,0xDC,0xDD,0xDC,0xDC,0xDC,0xDD,		 //20-27
	0xDE,0xDF,0xDF,0xDF,0xDC,0xDC,0xDC,0x20,		 //28-2F
	0xDE,0xDE,0xDC,0xDB,0xDC,0xDC,0xDC,0x20,		 //30-37
	0xDE,0xDE,0xDE,0x20,0xDE,0x20,0x20,0x20,		 //38-3F
 //40-7F are duplicates of 00-3F graphics characters.
 //We really need a Hercules RamFont to do this right.
	0xDB,0xDB,0xDB,0xDD,0xDB,0xDD,0xDD,0xDD,		 //00-07
	0xDB,0xDF,0xDF,0xDF,0xDB,0xDF,0xDD,0xDD,		 //08-0F
	0xDB,0xDF,0xDB,0xDF,0xDC,0xDB,0xDC,0xDD,		 //10-17
	0xDE,0xDF,0xDF,0xDF,0xDE,0xDF,0xDF,0x20,		 //18-1F
	0xDB,0xDB,0xDC,0xDD,0xDC,0xDC,0xDC,0xDD,		 //20-27
	0xDE,0xDF,0xDF,0xDF,0xDC,0xDC,0xDC,0x20,		 //28-2F
	0xDE,0xDE,0xDC,0xDB,0xDC,0xDC,0xDC,0x20,		 //30-37
	0xDE,0xDE,0xDE,0x20,0xDE,0x20,0x20,0x20,		 //38-3F

	0xE0,0xE1,0xE2,0xEB,0xEE,0x0B,0x20,0xE9,		 //80-87
	0x20,0x20,0x20,0xE6,0xE2,0x20,0xF8,0xE3,		 //88-8F
	0x20,0xE5,0xE7,0x76,0xED,0x78,0x20,0x77,		 //90-97
	0xEA,0xFB,0x1A,0x1B,0x18,0xF6,0xE4,0xF7,		 //98-9F
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,		 //A0-A7
	0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,		 //A8-AF
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,		 //B0-B7
	0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,		 //B8-BF
	0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,		 //C0-C7
	0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,		 //C8-CF
	0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,		 //D0-D7
	0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,		 //D8-DF
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,		 //E0-E7
	0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,		 //E8-EF
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,		 //F0-F7
	0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0xDB,		 //F8-FF
	};

/*
 * Copy the Poly screen to the PC screen.
 */
void fgupscreen( void )
	{
	U8 far *dest = MK_FP(videoseg, 16 + 160);
	U8 far *src = ram + sstart;
	S16 i, j;

	// copy Poly screen to real screen
	for (i = 0; i < 16; ++i)
		{
		for (j = 0; j < 64; ++j)
			{
			*dest = grtbl[*src];
			dest += 2;
			++src;
			}
		dest += 32;
		}
	}

/*
 * Create a map from Poly screen (locations 0..3ff) to real screen.
 */
U8 far *scrnmap[1024];

void mkscrnmap( void )
	{
	U8 far *dest = MK_FP(videoseg, 16 + 160);
	S16 i, j, polycell = 0;

	for (i = 0; i < 16; ++i)
		{
		for (j = 0; j < 64; ++j)
			{
			scrnmap[polycell] = dest;
			++polycell;
			dest += 2;
			}
		dest += 32;
		}
	}



/*
 * Check if a real-time clock interrupt tick has gone by.
 *
 * Old: do this at the PC's 18.2 Hz rate.
 * New: adaptively count instructions and try to get about 60 / second.
 */
void check_rt( void )
	{
	static S16 clk, oldclk = 0, ticks4 = 0, ticks18 = 0, ticks91 = 0;
	static S32 opavg, tt;
#define NUM_MATCH_STEPS 9
	static S32 match_steps[NUM_MATCH_STEPS] = {5, 5, 5, 5, 5, 4, 3, 2, 1};
	static S16 match_steps_i = 0;

	clk = *(S16 *)0x46cL;
	if (oldclk != clk)
		{
		oldclk = clk;

		// At about 1 Hz, flip the activity light in the corner.
		if (++ticks18 >= 18)
			{
			ticks18 = 0;

			if (*videotick == ' ')
				*videotick = '*';	// flash the corner
			else
				*videotick = ' ';
			}

		// count the number of opcodes in 5 seconds
		// 18.2 x 5 = 91 ticks of the PC
		if (++ticks91 >= 91)
			{
			ticks91 = 0;
			opavg = opcount / 5;
			tt = opavg / 60;
			if (adaptive_rtc_flg && tt > 500 && tt < 1000000L)
				rtc_opcount_max = tt;

			// if match_auto is on, use opavg to try to set match_delays
			// so that we head toward opavg == MATCH_TARGET.
			if (match_flag && match_auto)
				{
				if (opavg < MATCH_TARGET)
					match_delays -= match_steps[match_steps_i];
				else
					match_delays += match_steps[match_steps_i];
				if (match_delays < 5)
					match_delays = 5;
				if (match_delays > 100)
					match_delays = 100;

				if (match_steps_i < NUM_MATCH_STEPS - 1)
					++match_steps_i;
				}

			if (ops_flag)
				{
				cprintf("\r%ld avg ops/sec  RTC %ld %ld   match_delays %ld %d",
					opavg, tt, rtc_opcount_max, match_delays, match_steps_i);
				}

			opcount = 0;		// reset to zero for next average
			}

		// update the screen about 4x/second.
		if (++ticks4 >= 4)
			{
			ticks4 = 0;
			update_screen = 1;
			}

		// Each time through, set the Poly's RTC interrupt,
		// UNLESS we're using rtc_opcount_max to set it
		// in the main opcode loop.
		// (18 vs approx. 60 Hz)
		if (rtc_opcount_max == 0)
			{
			setimask(INT_RT);
			}
		}
	}


