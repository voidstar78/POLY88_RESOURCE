/*
 * Decode & display Dwight's Poly-88 tape image (.IMG) files.
 * Bob Bybee, 2/5/2005
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

unsigned int addr = 0x300;
unsigned char tapebuf[500];
int cas_flag;
char imgfile[80], casfile[80];

// Number of sync characters, to give enough time for interblock recovery
// and also to line up the blocks on 16-byte boundaries.
#define NUM_SYNC 31

typedef struct
	{
	char name[8];
	unsigned int recno;
	unsigned char reclen;
	unsigned int addr;
	unsigned char type;
	} HDR;
HDR hdr;

void main( int argc, char **argv )
	{
	FILE *fp, *outfp = NULL;
	unsigned int un, truelen, lineno = 0;
	int sum, i;
	char *stype, *p;
	unsigned char uc, *up;

	++argv, --argc;

	if (strcmp(*argv, "-cas") == 0)
		{
		cas_flag = 1;
		++argv, --argc;
		}

	if (argc < 1)
		{
	usage:
		printf("usage: imgdump [-cas] <file> [addr]\n"
			"-cas   creates *.cas tape image file\n"
			"       default for addr is 300H\n");
		exit(1);
		}

	strcpy(imgfile, *argv);
	if (strchr(imgfile, '.') == NULL)
		strcat(imgfile, ".img");

	if ((fp = fopen(imgfile, "rb")) == NULL)
		{
		printf("Can't open file: %s\n", imgfile);
		exit(1);
		}

	if (argc == 2)
		{
		if (sscanf(argv[1], "%x", &un) == 1)
			addr = un;
		else
			goto usage;
		}

	printf("Decoding image file %s at address 0x%x\n", imgfile, addr);

	if (cas_flag)
		{
		strcpy(casfile, imgfile);
		p = strchr(casfile, '.');
		strcpy(p, ".cas");

		if ((outfp = fopen(casfile, "wb")) == NULL)
			{
			printf("Can't create cassette file!\n");
			exit(1);
			}

		printf("Creating cassette file: %s\n", casfile);
		}

	fseek(fp, addr, SEEK_SET);	// skip to start of data in IMG file

	printf("\nName      Rec#  Len  Addr  Type\n");

	while (fread(&hdr, 1, sizeof(hdr), fp) == sizeof(hdr))
		{
		if ((truelen = hdr.reclen) == 0)
			truelen = 256;

		switch (hdr.type)
			{
			case 0:  stype = "abs-binary";		break;
			case 1:  stype = "comment"; 		break;
			case 2:  stype = "end"; 			break;
			case 3:  stype = "auto-execute"; 	break;
			case 4:  stype = "data";			break;
			case 5:  stype = "BASIC tokens?";	break;
			default: stype = "???";				break;
			}

		printf("%-8.8s  %04x  %3x  %04x  %02x  %s\n",
			hdr.name, hdr.recno, truelen, hdr.addr, hdr.type, stype);

		if (++lineno == 20)
			{
			lineno = 0;
			cprintf("Press any key... ");
			if (getch() == 0x1b)
				exit(0);
			cprintf("\r");
			clreol();
			}

		if (cas_flag)
			{
			for (i = 0; i < NUM_SYNC; ++i)
				fputc(0xe6, outfp);		// put sync chars
			fputc(0x01, outfp);			// put SOH char

			sum = 0;
			up = (unsigned char *)&hdr;
			for (i = 0; i < sizeof(hdr); ++i)
				{
				uc = *up++;
				fputc(uc, outfp);		// put header
				sum += uc;
				}

			fputc(-sum, outfp);			// put checksum
			}


		if (hdr.type == 2 || hdr.type == 3)
			break;	// end of tape

		memset(tapebuf, 0, sizeof(tapebuf));
		if (fread(tapebuf, 1, truelen, fp) != truelen)
			printf("Warning - can't read record data!\n");

		if (cas_flag)
			{
			// put tape data & sum also
			sum = 0;
			up = (unsigned char *)tapebuf;
			for (i = 0; i < truelen; ++i)
				{
				uc = *up++;
				fputc(uc, outfp);		// put data buffer
				sum += uc;
				}

			fputc(-sum, outfp);			// put checksum
			}
		}

	fclose(fp);

	if (outfp != NULL)
		fclose(outfp);

	exit(0);
	}