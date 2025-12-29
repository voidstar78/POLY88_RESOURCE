/*
 * Header for PolyMorphic Emulator, pm.exe
 */


/*
 * general purpose types
 */
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef char S8;
typedef short S16;
typedef long S32;



/*
 * Miscellaneous values
 */
#define SCREENSIZE (80 * 25 * 2)	/* PC screen memory */
#define WINY0 19					/* lower window, y0 coord */

/*
 * special RAM locations
 */
#define DONT 	0x2d90
#define TIMERVALUE 0x0c00
#define WH8  	0x0c40
#define TIMECODE 0x0040
#define POS 	0x0c0e
#define SCEND 	0x0c1e
#define FPSTART 0x1ff0		/* North Star floating-point board, start */
#define FPSIZE 	16			/* and size */

#define SSTART_8813	0x1800		/* screen memory start */
#define SSTART_88	0xf800
#define SSIZE		1024		/* and size */

#define SYSRES	0x2d92
#define NFDIR	0x2da0

/*
 * bits in flag register
 */
#define C_FLG	0x01
#define P_FLG	0x04
#define AC_FLG	0x10
#define Z_FLG	0x40
#define S_FLG	0x80

/*
 * bits in imask
 */
#define INT_SS 0x80		/* single-step, RST 7 */
#define INT_RT 0x40		/* 60 Hz clock, RST 6 */
#define INT_KB 0x20		/* keyboard,    RST 5 */
#define INT_US 0x10		/* USART, 		RST 4 */
#define INT_EI2	0x08	/* EI after 2nd instruction */
#define INT_EI1	0x04	/* EI after next instruction */
#define INT_SSM	0x03	/* SS after 2-1-0 instruction */

/*
 * Poly disk/directory information
 */
#define F_DEL 0x80		/* file flag bits */
#define F_SYS 0x40
#define F_NEW 0x20
#define F_LEN 0x1f
#define SECSIZE 256		/* file sector size */


/*
 * structure of Poly directory
 */
typedef struct
	{
	U8 dck;					/* checksum */
	char dname[8];			/* disk name */
	U16 nf;					/* # of files */
	U16 nfa;				/* next free directory addr (base 2800) */
	U16 nda;				/* next free disk address */
	char fdes[1024 - 15];	/* start of FDE list */
	} POLY_DIR;

typedef struct
	{
	U16 fda;		/* file's disk address */
	U16 dns;		/* # of sectors */
	U16 la;		/* load address */
	U16 sa;		/* start address */
	char name[F_LEN + 1];	/* file name */
	} FDE;

/*
 * register pairs
 */
typedef struct
	{
	U8 l;
	U8 h;
	} HLBYTE;

typedef struct
	{
	U16 hl;
	} HLWORD;

typedef union {
	HLBYTE b;
	HLWORD w;
	} REGPAIR;

#define hlreg (hlregpair.w.hl)
#define hreg  (hlregpair.b.h)
#define lreg  (hlregpair.b.l)

#define bcreg (bcregpair.w.hl)
#define breg  (bcregpair.b.h)
#define creg  (bcregpair.b.l)

#define dereg (deregpair.w.hl)
#define dreg  (deregpair.b.h)
#define ereg  (deregpair.b.l)

#define afreg (afregpair.w.hl)
#define areg  (afregpair.b.h)
#define freg  (afregpair.b.l)

/*
 * externals
 */
extern U8 far *ram;
extern S16 vgaused;
extern U8 grtbl[];

extern REGPAIR hlregpair, bcregpair, deregpair, afregpair;
extern U16 spreg, pcreg;
extern U8 syscall, kbport, iflag, endop;
extern U8 imask;
extern U8 last_port1;

extern U16 ramseg, videoseg, emcount;
extern U16 sstart;

extern S16 gotkbhit, running, adaptive_rtc_flg;
extern S32 opcount, usart_opcount, rtc_opcount, rtc_opcount_max;
#define USART_OPCOUNT 1000		// # ops before issuing an interrupt

extern char diskname[8][80];
extern S16 breaksys_before, breaksys_after;
extern S16 poly88_flag, step_flag, tbasic_flag, ops_flag;
extern FILE *cass_fp;
extern U8 rom[], rom40[];
extern FILE *wh1_fp;

extern S16 match_flag, match_auto;
extern S32 match_delays;
#define MATCH_TARGET 214000L	// target # ops/sec when "match" is auto

extern U8 far *scrnmap[1024];
extern void mkscrnmap( void );


/*
 * prototypes
 */
void main( S16 argc, char **argv );
void emu( S16 once );
void dumpregs( void );
void regmem( char *regname, U16 val );
void dumpmem( U16 addr0, U16 addr1 );
void unasmmem( U16 addr0, U16 addr1 );
void setpcvideo( void );
void border( void );
void check_keyb( void );
void check_rt( void );
void initram( void );
void initdev( S16 dloaded );
void dio( void );
void bootrom( void );
S16 disasm( U16 pc );
void upscreen( void );
void chrcopy( S16 x, S16 y, char *src );
void startemu( S16 howmany );
void setimask( U8 mask );
void breakem( void ), breakem2( void );
void loadfile( U16 addr, char *file );
void doints( void );
void systemcall( void );
void sysmove( void );
void sysioret( void );
void sysvti( void );
void cline( U16 *cp );
void bootsys( void );
void sysnames( void );
void sysecho( S16 newline );
void sysprintnb( void );
void sysprint( void );
void sysclrline( void );
void syssize( void );
void sysconn( void );
void sysdos( void );
void sysparse( void );
void systext( void );
void syswrit( void );
void sysclos( void );
void writerr( char c, S16 *errp );
void sysread( void );
void syspdev( void );
void sysrdprt( void );
void sysreadtape( void );
void close_cassfp( void );
S16 cass_data_avail( void );
void usart_control( void );
void crypt( char *buf );
void getlicense( char *buf );
S16 getchksum( char *buf );
void cgasnow( void );
void egachar( void );
void vgachar( void );
void oldega( void );
void oldvga( void );
S16 get16args( char *inbuf, U8 *mapp );
void loadfont( char *file );
void undofont( void );
S16 setcolors( char *inbuf );
void set_win_attr( S16 x1, S16 y1, S16 x2, S16 y2, S16 attr );
S16 parse_colors( char *inbuf );
void sysmathop( void );
void setfpb( char *flag );
void fill_fpb( void );
void fgupscreen( void );
void disasmrom( void );
void testloop( S16 install );
void rtctest( void );
void memw( U16 addr, U8 value );
