/*
 * 8080 disassembler
 */
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include "pmhdr.h"


struct
	{
	S16 args;
	char *mnem;
	} opcodes[256] =
	{
    /* 0x00 */ { 0, "NOP"      },
    /* 0x01 */ { 2, "LXI  B,"  },
    /* 0x02 */ { 0, "STAX B"   },
	/* 0x03 */ { 0, "INX  B"   },
    /* 0x04 */ { 0, "INR  B"   },
	/* 0x05 */ { 0, "DCR  B"   },
    /* 0x06 */ { 1, "MVI  B,"  },
    /* 0x07 */ { 0, "RLC"      },
	/* 0x08 */ { 0, "???"      },
    /* 0x09 */ { 0, "DAD  B"   },
    /* 0x0A */ { 0, "LDAX B"   },
	/* 0x0B */ { 0, "DCX  B"   },
    /* 0x0C */ { 0, "INR  C"   },
	/* 0x0D */ { 0, "DCR  C"   },
	/* 0x0E */ { 1, "MVI  C,"  },
	/* 0x0F */ { 0, "RRC"      },
    /* 0x10 */ { 0, "???"      },
	/* 0x11 */ { 2, "LXI  D,"  },
    /* 0x12 */ { 0, "STAX D"   },
    /* 0x13 */ { 0, "INX  D"   },
    /* 0x14 */ { 0, "INR  D"   },
	/* 0x15 */ { 0, "DCR  D"   },
    /* 0x16 */ { 1, "MVI  D,"  },
    /* 0x17 */ { 0, "RAL"      },
	/* 0x18 */ { 0, "???"      },
    /* 0x19 */ { 0, "DAD  D"   },
    /* 0x1A */ { 0, "LDAX D"   },
	/* 0x1B */ { 0, "DCX  D"   },
	/* 0x1C */ { 0, "INR  E"   },
	/* 0x1D */ { 0, "DCR  E"   },
    /* 0x1E */ { 1, "MVI  E,"  },
    /* 0x1F */ { 0, "RRA"      },
	/* 0x20 */ { 0, "???"      },
    /* 0x21 */ { 2, "LXI  H,"  },
	/* 0x22 */ { 2, "SHLD "    },
    /* 0x23 */ { 0, "INX  H"   },
    /* 0x24 */ { 0, "INR  H"   },
	/* 0x25 */ { 0, "DCR  H"   },
    /* 0x26 */ { 1, "MVI  H,"  },
	/* 0x27 */ { 0, "DAA"      },
    /* 0x28 */ { 0, "???"      },
	/* 0x29 */ { 0, "DAD  H"   },
    /* 0x2A */ { 2, "LHLD "    },
	/* 0x2B */ { 0, "DCX  H"   },
    /* 0x2C */ { 0, "INR  L"   },
	/* 0x2D */ { 0, "DCR  L"   },
    /* 0x2E */ { 1, "MVI  L,"  },
    /* 0x2F */ { 0, "CMA"      },
	/* 0x30 */ { 0, "???"      },
    /* 0x31 */ { 2, "LXI  SP," },
	/* 0x32 */ { 2, "STA  "    },
    /* 0x33 */ { 0, "INX  SP"  },
    /* 0x34 */ { 0, "INR  M"   },
	/* 0x35 */ { 0, "DCR  M"   },
    /* 0x36 */ { 1, "MVI  M,"  },
    /* 0x37 */ { 0, "STC"      },
    /* 0x38 */ { 0, "???"      },
	/* 0x39 */ { 0, "DAD  SP"  },
    /* 0x3A */ { 2, "LDA  "    },
	/* 0x3B */ { 0, "DCX  SP"  },
    /* 0x3C */ { 0, "INR  A"   },
	/* 0x3D */ { 0, "DCR  A"   },
	/* 0x3E */ { 1, "MVI  A,"  },
    /* 0x3F */ { 0, "CMC"      },
    /* 0x40 */ { 0, "MOV  B,B" },
	/* 0x41 */ { 0, "MOV  B,C" },
    /* 0x42 */ { 0, "MOV  B,D" },
    /* 0x43 */ { 0, "MOV  B,E" },
	/* 0x44 */ { 0, "MOV  B,H" },
    /* 0x45 */ { 0, "MOV  B,L" },
    /* 0x46 */ { 0, "MOV  B,M" },
    /* 0x47 */ { 0, "MOV  B,A" },
	/* 0x48 */ { 0, "MOV  C,B" },
    /* 0x49 */ { 0, "MOV  C,C" },
    /* 0x4A */ { 0, "MOV  C,D" },
	/* 0x4B */ { 0, "MOV  C,E" },
    /* 0x4C */ { 0, "MOV  C,H" },
	/* 0x4D */ { 0, "MOV  C,L" },
    /* 0x4E */ { 0, "MOV  C,M" },
    /* 0x4F */ { 0, "MOV  C,A" },
	/* 0x50 */ { 0, "MOV  D,B" },
	/* 0x51 */ { 0, "MOV  D,C" },
    /* 0x52 */ { 0, "MOV  D,D" },
    /* 0x53 */ { 0, "MOV  D,E" },
    /* 0x54 */ { 0, "MOV  D,H" },
    /* 0x55 */ { 0, "MOV  D,L" },
	/* 0x56 */ { 0, "MOV  D,M" },
    /* 0x57 */ { 0, "MOV  D,A" },
	/* 0x58 */ { 0, "MOV  E,B" },
	/* 0x59 */ { 0, "MOV  E,C" },
    /* 0x5A */ { 0, "MOV  E,D" },
	/* 0x5B */ { 0, "MOV  E,E" },
    /* 0x5C */ { 0, "MOV  E,H" },
	/* 0x5D */ { 0, "MOV  E,L" },
    /* 0x5E */ { 0, "MOV  E,M" },
    /* 0x5F */ { 0, "MOV  E,A" },
    /* 0x60 */ { 0, "MOV  H,B" },
    /* 0x61 */ { 0, "MOV  H,C" },
    /* 0x62 */ { 0, "MOV  H,D" },
	/* 0x63 */ { 0, "MOV  H,E" },
	/* 0x64 */ { 0, "MOV  H,H" },
	/* 0x65 */ { 0, "MOV  H,L" },
    /* 0x66 */ { 0, "MOV  H,M" },
    /* 0x67 */ { 0, "MOV  H,A" },
	/* 0x68 */ { 0, "MOV  L,B" },
    /* 0x69 */ { 0, "MOV  L,C" },
    /* 0x6A */ { 0, "MOV  L,D" },
    /* 0x6B */ { 0, "MOV  L,E" },
	/* 0x6C */ { 0, "MOV  L,H" },
    /* 0x6D */ { 0, "MOV  L,L" },
	/* 0x6E */ { 0, "MOV  L,M" },
	/* 0x6F */ { 0, "MOV  L,A" },
    /* 0x70 */ { 0, "MOV  M,B" },
    /* 0x71 */ { 0, "MOV  M,C" },
    /* 0x72 */ { 0, "MOV  M,D" },
    /* 0x73 */ { 0, "MOV  M,E" },
    /* 0x74 */ { 0, "MOV  M,H" },
    /* 0x75 */ { 0, "MOV  M,L" },
	/* 0x76 */ { 0, "HLT"      },
	/* 0x77 */ { 0, "MOV  M,A" },
    /* 0x78 */ { 0, "MOV  A,B" },
    /* 0x79 */ { 0, "MOV  A,C" },
	/* 0x7A */ { 0, "MOV  A,D" },
    /* 0x7B */ { 0, "MOV  A,E" },
    /* 0x7C */ { 0, "MOV  A,H" },
    /* 0x7D */ { 0, "MOV  A,L" },
    /* 0x7E */ { 0, "MOV  A,M" },
    /* 0x7F */ { 0, "MOV  A,A" },
	/* 0x80 */ { 0, "ADD  B"   },
	/* 0x81 */ { 0, "ADD  C"   },
    /* 0x82 */ { 0, "ADD  D"   },
    /* 0x83 */ { 0, "ADD  E"   },
    /* 0x84 */ { 0, "ADD  H"   },
    /* 0x85 */ { 0, "ADD  L"   },
    /* 0x86 */ { 0, "ADD  M"   },
    /* 0x87 */ { 0, "ADD  A"   },
    /* 0x88 */ { 0, "ADC  B"   },
	/* 0x89 */ { 0, "ADC  C"   },
	/* 0x8A */ { 0, "ADC  D"   },
    /* 0x8B */ { 0, "ADC  E"   },
	/* 0x8C */ { 0, "ADC  H"   },
    /* 0x8D */ { 0, "ADC  L"   },
    /* 0x8E */ { 0, "ADC  M"   },
    /* 0x8F */ { 0, "ADC  A"   },
    /* 0x90 */ { 0, "SUB  B"   },
    /* 0x91 */ { 0, "SUB  C"   },
    /* 0x92 */ { 0, "SUB  D"   },
	/* 0x93 */ { 0, "SUB  E"   },
	/* 0x94 */ { 0, "SUB  H"   },
    /* 0x95 */ { 0, "SUB  L"   },
    /* 0x96 */ { 0, "SUB  M"   },
    /* 0x97 */ { 0, "SUB  A"   },
    /* 0x98 */ { 0, "SBB  B"   },
    /* 0x99 */ { 0, "SBB  C"   },
    /* 0x9A */ { 0, "SBB  D"   },
    /* 0x9B */ { 0, "SBB  E"   },
	/* 0x9C */ { 0, "SBB  H"   },
	/* 0x9D */ { 0, "SBB  L"   },
	/* 0x9E */ { 0, "SBB  M"   },
    /* 0x9F */ { 0, "SBB  A"   },
	/* 0xA0 */ { 0, "ANA  B"   },
    /* 0xA1 */ { 0, "ANA  C"   },
    /* 0xA2 */ { 0, "ANA  D"   },
    /* 0xA3 */ { 0, "ANA  E"   },
    /* 0xA4 */ { 0, "ANA  H"   },
	/* 0xA5 */ { 0, "ANA  L"   },
    /* 0xA6 */ { 0, "ANA  M"   },
	/* 0xA7 */ { 0, "ANA  A"   },
	/* 0xA8 */ { 0, "XRA  B"   },
    /* 0xA9 */ { 0, "XRA  C"   },
    /* 0xAA */ { 0, "XRA  D"   },
    /* 0xAB */ { 0, "XRA  E"   },
    /* 0xAC */ { 0, "XRA  H"   },
    /* 0xAD */ { 0, "XRA  L"   },
    /* 0xAE */ { 0, "XRA  M"   },
	/* 0xAF */ { 0, "XRA  A"   },
	/* 0xB0 */ { 0, "ORA  B"   },
    /* 0xB1 */ { 0, "ORA  C"   },
    /* 0xB2 */ { 0, "ORA  D"   },
	/* 0xB3 */ { 0, "ORA  E"   },
    /* 0xB4 */ { 0, "ORA  H"   },
    /* 0xB5 */ { 0, "ORA  L"   },
    /* 0xB6 */ { 0, "ORA  M"   },
	/* 0xB7 */ { 0, "ORA  A"   },
    /* 0xB8 */ { 0, "CMP  B"   },
    /* 0xB9 */ { 0, "CMP  C"   },
	/* 0xBA */ { 0, "CMP  D"   },
    /* 0xBB */ { 0, "CMP  E"   },
	/* 0xBC */ { 0, "CMP  H"   },
    /* 0xBD */ { 0, "CMP  L"   },
    /* 0xBE */ { 0, "CMP  M"   },
    /* 0xBF */ { 0, "CMP  A"   },
    /* 0xC0 */ { 0, "RNZ"      },
    /* 0xC1 */ { 0, "POP  B"   },
	/* 0xC2 */ { 2, "JNZ  "    },
	/* 0xC3 */ { 2, "JMP  "    },
    /* 0xC4 */ { 2, "CNZ  "    },
    /* 0xC5 */ { 0, "PUSH B"   },
	/* 0xC6 */ { 1, "ADI  "    },
    /* 0xC7 */ { 0, "RST  0"   },
    /* 0xC8 */ { 0, "RZ"       },
	/* 0xC9 */ { 0, "RET"      },
	/* 0xCA */ { 2, "JZ   "    },
    /* 0xCB */ { 0, "???"      },
	/* 0xCC */ { 2, "CZ   "    },
	/* 0xCD */ { 2, "CALL "    },
    /* 0xCE */ { 1, "ACI  "    },
	/* 0xCF */ { 0, "RST  1"   },
	/* 0xD0 */ { 0, "RNC"      },
    /* 0xD1 */ { 0, "POP  D"   },
    /* 0xD2 */ { 2, "JNC  "    },
    /* 0xD3 */ { 1, "OUT  "    },
    /* 0xD4 */ { 2, "CNC  "    },
	/* 0xD5 */ { 0, "PUSH D"   },
	/* 0xD6 */ { 1, "SUI  "    },
    /* 0xD7 */ { 0, "RST  2"   },
    /* 0xD8 */ { 0, "RC"       },
	/* 0xD9 */ { 0, "???"      },
	/* 0xDA */ { 2, "JC   "    },
	/* 0xDB */ { 1, "IN   "    },
	/* 0xDC */ { 2, "CC   "    },
	/* 0xDD */ { 0, "???"      },
	/* 0xDE */ { 1, "SBI  "    },
	/* 0xDF */ { 0, "RST  3"   },
	/* 0xE0 */ { 0, "RPO"      },
	/* 0xE1 */ { 0, "POP  H"   },
	/* 0xE2 */ { 2, "JPO  "    },
	/* 0xE3 */ { 0, "XTHL"     },
	/* 0xE4 */ { 2, "CPO  "    },
	/* 0xE5 */ { 0, "PUSH H"   },
	/* 0xE6 */ { 1, "ANI  "    },
	/* 0xE7 */ { 0, "RST  4"   },
	/* 0xE8 */ { 0, "RPE"      },
	/* 0xE9 */ { 0, "PCHL"     },
	/* 0xEA */ { 2, "JPE  "    },
	/* 0xEB */ { 0, "XCHG"     },
	/* 0xEC */ { 2, "CPE  "    },
	/* 0xED */ { 2, "ESC  "    },
	/* 0xEE */ { 1, "XRI  "    },
	/* 0xEF */ { 0, "RST  5"   },
	/* 0xF0 */ { 0, "RP"       },
	/* 0xF1 */ { 0, "POP  PSW" },
	/* 0xF2 */ { 2, "JP	  "    },
	/* 0xF3 */ { 0, "DI"       },
	/* 0xF4 */ { 1, "CP   "    },
	/* 0xF5 */ { 0, "PUSH PSW" },
	/* 0xF6 */ { 1, "ORI  "    },
	/* 0xF7 */ { 0, "RST  6"   },
	/* 0xF8 */ { 0, "RM"       },
	/* 0xF9 */ { 0, "SPHL"     },
	/* 0xFA */ { 2, "JM   "    },
	/* 0xFB */ { 0, "EI"       },
	/* 0xFC */ { 2, "CM   "    },
	/* 0xFD */ { 0, "???"      },
	/* 0xFE */ { 1, "CPI  "    },
	/* 0xFF */ { 0, "RST  7"   }
	};


/*
 * Print disassembly for one opcode.
 * Return the instruction length (1-3).
 */
S16 disasm( U16 pc )
	{
	U8 inst;
	S16 args;

	inst = ram[pc];
	cprintf(opcodes[inst].mnem);

	args = opcodes[inst].args;
	switch (args)
		{
		case 2:
			cprintf("%02X", ram[pc + 2]);

		case 1:
			cprintf("%02X", ram[pc + 1]);

		case 0:
			break;
		}

	cprintf("\r\n");
	return (args + 1);
	}


struct
	{
	char *name;
	U16 addr;
	} equates[] =
	{
	{"IORET", 0x0064},
	{"MOVE",  0x0100},
	{"DONT",  0x2d90},
	{"SCEND", 0x0C1E},
	{"SCRHM", 0x0C1F},
	{"STACK", 0x1000},
	{"TIMER", 0x0C00},
	{"SRA1",  0x0C10},
	{"SRA2",  0x0C12},
	{"SRA3",  0x0C14},
	{"SRA4",  0x0C16},
	{"SRA5",  0x0C18},
	{"SRA6",  0x0C1A},
	{"SRA7",  0x0C1C},
	{"USER",  0x3200},
	{"VERLOC", 0x0439},
	{"WAKEUP", 0x0c40},
	{"POS",   0x0c0e},
	{"DEOUT", 0x03d1},
	{"Dio",   0x0406},
	{"Dhalt", 0x0409},
	{"Err",   0x040f},
	{"Flip",  0x042d},
	{"Flush", 0x041e},
	{"Fold",  0x042a},
	{"Gover", 0x0415},
	{"Iexec", 0x0436},
	{"Killi", 0x041b},
	{"Look",  0x0421},
	{"Msg",   0x040c},
	{"Ovrto", 0x0412},
	{"Rlgc",  0x0430},
	{"Rlwe",  0x0427},
	{"Rtn",   0x0418},
	{"Runr",  0x0424},
	{"Warm",  0x0403},
	{"WH0",   0x0c20},
	{"WH1",   0x0c24},
	{"WH2",   0x0c28},
	{"WH3",   0x0c2c},
	{"WH4",   0x0c30},
	{"WH5",   0x0c34},
	{"WH6",   0x0c38},
	{"WH7",   0x0c3c},
	{"WH8",   0x0c40},
	{"WH9",   0x0c44},
	{NULL,  0}
	};

char *find_equate( U16 addr )
	{
	S16 i;

	for (i = 0; equates[i].name != NULL; ++i)
		{
		if (equates[i].addr == addr)
		break;
		}

	return(equates[i].name);
	}


/*
 * Disassemble entire ROM space 0000 - 0BFF.
 */
void disasmrom( void )
	{
	U16 pc, tw, args, key, i, hasascii;
	U8 inst;
	char *np, *mn, outbuf[100], *p, c;

	pc = 0;
	while (pc < 0x0c00)
		{
		p = outbuf;

		if ((np = find_equate(pc)) == NULL)
			np = " ";
		p += sprintf(p, "%-7.7s", np);

		p += sprintf(p, "%04x  ", pc);

		inst = rom[pc];
		args = opcodes[inst].args;

		for (i = 0; i < 3; ++i)
			{
			if (i < args + 1)
				p += sprintf(p, "%02x ", rom[pc + i]);
			else
				p += sprintf(p, "   ");
			}

		mn = opcodes[inst].mnem;
		p += sprintf(p, "%-5s", mn);


		if (args == 2)
			{
			p += sprintf(p, "%02x%02x", rom[pc + 2], rom[pc + 1]);
			tw = (rom[pc + 2] << 8) | rom[pc + 1];
			if ((np = find_equate(tw)) != NULL)
				p += sprintf(p, "  %s", np);
			}
		else if (args == 1)
			{
			p += sprintf(p, "%02x", rom[pc + 1]);
			}


		hasascii = 0;
		for (i = 0; i < args + 1; ++i)
			{
			c = rom[pc + i];
			if (' ' <= c && c <= '~')
				hasascii = 1;
			}

		if (hasascii)
			{
			while (strlen(outbuf) < 45)
				p += sprintf(p, " ");
			p += sprintf(p, ";");
			for (i = 0; i < args + 1; ++i)
				{
				c = rom[pc + i];
				if (' ' <= c && c <= '~')
					;
				else
					c = '.';
				p += sprintf(p, "%c", c);
				}
			}

		pc += args + 1;
		printf("%s\n", outbuf);

		if (strncmp(mn, "RET", 3) == 0 ||
			strncmp(mn, "JMP", 3) == 0 ||
			strncmp(mn, "PCHL", 4) == 0)
			{
			printf("\n");
			}

		if (step_flag)
			{
			key = getch();
			if (key == 0x1b)
				break;
			}
		}
	}
