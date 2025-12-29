/*
 * pmath.c - Poly Emulator math routines to emulate the North Star
 * floating-point board.
 *
 * rdb 9/16/90
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "pmhdr.h"

#define FPDEBUG 0		/* define as 1 to enable printouts */


char *fp_ops[] = {"???", "add", "sub", "mul", "div"};

/*****************************************************************
 * This code can be compiled stand-alone or as part of the
 * PM program.  Change the #define below to accomplish this.
 *****************************************************************/

#define TEST_MATH 0		/* 1 when compiling alone, else 0 */

#if TEST_MATH
#include <math.h>
#include <ctype.h>

double epsilon;
#endif

/*****************************************************************
 * Start of code which exists in both stand-alone and PM versions.
 *****************************************************************/

#define OP_ADD 1
#define OP_SUB 2
#define OP_MUL 3
#define OP_DIV 4
char op_chars[] = "?+-*/";

#define MAXDIGITS 14
#define ACCUM_LEN (2 * MAXDIGITS + 5)
#define ACCUM_MID (ACCUM_LEN / 2)

S16 digits = 4;
S16 digit_bytes;
S16 oflo_flag;
S16 math_op = OP_DIV;

typedef struct
	{
	char array[ACCUM_LEN + MAXDIGITS];	/* number is stored 1 digit per byte */
	S16 exp;				/* zero-biased */
	S16 sign;				/* 1 or -1 */
	char *ptr;				/* pointer to MS digit (leftmost) */
	} FPARG;

FPARG yarg, larg, rarg, save_larg;		/* y = l <op> r */
S16 mres[ACCUM_LEN];		/* result for multiplies */

/* If the floating-point number's exponent (after unbiasing) is -64,
 * it means the exponent byte in FPB format was 00, and the entire
 * number is therefore defined to be zero.
 */
#define ZERO_EXP	-64

FPARG fp_zero =
	{
	{0},
	ZERO_EXP,				/* will become zero when biased by 64 */
	1,
	&fp_zero.array[ACCUM_MID]
	};


/*
 * prototypes
 */
void ns_to_asc( U16 addr, FPARG *arg );
void asc_to_ns( FPARG *arg, U16 addr );
void print_fparg( FPARG *arg );
void do_ops( char *left, char *right );
FPARG *fpb_add( void );
FPARG *fpb_mul( void );
FPARG *fpb_div( void );
void ten_comp( FPARG *arg );
FPARG *round( FPARG *arg );

#if TEST_MATH
void text_to_num( char *text, FPARG *arg, double *d );
double disp_num( FPARG *arg );
void main( void );
#endif



/*
 * Enter here when a FPB operation would be performed.
 *   areg =  FPB command byte, [digits:4 command:4]
 *   dereg -> right argument
 *   bcreg -> left argument
 */
void sysmathop( void )
	{
	FPARG *result;

	digits = (areg >> 4) & 0xf;
	digit_bytes = digits / 2;
	math_op = areg & 0xf;

#if 0
	cprintf("%d digits, %s   left side: ", digits, fp_ops[math_op]);
	for (i = 0; i < digits / 2 + 1; i++)
		cprintf("%02x ", ram[bcreg - i]);
	cprintf("   right side: ");
	for (i = 0; i < digits / 2 + 1; i++)
		cprintf("%02x ", ram[dereg - i]);
	cprintf("\r\n");
#endif

	/* Adjust bc and de to point to start of number, just as
	 * the original FPB code would have.
	 */
	bcreg -= digit_bytes;
	dereg -= digit_bytes;
	hlreg = 0x1ffd;				/* BASIC did this too... */

	ns_to_asc(bcreg, &larg);	/* convert to our ASCII format */
	ns_to_asc(dereg, &rarg);

#if 0
	print_fparg(&larg);
	cprintf(" %c ", op_chars[math_op]);
	print_fparg(&rarg);
	cprintf("  = ");
#endif

	freg = Z_FLG;			/* init result-flag to no error */
	oflo_flag = 0;			/* no overflow yet */
	switch (math_op)
		{
		case OP_SUB:
			rarg.sign = -rarg.sign;		/* and fall into add */

		case OP_ADD:
			result = fpb_add();
			result = round(result);
			break;

		case OP_MUL:
			result = fpb_mul();
			result = round(result);
			break;

		case OP_DIV:
			result = fpb_div();
			result = round(result);
			break;

		default:
			cprintf("Bad FPB command: %d\r\n", math_op);
			break;
		}

	if (oflo_flag)
		freg = 0;	/* had an overflow */

	asc_to_ns(result, bcreg);
	}



/*
 * Convert North-Star FP format to our FPARG structure.
 */
void ns_to_asc( U16 addr, FPARG *arg )
	{
	char *p;
	U8  *rp;
	S16 i, d;

	memset(arg->array, 0, ACCUM_LEN);
	p = &arg->array[ACCUM_MID];
	arg->ptr = p;
	rp = &ram[addr];
	for (i = 0; i < digit_bytes; ++i)
		{
		d = *rp++;
		*p++ = (d >> 4) & 0xf;
		*p++ = d & 0xf;
		}

	d = *rp;			/* get exponent/sign byte */
	if (d & 0x80)
		arg->sign = -1;
	else
		arg->sign = 1;

	arg->exp = (d & 0x7f) - 64;
	}


/*
 * Convert FPARG to FPB format.
 */
void asc_to_ns( FPARG *arg, U16 addr )
	{
	U8 d, *rp;
	char *p;
	S16 i;

	p = arg->ptr;
	rp = &ram[addr];
	for (i = 0; i < digit_bytes; ++i)
		{
		d = *p++ << 4;
		d |= *p++;
		*rp++ = d;
		}

	d = arg->exp + 64;
	if (arg->sign < 0)
		d |= 0x80;
	*rp = d;
	}


/*
 * Print an FPARG using cprintf.
 */
void print_fparg( FPARG *arg )
	{
	S16 i;

	if (arg->sign < 0)
		cprintf("-0.");
	else
		cprintf(" 0.");

	for (i = 0; i < digits; ++i)
		cprintf("%c", arg->ptr[i] + '0');

	cprintf(" E%3d", arg->exp);
	}



/*
 * Add.
 */
FPARG *fpb_add( void )
	{
	char *lp, *rp;
	S16 ediff, i, t, reversed, result_sign;

	/* Line up decimal points by shifting one of the operands
	 * if their exponents are unequal.  Make lp and rp point to the
	 * rounding digit (one place to the right of the LS digit).
	 */
	lp = &larg.array[ACCUM_MID + digits];
	rp = &rarg.array[ACCUM_MID + digits];
	if ((ediff = larg.exp - rarg.exp) != 0)
		{
		if (ediff > 0)		/* lexp > rexp, shift right rarg */
			{
			if (ediff > digits + 1)
				return (&larg);
			else
				{
				rp -= ediff;
				rarg.exp += ediff;
				}
			}
		else				/* lexp < rexp, shift right larg */
			{
			ediff = -ediff;
			if (ediff > digits + 1)
				return (&rarg);
			else
				{
				lp -= ediff;
				larg.exp += ediff;
				}
			}
		}

	larg.ptr = lp - digits;		/* remember MS digit position of args */
	rarg.ptr = rp - digits;

	/* If both have the same sign, do an add.
	 * The result will have the sign of both addends.
	 */
	if (larg.sign == rarg.sign)
		{
		/* Do the add.  larg += rarg, so result will be in larg.
		 * Do N+1 digits starting one to the right of the LS, just in case
		 * a shift occurred above.
		 */
		for (i = 0; i <= digits; ++i)
			{
			t = *lp + *rp;
			if (t > 9)
				{
				t -= 10;
				++lp[-1];
				}
			*lp = t;
			--lp;
			--rp;
			}
		}
	else
		{
		/* If larg < 0 and rarg > 0, we need to perform -larg + rarg.
		 * We would prefer to do larg - rarg, and reverse it later.
		 */
		reversed = (larg.sign < 0) ? -1 : 1;	/* remember to reverse later */

		/* Do the subtract.  larg -= rarg, so result will be in larg.
		 * Do N+1 digits starting one to the right of the LS, just in case
		 * a shift occurred above.
		 */
		for (i = 0; i <= digits; ++i)
			{
			t = *lp - *rp;
			if (t < 0)
				{
				t += 10;
				--lp[-1];
				}
			*lp = t;
			--lp;
			--rp;
			}

		/* If we borrowed, ten's-comp the value and reverse the sign.
		 */
		if (*lp < 0)
			{
			result_sign = -1;		/* remember result was negative */
			*lp++ = 0;				/* kill the borrow digit, point to MS digit */
			for (i = 0; i <= digits; ++i, ++lp)
				*lp = 9 - *lp;		/* 9's complement */
			--lp;					/* back up to the rounding digit */
			++*lp;					/* increment it for 10's-comp */
			while (*lp > 9)			/* while it needs carry */
				{
				++lp[-1];
				*lp -= 10;
				--lp;
				}
			}
		else
			result_sign = 1;		/* result was positive */

		larg.sign = result_sign * reversed;
		}

	if (larg.ptr[-1] > 0)	/* check for carry out of MS digit */
		{
		--larg.ptr;
		++larg.exp;			/* carry occurred */
		}
	else 		/* we'll need to renormalize by shifting larg left, */
		{		/* if it's now pointing to a zero */
		for (i = 0; *larg.ptr == 0 && i < digits; ++i)
			{
			++larg.ptr;
			--larg.exp;
			}
		}

	return (&larg);
	}



/*
 * Round the argument and re-normalize after rounding, if needed.
 * On entry, arg->ptr must point to MS digit, number must be normalized.
 */
FPARG *round( FPARG *arg )
	{
	char *tp;

	tp = arg->ptr + digits;		/* tp -> rounding digit, which will be lost */
	if (*tp >= 5)				/* if rounding is required */
		{
		*tp = 0;			/* kill the rounding digit */
		--tp;				/* move left one, to LS digit */
		++*tp;				/* add 1 to round it up */
		while (*tp > 9)		/* while it needs carry */
			{
			++tp[-1];
			*tp -= 10;
			--tp;
			}

		if (arg->ptr[-1] > 0)			/* recheck MS-1 digit */
			{
			--arg->ptr;
			++arg->exp;			/* MS carry occurred */
			}
		}

	if (*arg->ptr == 0 || arg->exp < -63)
		return (&fp_zero);		/* underflow occurred */

	if (arg->exp > 63)
		{
		oflo_flag = 1;			/* overflow occurred */
		return (&fp_zero);
		}

	return (arg);
	}


/*
 * Multiply.
 */
FPARG *fpb_mul( void )
	{
	char *lp, *rp;
	S16 i, t, il, ir, *mp;

	memset(mres, 0, sizeof(mres));		/* clear result */

	for (ir = digits - 1; ir >= 0; --ir)
		{
		rp = &rarg.array[ACCUM_MID + ir];
		mp = &mres[ACCUM_MID + ir];
		lp = &larg.array[ACCUM_MID + digits - 1];
		for (il = digits - 1; il >= 0; --il)
			*mp-- += *lp-- * *rp;
		}


	/* Do the carrying, as we move the result into larg.
	 */
	mp = &mres[ACCUM_MID + digits - 1];			/* point to LS digit */
	lp = &larg.array[ACCUM_MID + digits - 1];	/* here too */
	for (i = 2 * digits; i > 0; --i)
		{
		t = *mp--;
		*lp-- = t % 10;
		*mp += t / 10;
		}

	larg.sign *= rarg.sign;		/* sign of result */
	larg.exp += rarg.exp;		/* exponent of result (may get adjusted below) */
	++lp;						/* point to MS of result */
	if (*lp == 0)				/* if underflowed */
		{
		++lp;
		--larg.exp;
		}

	larg.ptr = lp;
	return (&larg);
	}


/*
 * Divide.
 */
FPARG *fpb_div( void )
	{
	char *lp, *rp, *yp;
	S16 i, t, qd;

	if (larg.exp == ZERO_EXP)	/* 0 / n = 0 */
		return (&fp_zero);

	if (rarg.exp == ZERO_EXP)
		{
		oflo_flag = 1;			/* divide by zero */
		return (&fp_zero);
		}

	yarg = fp_zero;						/* clear result */
	yarg.sign = larg.sign * rarg.sign;	/* get its sign */

	/* point to rounding digit of larg, rarg
	 */
	larg.ptr = &larg.array[ACCUM_MID + digits];
	rarg.ptr = &rarg.array[ACCUM_MID + digits];

	yp = yarg.ptr = &yarg.array[ACCUM_MID];

	/* Perform the division loop for DIGITS + 2 digits of the quotient.
	 */
	for (qd = digits + 1; qd > 0; --qd, ++yp, ++larg.ptr)
		{
		while (1)
			{
			/* subtract: larg = larg - rarg.
			 */
			save_larg = larg;		/* save the dividend */
			lp = larg.ptr;
			rp = rarg.ptr;
			for (i = 0; i <= digits; ++i)
				{
				t = *lp - *rp;
				if (t < 0)
					{
					t += 10;
					--lp[-1];
					}
				*lp = t;
				--lp;
				--rp;
/* cprintf("i: %d  ", i);
for (ii = ACCUM_MID; ii < ACCUM_LEN; ii++)
	cprintf("%d", larg.array[ii]);
cprintf("  %d\r\n", larg.exp);
getch(); */
				}

			/* If we borrowed, time to restore larg and end this iteration.
			 */
			if (*lp < 0)
				{
				larg = save_larg;
				break;
				}
			else
				++*yp;		/* add to quotient */
			}
/* for (ii = ACCUM_MID; ii < ACCUM_LEN; ii++)
	cprintf("%d", larg.array[ii]);
cprintf("  %d\r\n", larg.exp);
getch(); */
		}


	/* make exponent of result
	 */
	yarg.exp = larg.exp - rarg.exp;
	if (*yarg.ptr > 0)		/* if we got a 1st digit in quotient */
		++yarg.exp;			/* adjust the exponent */
	else
		++yarg.ptr;			/* else point to the 1st digit */

	return (&yarg);
	}



/*****************************************************************
 * Start of code which only exists in stand-alone version.
 *****************************************************************/

#if TEST_MATH

/*
 * Convert input text into usable form.
 * (Only for testing.)
 */
void text_to_num( char *text, FPARG *arg, double *d )
	{
	char c, *aptr;
	S16 digits_got, had_dp, spec_exp;

	memset(arg->array, 0, ACCUM_LEN);

	arg->sign = 1;					/* parse a leading sign */
	if (*text == '-')
		{
		++text;
		arg->sign = -1;
		}
	else if (*text == '+')
		++text;

	aptr = &arg->array[ACCUM_MID];	/* point to center of array */
	arg->ptr = aptr;
	arg->exp = 0;					/* init exponent */
	had_dp = 0;						/* no . found yet */
	digits_got = 0;					/* no digits gotten yet */
	while (isdigit(c = *text++) || c == '.')
		{
		if (c == '.')
			had_dp = 1;
		else
			{
			if (c > '0' || digits_got > 0)
				{
				if (digits_got >= digits)
					c = 0;			/* got enough significant ones already */

				*aptr++ = c & 0xf;	/* insert in array, moving right */
				++digits_got;
				if (!had_dp)
					++arg->exp;
				}
			}
		}

	if (tolower(c) == 'e')		/* if exponent specified */
		{
		sscanf(text, "%d", &spec_exp);		/* scan it */
		arg->exp += spec_exp;				/* add to exponent so far */
		}

	*d = disp_num(arg);
	}


/*
 * Display an ASCII-numeric value.
 */
double disp_num( FPARG *arg )
	{
	S16 i, ap, dig, exp;
	char sign;
	double x = 0.0, t = 0.1;

	sign = (arg->sign == -1) ? '-' : '+';
	printf("%c", sign);
	ap = arg->ptr - arg->array;
	for (i = 0; i < ACCUM_LEN; ++i)
		{
		dig = arg->array[i];
		if (i == ap)
			printf(".");
		printf("%c", dig + '0');
		if (i >= ap && i - ap < digits)
			{
			x += dig * t;
			t /= 10.0;
			}
		}

	if ((exp = arg->exp) >= 0)
		t = 10.0;
	else
		{
		t = 0.1;
		exp = -exp;
		}
	for (i = 0; i < exp; ++i)
		x *= t;

	printf(" E %d    %c%g\n", arg->exp, sign, x);
	if (sign == '-')
		x = -x;
	return (x);
	}


/*
 * Main stand-alone program.  Gets values from command line and
 * performs an operation on them.
 */
void main( void )
	{
	char cmdline[80], cmd[20], arg1[20], arg2[20];
	S16 nargs;

	epsilon = pow10(-1 - digits);
	while (1)
		{
		printf("math> ");
		gets(cmdline);
		cmd[0] = '\0';
		arg1[0] = '\0';
		arg2[0] = '\0';
		nargs = sscanf(cmdline, "%s %s %s", cmd, arg1, arg2);
		if (cmd[0] == '\0')
			;
		else if (strnicmp(cmd, "add", 3) == 0)
			math_op = OP_ADD;
		else if (strnicmp(cmd, "sub", 3) == 0)
			math_op = OP_SUB;
		else if (strnicmp(cmd, "mul", 3) == 0)
			math_op = OP_MUL;
		else if (strnicmp(cmd, "div", 3) == 0)
			math_op = OP_DIV;
		else if (nargs == 1 || nargs == 2)
			do_ops(cmd, arg1);
		else if (*cmd == 'q')
			exit(1);
		else
			printf("what?\n");
		}
	}


/*
 * Do math operations.  left and right are ASCII multiprecision numbers.
 */
void do_ops( char *left, char *right )
	{
	FPARG *result;
	double flarg, frarg, fresult;

	printf("%s %c %s\n", left, op_chars[math_op], right);
	text_to_num(left, &larg, &flarg);
	text_to_num(right, &rarg, &frarg);

	oflo_flag = 0;		/* no overflow yet */
	switch (math_op)
		{
		case OP_SUB:
			rarg.sign = -rarg.sign;		/* and fall into add */

		case OP_ADD:
			result = fpb_add();
			result = round(result);
			fresult = disp_num(result);
			if (fresult != 0.0 && fabs(flarg + frarg - fresult) / fresult > epsilon)
				printf("***** error!  %g %g %g\n", flarg, frarg, fresult);
			break;

		case OP_MUL:
			result = fpb_mul();
			result = round(result);
			fresult = disp_num(result);
			if (fresult != 0.0 && fabs(flarg * frarg - fresult) / fresult > epsilon)
				printf("***** error!  %g %g %g\n", flarg, frarg, fresult);
			break;

		case OP_DIV:
			result = fpb_div();
			result = round(result);
			fresult = disp_num(result);
			if (fresult != 0.0 && fabs(flarg / frarg - fresult) / fresult > epsilon)
				printf("***** error!  %g %g %g\n", flarg, frarg, fresult);
			break;

		default:
			break;
		}

	if (oflo_flag)
		printf("**** overflow!\n");

	}

#endif		/* #if TEST_MATH */
