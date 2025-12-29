/*
 * Opcode execution (central emulation loop) for Poly Emulator.
 * Used to be in assembly; moving to C as a first step in a truly
 * portable Poly emulator.
 *
 * Bob Bybee, 11/2/2012
 */
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include "pmhdr.h"

extern U8 paritytable[];

REGPAIR hlregpair, bcregpair, deregpair, afregpair;
U16 spreg, pcreg;

U16 emcount;
U16 ramseg;
U8 syscall, kbport;
U8 iflag;
U8 imask;
S32 opcount, usart_opcount, rtc_opcount, rtc_opcount_max;

#define RST7 0xff
#define RST6 0xf7
#define RST5 0xef
#define RST4 0xe7


#define CARRY (freg & C_FLG)


/*
 * Write to memory, if >= 0c00.  0000 - 0bff are ROMs.
 */
void memw( U16 addr, U8 value )
	{

	if (addr >= 0x0c00)			// if not writing to ROM
		ram[addr] = value;

	if ((addr & 0xfc00) == sstart)		// if writing to screen
		*scrnmap[addr & 0x3ff] = grtbl[value];	// put to PC screen too
	}


/*
 * Set the interrupt mask, and break from emulation.
 */
void setimask( U8 mask )
	{
	imask |= mask;
	breakem();
	}


/*
 * call: pushes PC, goes to arg-provided PC.
 */
static void call( U8 npchi, U8 npclo )
	{

	--spreg;
	memw(spreg, pcreg >> 8);	// push PC hi
	--spreg;
	memw(spreg, pcreg);			// push PC lo

	pcreg = ((npchi << 8) | npclo);
	}


/*
 * immediate call:  gets new PC from immediate data.
 */
static void icall( void )
	{
	U8 tblo, tbhi;

	tblo = ram[pcreg++];
	tbhi = ram[pcreg++];
	call(tbhi, tblo);
	}

/*
 * Call, if condx is true.
 */
static void icallcc( U8 condx )
	{
	if (condx)
		icall();
	else
		pcreg += 2;
	}


/*
 * immediate jump:  regular jump, gets new PC from immediate data.
 */
static void ijump( void )
	{
	U8 tblo, tbhi;

	tblo = ram[pcreg++];
	tbhi = ram[pcreg++];
	pcreg = ((tbhi << 8) | tblo);
	}


/*
 * Jump, if condx is true.
 */
static void ijumpcc( U8 condx )
	{
	if (condx)
		ijump();
	else
		pcreg += 2;
	}

/*
 * Return, if condx true.
 */
static void retcc( U8 condx )
	{
	U8 tblo, tbhi;

	if (condx)
		{
		tblo = ram[spreg++];
		tbhi = ram[spreg++];
		pcreg = (tbhi << 8) | tblo;
		}
	}


/*
 * DAD regpair - affects C flag only.
 */
void dad( U16 regpair )
	{
	U32 tbig;

	tbig = hlreg;
	tbig += regpair;
	hlreg = (U16)tbig;

	if (tbig & 0xffff0000L)
		freg |= C_FLG;
	else
		freg &= ~C_FLG;
	}

/*
 * Set/clear the Z, S, P flags.
 * Note: clears AC flag; assumes caller will set it if needed.
 */
static void setzsp( U8 v )
	{

	freg &= ~(Z_FLG | S_FLG | P_FLG | AC_FLG);
	if (v & 0x80)
		freg |= S_FLG;
	else if (v == 0)	// if previous test (& 0x80) this one can't be true.
		freg |= Z_FLG;

	freg |= paritytable[v];
	}


/*
 * ADD value to areg - with carry if given.  Set all flags.
 */
static void add( U8 v, U8 carry )
	{
	U16 vw, aw, a4, v4;

	v4 = v & 0x0f;
	a4 = areg & 0x0f;
	a4 = a4 + v4 + carry;		// to look for carry out of bits 0-3

	vw = v;
	aw = areg;
	aw = aw + vw + carry;
	areg = (U8)aw;

	setzsp(areg);
	freg &= ~(AC_FLG | C_FLG);
	if (a4 & 0xf0)
		freg |= AC_FLG;
	if (aw & 0xff00)
		freg |= C_FLG;
	}


/*
 * SUB value from areg - with carry if given.  Set all flags.
 */
static void sub( U8 v, U8 carry )
	{
	U16 vw, aw, a4, v4;

	v4 = v & 0x0f;
	a4 = areg & 0x0f;
	a4 = a4 - v4 - carry;		// to look for carry out of bits 0-3

	vw = v;
	aw = areg;
	aw = aw - vw - carry;
	areg = (U8)aw;

	setzsp(areg);
	freg &= ~(AC_FLG | C_FLG);
	if (a4 & 0xf0)
		freg |= AC_FLG;
	if (aw & 0xff00)
		freg |= C_FLG;
	}


/*
 * CMP - uses SUB but doesn't change areg.
 */
static void cmp( U8 v )
	{
	U8 save_areg;

	save_areg = areg;
	sub(v, 0);
	areg = save_areg;
	}


/*
 * DAA - now taken from the Intel 8086 manual
 */
static void daa( void )
	{
	U8 old_areg, old_cf, new_ac = 0, new_c = 0;
	U16 tw;

	old_areg = areg;
	old_cf = (freg & C_FLG);
	freg &= ~C_FLG;

	if (((areg & 0x0f) > 9) || (freg & AC_FLG))
		{
		new_ac = 1;

		areg += 6;
		tw = areg & 0x0f;
		tw += 6;	// look for carry caused by add 6

		if (old_cf || (tw >= 0x100))	// if carry caused by add 6
			new_c = 1;
		}
//	else
//		freg &= ~AC_FLG;

	if ((old_areg > 0x99) || old_cf)
		{
		areg += 0x60;
		new_c = 1;
		}
//	else
//		freg &= ~C_FLG;

	setzsp(areg);
	freg &= ~(C_FLG | AC_FLG);
	if (new_ac)
		freg |= AC_FLG;
	if (new_c)
		freg |= C_FLG;
	}


/*
 * INR/DCR:  inc/dec an 8-bit value, sets all (including AC) but not Carry.
 */
static void inr( U8 *vp )
	{
	U8 v, v4;

	v = *vp;
	v4 = v & 0x0f;
	++v;
	setzsp(v);
	*vp = v;

	++v4;
	if (v4 & 0x10)
		freg |= AC_FLG;
	}

static void dcr( U8 *vp )
	{
	U8 v, v4;

	v = *vp;
	v4 = v & 0x0f;
	--v;
	setzsp(v);
	*vp = v;

	--v4;
	if (v4 & 0x10)
		freg |= AC_FLG;
	}

static void inrm( void )
	{
	U8 m;

	m = ram[hlreg];
	inr(&m);
	memw(hlreg, m);
	}

static void dcrm( void )
	{
	U8 m;

	m = ram[hlreg];
	dcr(&m);
	memw(hlreg, m);
	}


/*
 *  And/Or/Xor functions, result always in A.
 */
static void ana( U8 v )
	{

	areg &= v;
	setzsp(areg);
	freg &= ~C_FLG;
	}

static void xra( U8 v )
	{

	areg ^= v;
	setzsp(areg);
	freg &= ~C_FLG;
	}

static void ora( U8 v )
	{

	areg |= v;
	setzsp(areg);
	freg &= ~C_FLG;
	}


/*
 * Rotate functions, affect only Carry.
 */
static void rlc( void )
	{
	U16 tw;

	tw = areg << 1;
	areg = (U8)tw;
	if (tw & 0x0100)
		{
		areg |= 1;
		freg |= C_FLG;
		}
	else
		freg &= ~C_FLG;
	}

static void rrc( void )
	{
	U8 tc;

	tc = areg & 1;
	areg >>= 1;
	if (tc)
		{
		areg |= 0x80;
		freg |= C_FLG;
		}
	else
		freg &= ~C_FLG;
	}


static void ral( void )
	{
	U8 tc, oldc;

	tc = areg & 0x80;
	oldc = freg & C_FLG;
	areg <<= 1;

	if (oldc)
		areg |= 0x01;

	if (tc)
		freg |= C_FLG;
	else
		freg &= ~C_FLG;
	}

static void rar( void )
	{
	U8 tc, oldc;

	tc = areg & 0x01;
	oldc = freg & C_FLG;
	areg >>= 1;

	if (oldc)
		areg |= 0x80;

	if (tc)
		freg |= C_FLG;
	else
		freg &= ~C_FLG;
	}



/*
 * IN from port.
 */
static void in8080port( U8 port )
	{
//static int casscount = 0;

	if (port == 0x18 || port == 0xf8)	// keyboard input
		{
		areg = kbport;
		return;
		}

	if (port == 1)					// USART status register
		{
		if (cass_data_avail())
			areg = 0x02;	// RxD available bit
		else
			areg = 0;

		return;
		}

	if (port == 0)					// USART data register
		{
		usart_opcount = 0;				// reset the "timer"

		if (cass_data_avail())
			{
			areg = fgetc(cass_fp);
			}
		else
			areg = 0xff;
		}
	else
		areg = 0xff;	// no data
	}

/*
 * OUT to port.
 */
static void out8080port( U8 port )
	{
	switch (port)
		{
		case 0x0c:			// single step enable
			setimask(INT_SSM);
			break;

		case 0x01:			// USART control, open/close cassette file
			usart_control();
			break;

		}
	}

/*
 * SYSCALL:  format is ED ED nn
 */
static void opsyscall( void )
	{
	U8 nn;

	pcreg++;				// skip 2nd ED
	nn = ram[pcreg++];		// get syscall type
	syscall = nn;
	breakem();
	}


/************************************************************************
 *
 */

void breakem( void )
	{

	emcount = 0;
	}


void startemu( S16 howmany )
	{
	U8 opcode, tblo, tbhi, tim, rst_val;
	U16 tw;
	S16 match_i, match_t;

	emcount = howmany;
	while (emcount-- > 0)
		{

		// see if we're doing artificial delays
		if (match_flag)
			{
			for (match_i = match_t = 0; match_i < match_delays; ++match_i)
				{
				++match_t;
				match_t *= match_i;
				}
			}

		rst_val = 0;

		// check for pending interrupts, or countdowns to EI, SS
		if (imask)
			{
			if (imask & INT_EI2)
				{
				imask ^= INT_EI2;
				imask |= INT_EI1;
				}
			else if (imask & INT_EI1)
				{
				imask ^= INT_EI1;
				iflag = 1;				// time to EI for real
				}

			tim = (imask & INT_SSM);	// counts 3-2-1 down to single-step
			if (tim != 0)
				{
				imask &= ~INT_SSM;
				--tim;
				imask |= tim;
				if (tim == 0)
					imask |= INT_SS;
				}
			}

		// see if time to do a USART receive data interrupt
		if ((last_port1 & 0x04)					// if receiver enabled
			&& usart_opcount > USART_OPCOUNT	// and some time has gone by
			&& cass_data_avail())				// and something to read
			{
			imask |= INT_US;
			usart_opcount = 0;
			}

		// see if it's time to do an adaptive RTC interrupt
		if (rtc_opcount_max > 0 && rtc_opcount >= rtc_opcount_max)
			{
			imask |= INT_RT;
			rtc_opcount = 0;
			}

		// see if there's actually an interrupt pending
		if (iflag && (imask & (INT_RT | INT_SS | INT_KB | INT_US)))
			{
			if (imask & INT_SS)
				{
				imask ^= INT_SS;
				rst_val = RST7;
				}
			else if (imask & INT_RT)
				{
				imask ^= INT_RT;
				rst_val = RST6;
				}
			else if (imask & INT_KB)
				{
				imask ^= INT_KB;
				rst_val = RST5;
				}
			else if (imask & INT_US)
				{
				imask ^= INT_US;
				rst_val = RST4;
				}
			}

		if (rst_val != 0)
			{
			iflag = 0;			// disable, we have an interrupt
			opcode = rst_val;	// do a RST for interrupt service
			}
		else
			opcode = ram[pcreg++];		// get next instruction

		++opcount;			// # opcodes executed for averaging
		++usart_opcount;	// # since last USART interrupt
		++rtc_opcount;		// # since RTC ticked

		switch (opcode)
			{
case 0x00: /* NOP       */							break;
case 0x01: /* LXI B,    */	creg = ram[pcreg++];
							breg = ram[pcreg++];	break;
case 0x02: /* STAX B    */	memw(bcreg, areg);		break;
case 0x03: /* INX B     */	++bcreg;				break;
case 0x04: /* INR B     */	inr(&breg);				break;
case 0x05: /* DCR B     */	dcr(&breg);				break;
case 0x06: /* MVI B,    */	breg = ram[pcreg++];	break;
case 0x07: /* RLC       */	rlc();					break;
case 0x08: /* -         */							break;
case 0x09: /* DAD B     */	dad(bcreg);				break;
case 0x0a: /* LDAX B    */	areg = ram[bcreg];		break;
case 0x0b: /* DCX B     */	--bcreg;				break;
case 0x0c: /* INR C     */	inr(&creg);				break;
case 0x0d: /* DCR C     */	dcr(&creg);				break;
case 0x0e: /* MVI C,    */	creg = ram[pcreg++];	break;
case 0x0f: /* RRC       */	rrc();					break;

case 0x10: /* -         */							break;
case 0x11: /* LXI D,    */	ereg = ram[pcreg++];
							dreg = ram[pcreg++];	break;
case 0x12: /* STAX D    */	memw(dereg, areg);		break;
case 0x13: /* INX D     */	++dereg;				break;
case 0x14: /* INR D     */	inr(&dreg);				break;
case 0x15: /* DCR D     */	dcr(&dreg);				break;
case 0x16: /* MVI D,    */	dreg = ram[pcreg++];	break;
case 0x17: /* RAL       */	ral();					break;
case 0x18: /* -         */							break;
case 0x19: /* DAD D     */	dad(dereg);				break;
case 0x1a: /* LDAX D    */	areg = ram[dereg];		break;
case 0x1b: /* DCX D     */	--dereg;				break;
case 0x1c: /* INR E     */	inr(&ereg);				break;
case 0x1d: /* DCR E     */	dcr(&ereg);				break;
case 0x1e: /* MVI E,    */	ereg = ram[pcreg++];	break;
case 0x1f: /* RAR       */	rar();					break;

case 0x20: /* RIM       */							break;
case 0x21: /* LXI H,    */	lreg = ram[pcreg++];
							hreg = ram[pcreg++];	break;
case 0x22: /* SHLD      */	tblo = ram[pcreg++];
							tbhi = ram[pcreg++];
							tw = (tbhi << 8) | tblo;
							memw(tw, lreg);
							memw(tw + 1, hreg);		break;
case 0x23: /* INX H     */	++hlreg;				break;
case 0x24: /* INR H     */	inr(&hreg);				break;
case 0x25: /* DCR H     */	dcr(&hreg);				break;
case 0x26: /* MVI H,    */	hreg = ram[pcreg++];	break;
case 0x27: /* DAA       */	daa();					break;
case 0x28: /* -         */							break;
case 0x29: /* DAD H     */	dad(hlreg);				break;
case 0x2a: /* LHLD      */	tblo = ram[pcreg++];
							tbhi = ram[pcreg++];
							tw = (tbhi << 8) | tblo;
							lreg = ram[tw];
							hreg = ram[tw + 1];		break;
case 0x2b: /* DCX H     */	--hlreg;				break;
case 0x2c: /* INR L     */	inr(&lreg);				break;
case 0x2d: /* DCR L     */	dcr(&lreg);				break;
case 0x2e: /* MVI L,    */	lreg = ram[pcreg++];	break;
case 0x2f: /* CMA       */	areg = ~areg;			break;

case 0x30: /* SIM       */							break;
case 0x31: /* LXI SP,   */	tblo = ram[pcreg++];
							tbhi = ram[pcreg++];
							spreg = (tbhi << 8) | tblo;	break;
case 0x32: /* STA       */	tblo = ram[pcreg++];
							tbhi = ram[pcreg++];
							memw((tbhi << 8) | tblo, areg);	break;
case 0x33: /* INX SP    */	++spreg;					break;
case 0x34: /* INR M     */	inrm();						break;
case 0x35: /* DCR M     */	dcrm();						break;
case 0x36: /* MVI M,    */	memw(hlreg, ram[pcreg++]);	break;
case 0x37: /* STC       */	freg |= C_FLG;				break;
case 0x38: /* -         */								break;
case 0x39: /* DAD SP    */	dad(spreg);					break;
case 0x3a: /* LDA       */	tblo = ram[pcreg++];
							tbhi = ram[pcreg++];
							areg = ram[(tbhi << 8) | tblo];	break;
case 0x3b: /* DCX SP    */	--spreg;					break;
case 0x3c: /* INR A     */	inr(&areg);					break;
case 0x3d: /* DCR A     */	dcr(&areg);					break;
case 0x3e: /* MVI A,    */	areg = ram[pcreg++];		break;
case 0x3f: /* CMC       */	freg ^= C_FLG;				break;

case 0x40: /* MOV B,B   */						break;
case 0x41: /* MOV B,C   */	breg = creg;		break;
case 0x42: /* MOV B,D   */	breg = dreg;		break;
case 0x43: /* MOV B,E   */	breg = ereg;		break;
case 0x44: /* MOV B,H   */	breg = hreg;		break;
case 0x45: /* MOV B,L   */	breg = lreg;		break;
case 0x46: /* MOV B,M   */	breg = ram[hlreg];	break;
case 0x47: /* MOV B,A   */	breg = areg;		break;

case 0x48: /* MOV C,B   */	creg = breg;		break;
case 0x49: /* MOV C,C   */						break;
case 0x4a: /* MOV C,D   */	creg = dreg;		break;
case 0x4b: /* MOV C,E   */	creg = ereg;		break;
case 0x4c: /* MOV C,H   */	creg = hreg;		break;
case 0x4d: /* MOV C,L   */	creg = lreg;		break;
case 0x4e: /* MOV C,M   */	creg = ram[hlreg];	break;
case 0x4f: /* MOV C,A   */	creg = areg;		break;

case 0x50: /* MOV D,B   */	dreg = breg;		break;
case 0x51: /* MOV D,C   */	dreg = creg;		break;
case 0x52: /* MOV D,D   */						break;
case 0x53: /* MOV D,E   */	dreg = ereg;		break;
case 0x54: /* MOV D,H   */	dreg = hreg;		break;
case 0x55: /* MOV D,L   */	dreg = lreg;		break;
case 0x56: /* MOV D,M   */	dreg = ram[hlreg];	break;
case 0x57: /* MOV D,A   */	dreg = areg;		break;

case 0x58: /* MOV E,B   */	ereg = breg;		break;
case 0x59: /* MOV E,C   */	ereg = creg;		break;
case 0x5a: /* MOV E,D   */	ereg = dreg;		break;
case 0x5b: /* MOV E,E   */						break;
case 0x5c: /* MOV E,H   */	ereg = hreg;		break;
case 0x5d: /* MOV E,L   */	ereg = lreg;		break;
case 0x5e: /* MOV E,M   */	ereg = ram[hlreg];	break;
case 0x5f: /* MOV E,A   */	ereg = areg;		break;

case 0x60: /* MOV H,B   */	hreg = breg;		break;
case 0x61: /* MOV H,C   */	hreg = creg;		break;
case 0x62: /* MOV H,D   */	hreg = dreg;		break;
case 0x63: /* MOV H,E   */	hreg = ereg;		break;
case 0x64: /* MOV H,H   */						break;
case 0x65: /* MOV H,L   */	hreg = lreg;		break;
case 0x66: /* MOV H,M   */	hreg = ram[hlreg];	break;
case 0x67: /* MOV H,A   */	hreg = areg;		break;

case 0x68: /* MOV L,B   */	lreg = breg;		break;
case 0x69: /* MOV L,C   */	lreg = creg;		break;
case 0x6a: /* MOV L,D   */	lreg = dreg;		break;
case 0x6b: /* MOV L,E   */	lreg = ereg;		break;
case 0x6c: /* MOV L,H   */	lreg = hreg;		break;
case 0x6d: /* MOV L,L   */						break;
case 0x6e: /* MOV L,M   */	lreg = ram[hlreg];	break;
case 0x6f: /* MOV L,A   */	lreg = areg;		break;

case 0x70: /* MOV M,B   */	memw(hlreg, breg);	break;
case 0x71: /* MOV M,C   */	memw(hlreg, creg);	break;
case 0x72: /* MOV M,D   */	memw(hlreg, dreg);	break;
case 0x73: /* MOV M,E   */	memw(hlreg, ereg);	break;
case 0x74: /* MOV M,H   */	memw(hlreg, hreg);	break;
case 0x75: /* MOV M,L   */	memw(hlreg, lreg);	break;
case 0x76: /* HLT       */						break;
case 0x77: /* MOV M,A   */	memw(hlreg,  areg);	break;

case 0x78: /* MOV A,B   */	areg = breg;		break;
case 0x79: /* MOV A,C   */	areg = creg;		break;
case 0x7a: /* MOV A,D   */	areg = dreg;		break;
case 0x7b: /* MOV A,E   */	areg = ereg;		break;
case 0x7c: /* MOV A,H   */	areg = hreg;		break;
case 0x7d: /* MOV A,L   */	areg = lreg;		break;
case 0x7e: /* MOV A,M   */	areg = ram[hlreg];
			// see if we can speed up the USART receiving.
			if (poly88_flag &&			// if we're in 88 mode
				hlreg == 0x0c0a &&      // and he's reading the RBUFF flag
				areg == 0xff &&			// and getting FF - not ready
				(last_port1 & 0x04)	&& 	// and receiver enabled
				usart_opcount < USART_OPCOUNT - 10 &&
				cass_data_avail())		// and something to read
				{
				usart_opcount = USART_OPCOUNT - 1;	// we're about ready
				}
												break;
case 0x7f: /* MOV A,A   */						break;

case 0x80: /* ADD B     */	add(breg, 0);		break;
case 0x81: /* ADD C     */	add(creg, 0);		break;
case 0x82: /* ADD D     */	add(dreg, 0);		break;
case 0x83: /* ADD E     */	add(ereg, 0);		break;
case 0x84: /* ADD H     */	add(hreg, 0);		break;
case 0x85: /* ADD L     */	add(lreg, 0);		break;
case 0x86: /* ADD M     */	add(ram[hlreg], 0);	break;
case 0x87: /* ADD A     */	add(areg, 0);		break;
case 0x88: /* ADC B     */	add(breg, CARRY);	break;
case 0x89: /* ADC C     */	add(creg, CARRY);	break;
case 0x8a: /* ADC D     */	add(dreg, CARRY);	break;
case 0x8b: /* ADC E     */	add(ereg, CARRY);	break;
case 0x8c: /* ADC H     */	add(hreg, CARRY);	break;
case 0x8d: /* ADC L     */	add(lreg, CARRY);	break;
case 0x8e: /* ADC M     */	add(ram[hlreg], CARRY);	break;
case 0x8f: /* ADC A     */	add(areg, CARRY);	break;

case 0x90: /* SUB B     */	sub(breg, 0);		break;
case 0x91: /* SUB C     */	sub(creg, 0);		break;
case 0x92: /* SUB D     */	sub(dreg, 0);		break;
case 0x93: /* SUB E     */	sub(ereg, 0);		break;
case 0x94: /* SUB H     */	sub(hreg, 0);		break;
case 0x95: /* SUB L     */	sub(lreg, 0);		break;
case 0x96: /* SUB M     */	sub(ram[hlreg], 0);	break;
case 0x97: /* SUB A     */	sub(areg, 0);		break;
case 0x98: /* SBB B     */	sub(breg, CARRY);	break;
case 0x99: /* SBB C     */	sub(creg, CARRY);	break;
case 0x9a: /* SBB D     */	sub(dreg, CARRY);	break;
case 0x9b: /* SBB E     */	sub(ereg, CARRY);	break;
case 0x9c: /* SBB H     */	sub(hreg, CARRY);	break;
case 0x9d: /* SBB L     */	sub(lreg, CARRY);	break;
case 0x9e: /* SBB M     */	sub(ram[hlreg], CARRY);	break;
case 0x9f: /* SBB A     */	sub(areg, CARRY);	break;

case 0xa0: /* ANA B     */	ana(breg);			break;
case 0xa1: /* ANA C     */	ana(creg);			break;
case 0xa2: /* ANA D     */	ana(dreg);			break;
case 0xa3: /* ANA E     */	ana(ereg);			break;
case 0xa4: /* ANA H     */	ana(hreg);			break;
case 0xa5: /* ANA L     */	ana(lreg);			break;
case 0xa6: /* ANA M     */	ana(ram[hlreg]);	break;
case 0xa7: /* ANA A     */	ana(areg);			break;
case 0xa8: /* XRA B     */	xra(breg);			break;
case 0xa9: /* XRA C     */	xra(creg);			break;
case 0xaa: /* XRA D     */	xra(dreg);			break;
case 0xab: /* XRA E     */	xra(ereg);			break;
case 0xac: /* XRA H     */	xra(hreg);			break;
case 0xad: /* XRA L     */	xra(lreg);			break;
case 0xae: /* XRA M     */	xra(ram[hlreg]);	break;
case 0xaf: /* XRA A     */	xra(areg);			break;

case 0xb0: /* ORA B     */	ora(breg);			break;
case 0xb1: /* ORA C     */	ora(creg);			break;
case 0xb2: /* ORA D     */	ora(dreg);			break;
case 0xb3: /* ORA E     */	ora(ereg);			break;
case 0xb4: /* ORA H     */	ora(hreg);			break;
case 0xb5: /* ORA L     */	ora(lreg);			break;
case 0xb6: /* ORA M     */	ora(ram[hlreg]);	break;
case 0xb7: /* ORA A     */	ora(areg);			break;
case 0xb8: /* CMP B     */	cmp(breg);			break;
case 0xb9: /* CMP C     */	cmp(creg);			break;
case 0xba: /* CMP D     */	cmp(dreg);			break;
case 0xbb: /* CMP E     */	cmp(ereg);			break;
case 0xbc: /* CMP H     */	cmp(hreg);			break;
case 0xbd: /* CMP L     */	cmp(lreg);			break;
case 0xbe: /* CMP M     */	cmp(ram[hlreg]);	break;
case 0xbf: /* CMP A     */	cmp(areg);			break;

case 0xc0: /* RNZ       */	retcc((freg & Z_FLG) == 0);		break;
case 0xc1: /* POP B     */	creg = ram[spreg++];
							breg = ram[spreg++]; 			break;
case 0xc2: /* JNZ       */	ijumpcc((freg & Z_FLG) == 0); 	break;
case 0xc3: /* JMP       */	ijump();						break;
case 0xc4: /* CNZ       */	icallcc((freg & Z_FLG) == 0); 	break;
case 0xc5: /* PUSH B    */	memw(--spreg, breg);
							memw(--spreg, creg); 			break;
case 0xc6: /* ADI       */	add(ram[pcreg++], 0);			break;
case 0xc7: /* RST 0     */	call(0, 0 * 8);					break;

case 0xc8: /* RZ        */	retcc(freg & Z_FLG);			break;
case 0xc9: /* RET       */	retcc(1);						break;
case 0xca: /* JZ        */	ijumpcc(freg & Z_FLG);			break;
case 0xcb: /* -         */									break;
case 0xcc: /* CZ        */  icallcc(freg & Z_FLG);			break;
case 0xcd: /* CALL      */	icall(); 						break;
case 0xce: /* ACI       */	add(ram[pcreg++], CARRY);		break;
case 0xcf: /* RST 1     */	call(0, 1 * 8);					break;

case 0xd0: /* RNC       */	retcc((freg & C_FLG) == 0);		break;
case 0xd1: /* POP D     */	ereg = ram[spreg++];
							dreg = ram[spreg++]; 			break;
case 0xd2: /* JNC       */	ijumpcc((freg & C_FLG) == 0);	break;
case 0xd3: /* OUT       */	out8080port(ram[pcreg++]);		break;
case 0xd4: /* CNC       */	icallcc((freg & C_FLG) == 0);	break;
case 0xd5: /* PUSH D    */	memw(--spreg, dreg);
							memw(--spreg, ereg); 			break;
case 0xd6: /* SUI       */	sub(ram[pcreg++], 0);			break;
case 0xd7: /* RST 2     */	call(0, 2 * 8);					break;

case 0xd8: /* RC        */	retcc(freg & C_FLG);			break;
case 0xd9: /* -         */									break;
case 0xda: /* JC        */	ijumpcc(freg & C_FLG); 			break;
case 0xdb: /* IN        */	in8080port(ram[pcreg++]);		break;
case 0xdc: /* CC        */	icallcc(freg & C_FLG);			break;
case 0xdd: /* -         */									break;
case 0xde: /* SBI       */	sub(ram[pcreg++], CARRY);		break;
case 0xdf: /* RST 3     */	call(0, 3 * 8);					break;

case 0xe0: /* RPO       */	retcc((freg & P_FLG) == 0);		break;
case 0xe1: /* POP H     */	lreg = ram[spreg++];
							hreg = ram[spreg++]; 			break;
case 0xe2: /* JPO       */	ijumpcc((freg & P_FLG) == 0);	break;
case 0xe3: /* XTHL      */  tblo = ram[spreg];
							tbhi = ram[spreg+1];
							memw(spreg,   lreg);
							memw(spreg+1, hreg);
							lreg = tblo;
							hreg = tbhi;					break;
case 0xe4: /* CPO       */	icallcc((freg & P_FLG) == 0);	break;
case 0xe5: /* PUSH H    */	memw(--spreg, hreg);
							memw(--spreg, lreg);			break;
case 0xe6: /* ANI       */	ana(ram[pcreg++]);				break;
case 0xe7: /* RST 4     */	call(0, 4 * 8);					break;

case 0xe8: /* RPE       */	retcc(freg & P_FLG);			break;
case 0xe9: /* PCHL      */	pcreg = hlreg;					break;
case 0xea: /* JPE       */	ijumpcc(freg & P_FLG);			break;
case 0xeb: /* XCHG      */	tw = hlreg;
							hlreg = dereg;
							dereg = tw; 					break;
case 0xec: /* CPE       */	icallcc(freg & P_FLG);			break;
case 0xed: /* -         */	opsyscall();					break;
case 0xee: /* XRI       */	xra(ram[pcreg++]);				break;
case 0xef: /* RST 5     */	call(0, 5 * 8);					break;

case 0xf0: /* RP        */	retcc((freg & S_FLG) == 0);		break;
case 0xf1: /* POP PSW   */	freg = ram[spreg++];
							areg = ram[spreg++]; 			break;
case 0xf2: /* JP        */	ijumpcc((freg & S_FLG) == 0);	break;
case 0xf3: /* DI        */	iflag = 0;						break;
case 0xf4: /* CP        */	icallcc((freg & S_FLG) == 0);	break;
case 0xf5: /* PUSH PSW  */	memw(--spreg, areg);
							memw(--spreg, freg);			break;
case 0xf6: /* ORI       */	ora(ram[pcreg++]);				break;
case 0xf7: /* RST 6     */	call(0, 6 * 8);					break;
case 0xf8: /* RM        */	retcc(freg & S_FLG);			break;
case 0xf9: /* SPHL      */	spreg = (hreg << 8) | lreg;		break;
case 0xfa: /* JM        */	ijumpcc(freg & S_FLG);			break;
case 0xfb: /* EI        */	setimask(INT_EI2);			 	break;
case 0xfc: /* CM        */	icallcc(freg & S_FLG);			break;
case 0xfd: /* -         */									break;
case 0xfe: /* CPI       */	cmp(ram[pcreg++]);				break;
case 0xff: /* RST 7     */	call(0, 7 * 8);					break;
			}	// end of switch
		}	// end of while
	}


/*
 * parity table, P_FLG indicates even parity.
 */
U8 paritytable[256] = {
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG, P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0,
	P_FLG, 0, 0, P_FLG, 0, P_FLG, P_FLG, 0, 0, P_FLG, P_FLG, 0, P_FLG, 0, 0, P_FLG,
	};

