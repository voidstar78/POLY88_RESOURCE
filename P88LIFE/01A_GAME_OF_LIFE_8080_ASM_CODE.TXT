; LIFE WITH GRAPHICS CURSOR FOR 4.0 MONITOR ROM
	ORG     2000H
TADD1   EQU     2E00H
WH0     EQU     0C20H
TAD1    EQU     2EH
VADD    EQU     0F800H       ; VIDEO BLOCK ADDRESS
KBUF    EQU     0C0CH        ; KEYBOARD INPUT BUFFER ADDRESS
MADD    EQU     2400H        ; MASTER COPY ADDRESS
SADD    EQU     2900H        ; SLAVE COPY ADDRESS
MAD     EQU     24H          ; 1ST BYTE OF MADD
SAD     EQU     29H          ; 1ST BYTE OF SADD
LINE    EQU     64           ; LINE LENGTH
TADD    EQU     2308H        ; TABLE (MASK & SCRATCH)
TAD     EQU     23H          ; 1ST BYTE OF TADD
CADD    EQU     2280H        ; COUNT ADDRESS (GENERATIONS)
	JMP     SETUP                ; START OF GRAPHICS CURSOR
	LXI     H,TADD               ; SET UP MASK TABLE
	MVI     A,20H                ; FIRST MASK FOR TABLE
MASK:    MVI C,08H            ; GETS EIGHT SPOTS
TABLE:   MOV M,A
	INX     H
	DCR     C
	JNZ     TABLE             ;IN TABLE.
	RRC                       ;THEN MASK FOR NEXT LOWER BIT
	JNC     MASK              ;GETS THE NEXT EIGHT.
	LXI     H,SADD            ;SAVE SLAVE ADDRESS
	PUSH    H                 ;FOR USE IN LOOP
	LXI     H,CADD            ;LOAD CADD WITH OWN
	MVI     M,80H             ;SECOND BYTE TO START COUNT.
	LXI     H,VADD-40H        ;SET UP FOR SWAP FROM
	LXI     D,SADD-40H        ;SCREEN TO SLAVE WITH SLOP.
LOOP:    JMP KBDTST
CONTIN:  MOV A,M
	CMA                       ;COMPLEMENT FOR TRUE LIFE
	STAX    D                 ;STORE ON OTHER COPY.
	INX     H                 ;NEXT
	INX     D                 ;SPOT.
	MOV     A,H               ;CHECK
	ANI     7                 ;LAST THREE BITS OF 1ST BYTE
	CPI     5                 ;FOR END
	JNZ     LOOP              ;OF COPY PLUS SLOP.
	LXI     H,SADD-40H        ;SWAP SLAVE
	LXI     D,MADD-40H        ;TO MASTER
SWAP:    MOV A,M
	STAX    D
	INX     H
	INX     D
	MOV     A,H
	CPI     SAD+5             ;WITH SLOP
	JNZ     SWAP              ;UP TO HERE.
	LXI     D,CADD            ;SET UP FOR COUNT
	LXI     B,VADD+28H        ;IN UPPER RIGHT OF SCREEN
	LXI     H,CADD            ;WATCH THE ZERO AND CARRY!!
	MOV     L,E
COUNT:   INX H             ;NEXT SIGNIFICANT DIGIT
	DCX     B                 ;NEXT DOWN ON SCREEN
	JNZ     NOINC             ;ZERO FLAG TO INCREMENT
	INR     M
NOINC:   LDAX D            ;ARE WE TO END
	CMP     L                 ;OF COUNT (STORED AT CADD)?
	JC      REP               ;YES
	MVI     A,0BAH            ;NO, CHECK FOR
	CMP     M                 ;DECIMAL CARRY IN ASCII.
	JNZ     HERE              ;NO
REP:     MVI A,0B0H        ;YES, ZERO THAT DIGIT
	MOV     M,A               ;AND REPLACE MEMORY.
HERE:    MOV A,M           ;GET MEMORY
	STAX    B                 ;AND VIEW IT
	JNC     COUNT             ;UNTIL ALL DIGITS ARE VIEWED.
	DCX     H                 ;CHECK MOST SIGNIFICANT DIGIT
	CMP     M                 ;AGAINST NEXT MOST.
	JZ      THERE             ;BOTH ZERO? EXIT.
	XCHG                      ;NO, INCREASE
	INR     M                 ;END OF COUNT.
THERE:   LXI H,MADD-LINE-1
	MVI     D,TAD             ;GET IN POSITION FOR TABLE.
BYTE:    LXI B,INST        ;PSEUDO OP LIST
BIT:     LDAX B            ;LOAD PSEUDO OP.
	RRC                       ;CHECK RIGHT BIT FOR
	JNC     ROT               ;CELL CHECK FROM SAME BYTE
	RRC                       ;NO. NEXT BYTE?
	JNC     ONE               ;YES
	CPI     0F0H              ;NO. ALL NFHBRS DONE THIS BYTE?
	JNC     DONE              ;YES.
	LXI     D,LINE-3          ;NEXT LINE ON 3X3 MATRIX
	DAD     D                 ;INCREMENT BY LINE-2
	MVI     D,TAD             ;BY LINE-3+1, SINCE WE NEED
ONE:     INX H             ;A +1 ANYWAY
	ANI     3FH               ;GET RID OF 2 MSB'S.
	RLC                       ;ZERO CARRY BIT AND
ROT:     RAR               ;GET IN POSITION
	INX     B                 ;FOR THIS AND NEXT PSEUDO OP
	MOV     E,A               ;2ND BYTE FEEDS MASK TABLE
	LDAX    D                 ;LOAD MASK FOR BIT
	ANA     M                 ;AND CHECK IT ON THE MASTER
	JZ      BIT               ;NO LIFE, NEXT BIT
	XCHG                      ;BRING DOWN SCRATCH
	MVI     A,07H             ;ADDRESS TO STORE NEIGHBOR
	ANA     L                 ;COUNT CODED BY BIT #
	MOV     L,A           
	INR     M                 ;COUNT ONE NEIGHBOR
	XCHG                      ;GET MASTER COPY
	JMP     BIT               ;AND GET NEXT BIT IN BYTE
DONE:    LXI B,0-LINE-1    ;GO BACK TO BYTE
	DAD     B                 ;THAT WE'RE WORKING ON
	MVI     E,0               ;ZERO SCRATCHPAD BYTE #2
	XTHL                      ;MOVING ON TO SLAVE COPY.
LOAD:    SUB A             ;ZERO A SO WE CAN
	STAX    D                 ;ZERO NEIGHBOR COUNT
	MOV     A,C               ;GET INVERTED BIT MASK
	RRC                       ;COMING IN BFH AND ROTATE
	JNC     NEXT              ;GOT ALL BITS?
	MOV     C,A               ;NO, REPLACE MASK
	INR     E                 ;AND COUNT BIT NUMBER
	LDAX    D                 ;GET # NEIGHBORS OF THAT BIT
	CPI     02H               ;IS IT TWO?
	JZ      LOAD              ;YES, CELL STAYS THE WAY IT IS
	MOV     A,C               ;NO, SO
	ANA     M                 ;KILL CELL ON
	MOV     M,A               ;SLAVE COPY
	LDAX    D                 ;HOW MANY NHBRS AGAIN?
	CPI     03H               ;ARE THERE THREE?
	JNZ     LOAD              ;YES, GOOD WE KILLED IT.
	MOV     A,C               ;OOPS, GOT TO RESURRECT IT
	CMA                       ;BY INVERTING THE MASK
	ADD     M                 ;AND ADDING
	MOV     M,A               ;REPLACE SLAVE
	JMP     LOAD              ;UPDATE NEXT BIT IN BYTE
NEXT:    LXI B,0-LINE      ;UP ONE, WHICH IS UPPER
	INX     H                 ;INCREMENT SLAVE ADDRESS
	XTHL                      ;FOR PROPER INITIALIZATION
	MVI     A,MAD+04H         ;END OF SCREEN?
	CMP     H           
	DAD     B                 ;COMPLETE ONE UP
	JNZ     BYTE              ;SCREEN NOT OVER, NEXT BYTE
	POP     H                 ;LEAVE
	LXI     H,SADD            ;SADD ON STACK
	PUSH    H                 ;FOR NEXT TIME, SET UP TO
	LXI     D,VADD            ;SWAP SLAVE TO SCREEN
	JMP     LOOP              ;ON EACH SUCCESSIVE LOOP.
                              ;KBDTST CHECKS THE KEYBOARD FOR "E"
                              ;(EXIT COMMAND) AND IF SO JUMPS INTO
                              ;WARMSTART OF GRAPHICS CURSOR
KBDTST:  PUSH    PSW
	LDA     KBUF+1
	CPI     'E'
	JNZ     OVER2
	POP     PSW
	JMP     MODIFY
OVER2:POP     PSW
	JMP     CONTIN
INST:DW 65C4H        ;PSEUDO OPS CODE 48
	DW 70C4H            ;SPECIAL CASES:EIGHT
	DW 71D0H            ;NEIGHBORS FOR EACH OF
	DW 0A487H           ;SIX CELLS PER BYTE
	DW 0A888H           ;RIGHT TWO BITS OF
	DW 0ACC8H           ;EACH PSEUDO OP INDICATE
	DW 45CCH            ;WHETHER NEXT NEIGHBOR IS
	DW 0A484H           ;IN THE SAME BYTE AS
	DW 6828H            ;CURRENT NEIGHBOR, OR IN
	DW 0A888H           ;NEXT BYTE, OR NEXT LINE
	DW 4CC8H            ;IN 3X3 MATRIX OF
	DW 0CCACH           ;NEIGHBOR BYTES
	DW 5030H            ;NEXT THREE BITS CODE
	DW 34B0H            ;CELL WHOSE NEIGHBORS
	DW 7454H            ;WE ARE COUNTING, IN
	DW 0D494H           ;REVERSE ORDER
	DW 7858H            ;REMAINING THREE BITS
	DW 31B8H            ;CODE MASK FOR NEIGHBOR
	DW 3450H            ;IN SAME FORMAT
	DW 7454H
	DW 7858H
	DW 2D8FH
	DW 388CH
	DW 3998H
	DB 0FFH             ;WRITTEN BY BRIAN WILCOX
                            ;(TRANSCRIBED BY XIPHOD, APRIL 2026)
SETUP: LXI H,VADD
	MVI A,0CH
	ADD H
WIPE: MVI M,7FH
	INX H
	CMP H
	JNZ WIPE
MODIFY: MVI  C,6
	LXI D,TADD1
	MVI A,1
TABL1: STAX  D
	RLC
	INX D
	DCR C
	JNZ TABL1
	LXI H,VADD+0228H
	MVI B,0
	MVI M,7EH
LOOP1:  CALL WH0     ;KEYBOARD INPUT WORMHOLE
	ORI     80H  ;GETS COMMAND CHARACTER
	CALL    OUT1
	JMP     LOOP1  ;NEXT COMMAND
OUT1:   MOV     C,A
	MOV A,M
	ANI 40H
	JZ COMP
	CALL MASK1
	XRA M
	MOV M,A
                         ;COMMAND INTERPRETER
COMP: MOV A,C
	CPI 0D8H
	JZ CLNO
	CPI 0D5H
	CZ UP
	CPI 0C4H
	CZ DOWN
	CPI 0D2H
	CZ RIGHT
	CPI 0CCH
	CZ LEFT
	CPI 0D7H
	CZ WHITE
	CPI 0C2H
	CZ BLACK
	CPI 0CDH
	CZ MOVE
	CPI 0B1H
	CZ ONE1
	CPI 0B2H
	CZ TWO
	CPI 0B3H
	CZ THREE
	CPI 0B4H
	CZ FOUR
	CALL MASK1
	ANA M
	JZ LITE
	CMA
	ANA M
	ORI 40H
	MOV M,A
	JMP BACK
LITE:  MVI A,0BFH
	ANA M
	MOV M,A
BACK:  MOV A,B
	ANI 60H
	RZ
	ANI 40H
	JNZ BRITE
	MVI A,40H
	ORA M
	MOV M,A
	RET
BRITE: MVI A,0BFH
	ANA M
	MOV M,A
	RET
UP: MOV A,B
	ANI 2
	JNZ HERE1
	INR B
	RET
HERE1: DCR B
	DCR B
	LXI D,0-LINE
	DAD D
	RET
DOWN: MOV A,B
	ANI 3
	JZ THER1
	DCR B
	RET
THER1: INR B
	INR B
	LXI D,LINE
	DAD D
	RET
RIGHT: MOV A,B
	RLC
	CMC
	RAR
	MOV B,A
	RC
	INX H
	RET
LEFT: MOV A,B
	RLC
	CMC
	RAR
	MOV B,A
	RNC
	DCX H
	RET
WHITE: MOV A,B
	ORI 60H
	MOV B,A
	RET
BLACK: MOV A,B
	ORI 20H
	ANI 0BFH
	MOV B,A
	RET
MOVE: MOV A,B
	ANI 9FH
	MOV B,A
	RET
ONE1: CALL UP
	CALL RIGHT
	RET
TWO: CALL UP
	CALL LEFT
	RET
THREE: CALL DOWN
	CALL LEFT
	RET
FOUR: CALL DOWN
	CALL RIGHT
	RET
MASK1: MOV A,B
	RLC
	RAR
	JNC CLEAR
	ADI 3
CLEAR: ANI 1FH
	MVI D,TAD1
	MOV E,A
	LDAX D
	RET
                           ;THIS ROUTINE KILLS THE GENERATION
                           ;COUNTER WHEN JUPING INTO A MODIFIED
                           ;LIFE SO THE OCUNTER DOSN'T BECOME LIFE
CLNO:  LXI    H,VADD
CLNO1: MOV    A,L
	CPI     40H     ;END OF FIRST LINE
	JZ      2003H   ;BEGINING OF LIFE PROGRAM
	MOV     A,M
	ANI     80H     ;IS IT GRAPHICS? IF N0
	CNZ     BLANK   ;PUT IN GRAPHICS BLANK
	INX     H
	JMP     CLNO1
BLANK: MVI M,3FH
	RET