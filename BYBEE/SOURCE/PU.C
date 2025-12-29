/*
 * PU - Poly utility program for MS-DOS
 */
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>
#include <conio.h>
#include <ctype.h>
#include <sys\stat.h>


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
	unsigned char dck;		/* checksum */
	char dname[8];			/* disk name */
	unsigned int nf;		/* # of files */
	unsigned int nfa;		/* next free directory addr (base 2800) */
	unsigned int nda;		/* next free disk address */
	char fdes[1024 - 15];	/* start of FDE list */
	} POLY_DIR;

typedef struct
	{
	unsigned int fda;		/* file's disk address */
	unsigned int dns;		/* # of sectors */
	unsigned int la;		/* load address */
	unsigned int sa;		/* start address */
	char name[F_LEN + 1];	/* file name */
	} FDE;


POLY_DIR pdir;

#define CMDLINELEN 80
#define MAXCMD 10
char inbuf[CMDLINELEN], *cmds[MAXCMD];
int numargs;
int polyfh;
int enabled = 0;
char enprompt[] = "$$ ";
char disaprompt[] = "$ ";
char *prompt = disaprompt;


/*
 * prototypes
 */
void main( int argc, char **argv );
void splitargs( void );
void skpwht( char **p );
void lcmd( void );
void typecmd( char *file );
int polyopen( char *file, FDE *fdep );
void getcmd( char *file );
void helpcmd( void );
void createdisk( char *name );
void sizedisk( char *sizestr );



void main( int ac, char **av )
	{
	char *cmdp;

	--ac;
	++av;
	if (ac != 1)
		{
		printf("usage: pu <diskname>\n");
		exit(1);
		}

	if ((polyfh = open(*av, O_RDWR|O_BINARY)) == -1)
		{
		printf("Poly disk %s not found... create it (y/n) ? ", *av);
		gets(inbuf);
		if (tolower(*inbuf) == 'y')
			createdisk(*av);
		}

	if (read(polyfh, &pdir, sizeof(pdir)) != sizeof(pdir))
		{
		printf("can't read directory!\n", *av);
		exit(1);
		}

	while (1)
		{
		printf(prompt);
		gets(inbuf);
		splitargs();
		cmdp = cmds[0];
		if (stricmp(cmdp, "help") == 0 || stricmp(cmdp, "h") == 0)
			helpcmd();
		else if (stricmp(cmdp, "type") == 0 || stricmp(cmdp, "t") == 0)
			typecmd(cmds[1]);
		else if (stricmp(cmdp, "list") == 0 || stricmp(cmdp, "l") == 0)
			lcmd();
		else if (stricmp(cmdp, "get") == 0)
			getcmd(cmds[1]);
		else if (stricmp(cmdp, "size") == 0)
			sizedisk(cmds[1]);
		else if (stricmp(cmdp, "quit") == 0 || stricmp(cmdp, "q") == 0)
			break;
		else if (stricmp(cmdp, "en") == 0)
			{
			enabled = 1;
			prompt = enprompt;
			}
		else if (stricmp(cmdp, "disa") == 0)
			{
			enabled = 0;
			prompt = disaprompt;
			}
		else if (*cmdp == '\0')
			;		/* null command */
		else
			printf("What?\n");
		}

	close(polyfh);
	exit(0);
	}


/*
 * split up inbuf[] args, put pointers into cmd[] array
 */
void splitargs( void )
	{
	char *p = inbuf;

	numargs = 0;
	skpwht(&p);
	while (*p != '\0')
		{
		cmds[numargs] = p;
		++numargs;

		while (*p > ' ')	/* skip over the cmd */
			++p;

		if (*p != '\0')		/* if doesn't end w/NUL */
			{
			*p = '\0';		/* NUL-terminate it */
			++p;
			}
		skpwht(&p);
		}
	}



/*
 * skip a pointer over whitespace
 */
void skpwht( char **p )
	{
	char c;

	while ((c = **p) != '\0' && c <= ' ')
		++*p;
	}


/*
 * list directory
 */
void lcmd( void )
	{
	char *fde, name[35], stat[4], *p, *q, flag;
	int i, namelen, *pi;
	unsigned int fda, dns, la, sa, disksec;

	disksec = (unsigned int)(filelength(polyfh) >> 8);

	for (i = 0; i < 8; ++i)
		name[i] = pdir.dname[i];
	name[8] = '\0';
	printf("Disk %s has %u files on it.\n", name, pdir.nf);
	printf("%u sectors in use, %u free.\n", pdir.nda, disksec - pdir.nda);
	printf("%u bytes free in this directory.\n", 0x2c00 - pdir.nfa);
	if (enabled)
		printf("sector  load  start   stat");
	printf("   size  name\n");
	for (fde = pdir.fdes; *fde; )
		{
		flag = fde[0];
		namelen = flag & F_LEN;
		for (i = 0, p = &fde[1], q = name; i < namelen; ++i)
			*q++ = *p++;
		*q++ = '.';
		*q++ = *p++;
		*q++ = *p++;
		*q = '\0';
		pi = (int *)p;
		fda = *pi++;
		dns = *pi++;
		la = *pi++;
		sa = *pi++;
		fde = (char *)pi;

		for (i = 0; i < 3; ++i)		/* build stat[] string */
			stat[i] = ' ';
		stat[3] = '\0';
		if (flag & F_DEL)
			stat[0] = 'D';
		if (flag & F_SYS)
			stat[1] = 'S';
		if (flag & F_NEW)
			stat[2] = 'N';

		if ((flag & F_DEL) == 0 || enabled)
			{
			if (enabled)
				printf("%5X   %4X   %4X    %s", fda, la, sa, stat);
			printf("  %5d  %s\n", dns, name);
			}
		}
	}


/*
 * get a file into IBM-PC file
 */
void getcmd( char *file )
	{
	FDE fde;
	char buf[SECSIZE], c;
	int i, j, fh;
	int textmode = 0;

	if (!polyopen(file, &fde))
		{
		printf("Can't open file: %s\n", file);
		return;
		}

	while (1)
		{
		printf("Text or Binary treatment (t or b): ");
		gets(buf);
		c = tolower(*buf);
		if (c == 't')
			{
			textmode = 1;
			break;
			}
		if (c == 'b')
			break;
		}

	printf("Filename on IBM-PC side: ");
	gets(buf);
	if ((fh = open(buf, O_RDWR|O_BINARY|O_CREAT|O_TRUNC,
		S_IREAD|S_IWRITE)) == -1)
		{
		printf("Can't create file: %s\n", buf);
		return;
		}

	lseek(polyfh, 256L * fde.fda, SEEK_SET);
	for (i = 0; i < fde.dns; ++i)
		{
		read(polyfh, buf, SECSIZE);
		for (j = 0; j < SECSIZE; ++j)
			{
			c = buf[j];
			if (c == '\r' && textmode)		/* if \r, in text mode, */
				{
				write(fh, &c, 1);
				c = '\n';					/* chase with \n */
				write(fh, &c, 1);
				}
			else if (c != '\0' || !textmode)
				write(fh, &c, 1);			/* supress NULs in text mode */
			}
		}
	close(fh);
	}


/*
 * type a file
 */
void typecmd( char *file )
	{
	FDE fde;
	char buf[SECSIZE], c;
	int i, j, curpos, lpos;

	if (!polyopen(file, &fde))
		{
		printf("Can't open file: %s\n", file);
		return;
		}

	curpos = 0;
	lpos = 0;
	lseek(polyfh, 256L * fde.fda, SEEK_SET);
	for (i = 0; i < fde.dns; ++i)
		{
		read(polyfh, buf, SECSIZE);
		for (j = 0; j < SECSIZE; ++j)
			{
			if (kbhit())
				c = getch();

			c = buf[j];
			if (c >= ' ')
				{
				putch(c);
				++curpos;
				}
			else if (c == '\r')
				{
				putch('\r');
				putch('\n');
				curpos = 0;
				++lpos;
				if (lpos == 23)
					{
					lpos = 0;
					putch('.');
					c = getch();
					putch(8);
					if (tolower(c) == 'x')
						return;
					}
				}
			else if (c == '\t')
				{
				do	{
					putch(' ');
					++curpos;
					} while ((curpos & 7) != 0);
				}
			}
		}
	}


/*
 * open a poly file, entering vital info into fde
 */
int polyopen( char *file, FDE *fdep )
	{
	char *fdp, *p, *q, flag;
	int i, *pi, namelen;

	for (fdp = pdir.fdes; *fdp; )
		{
		flag = fdp[0];
		namelen = flag & F_LEN;
		for (i = 0, p = &fdp[1], q = fdep->name; i < namelen; ++i)
			*q++ = *p++;
		*q++ = '.';
		*q++ = *p++;
		*q++ = *p++;
		*q = '\0';
		pi = (int *)p;
		fdep->fda = *pi++;
		fdep->dns = *pi++;
		fdep->la = *pi++;
		fdep->sa = *pi++;
		fdp = (char *)pi;

		if ((flag & F_DEL) == 0 && strcmp(file, fdep->name) == 0)
			return (1);
		}

	return (0);
	}


/*
 * help command - explain all other commands
 */
void helpcmd( void )
	{
	printf("Poly Utility (PU) - commands are:\n"
		"list           directory listing\n"
		"en             ENabled mode (long directory)\n"
		"disa           DISAbles mode (short directory)\n"
		"type <file>    display a file\n"
		"get <file>     gets Poly file into PC file\n"
		"size <len>     change disk to <len> sectors\n"
		"quit           exit this program\n");
	}


/*
 * Create a Poly disk.  Prompt for the # of sectors.
 */
void createdisk( char *name )
	{
	unsigned char buf[256], *p;
	unsigned int nsec;
	int i;

	if ((polyfh = open(name, O_RDWR|O_BINARY|O_CREAT|O_TRUNC,
		S_IREAD|S_IWRITE)) == -1)
		{
		printf("Can't create Poly disk %s!\n", name);
		exit(1);
		}

	while (1)
		{
		printf("How many Poly sectors do you want this disk to contain? ");
		gets(inbuf);
		if (sscanf(inbuf, " %u", &nsec) == 1 && nsec > 349)
			break;
		}

	memset(buf, 0, 256);
	while (nsec-- > 0)		/* write out the empty disk */
		{
		if (write(polyfh, buf, 256) != 256)
			{
			printf("Can't write!\n");
			exit(1);
			}
		}

	memset(&pdir, 0, sizeof(pdir));		/* create the blank directory */
	printf("Disk name? ");
	gets(inbuf);
	for (i = 0; i < 8; ++i)
		{
		if (inbuf[i] == '\0')
			break;
		else
			pdir.dname[i] = inbuf[i];
		}
	pdir.nfa = 0x280f;		/* next free directory address */
	pdir.nda = 4;			/* next free disk sector */
	for (p = (unsigned char *)&pdir + 1, i = 1; i < sizeof(pdir); ++p, ++i)
		pdir.dck += *p;
	lseek(polyfh, 0L, SEEK_SET);
	write(polyfh, &pdir, sizeof(pdir));
	close(polyfh);

	if ((polyfh = open(name, O_RDWR|O_BINARY)) == -1)
		{
		printf("Strange... created but can't reopen file!\n");
		exit(1);
		}
	}


/*
 * Change disk size to # sectors.
 */
void sizedisk( char *sizestr )
	{
	unsigned int nsec;
	long dbytes;

	if (sscanf(sizestr, " %u", &nsec) != 1)
		{
		printf("usage:   size nnn\n");
		return;
		}

	if (nsec < pdir.nda)
		{
		printf("Currently %u sectors in use, can't go below that.\n",
			pdir.nda);
		return;
		}

	dbytes = ((long)nsec) << 8;
	if (chsize(polyfh, dbytes) == -1)
		perror("Can't change disk size");
	}
