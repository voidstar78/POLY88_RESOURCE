; SAVETB.ASM   DKE 2-13-00
;  Adds extended operations to Tiny Basic. This includes
;  a SAVE, PEEK and POKE operations. PEEK is treated as
;  a function. POKE is a command like LET and SAVE is a
;  command with no parameter until executed. When SAVE
;  is executed, it will prompt for P or B and file name
;  just like the monitor does. Files saved with this command
;  will auto start TB with the file save ready to execute.

WH0     EQU     0C20H   ; Monitor serial in
WH1     EQU     0C24H   ; Monitor serial out
SRA4    EQU     0C16H   ; Keyboard interupt vector
SETUP   EQU     02ADH   ; Init USART
HEXC    EQU     03AAH   ; Hex converter
DEOUT   EQU     03D1H   ; Prints DE as hex
TBUFF   EQU     0C08H   ; Input buffer

EXEC    EQU     04DBH   ; Tiny Basics execute table
PARN    EQU     0870H   ; TB gets ( expr ) to HL
SETVAL  EQU     0904H   ; TB sets variables
NOTF    EQU     085FH   ; TB fetch variable
FINISH  EQU     091AH   ; TB To finish line
WARMTB  EQU     0433H   ; TB warm entry for auto start

        ORG     4000H
        ; TB extended operations
INITEX: JMP     INITTB  ; any initialization needed
CMNDEX: JMP     CMNDEXEC        ; command extensions
FUNCEX: JMP     FUNCEXEC        ; function extensions data ret in HL
SIZEEX: DW      ENDEX-4000H     ; size of this to save with prog

LENGTH: DS      2
WNAME:  DS      8
WRN:    DS      2
WLEN:   DS      1
WADR:   DS      2
WTYPE:  DS      1

SAVE:   PUSH    D
        LXI     H,TISR
        SHLD    SRA4
STAR2:  MVI     A,0CH
        CALL    WH1
        CALL    WH0     ; get record method
        CALL    WH1
        CPI     'B'
        JZ      BITE
        CPI     'P'
        JNZ     STAR2
POLY:   CALL    SETUP   ; Biphase
        DB      005H
        DB      0AAH
        DB      040H
        DB      00CH
        DB      0E6H
        DB      0E6H
        DB      000H
        JMP     NAMER
BITE:   CALL    SETUP   ; Byte Format
        DB      006H
        DB      0AAH
        DB      040H
        DB      0CEH
        DB      000H
NAMER:  LXI     H,0     ; fetch name
        SHLD    WRN
        MVI     C,8     ; 8 Charecters max
        LXI     H,WNAME+7
NAM:    MVI     M,020H
        DCX     H
        DCR     C
        JNZ     NAM
        INX     H
        MVI     C,8
        CALL    CRLF
NAM0:   CALL    WH0
        CALL    WH1
        CPI     00DH
        JZ      DUMPC
        MOV     M,A
        INX     H
        DCR     C
        JNZ     NAM0
DUMPC:  XRA     A        ; Data type
        STA     WTYPE
SIZE:   LXI     H,02000H ; text code starts here
        SHLD    WADR
        MOV     A,B
        XCHG
        LHLD    02000H ; calc data length
        MOV     A,L
        SUB     E
        MOV     L,A
        MOV     A,H
        SBB     D
        MOV     H,A
        SHLD    LENGTH
        CALL    DUMPR   ; Save TB text program
 ; If TB finds 0C3H at 0C80H it doesn't clear
 ;  text on start. Also the address points
 ;  to this program at 4000H, 4003H and 4006H.
 ;  See these addresses. This allows auto-start
 ;  of TB with save program im memory.
TBOK:   LXI     H,0C80H ; Check for TBASIC to keep
        SHLD    WADR    ;  from overwriting data
        LXI     H,0003H ;  just loaded and save extend pntr.
        SHLD    LENGTH  ;  Only need 3 bytes
        CALL    DUMPR
 ; Need to save these extended commands as well
        LXI     H,4000H ; Save and these extentions
        SHLD    WADR
        LHLD    SIZEEX  ; Size of all this stuff
        SHLD    LENGTH
        CALL    DUMPR
ENDC:   MVI     A,3     ; Auto Start record
        STA     WTYPE
        MVI     A,1     ; Min length needed
        STA     WLEN
        LXI     H,WARMTB   ; Tiny Basic start address
        SHLD    WADR    ;  location to start
        CALL    DUMP    ; last record
        POP     D
        JMP     FINISH  ; Let TB finish line

DUMPR:  LXI     H,LENGTH+1
        MOV     A,M
        ORA     A
        JZ      OVER ; less then 256 to do?
        DCR     M
        XRA     A
        STA     WLEN
        CALL    DUMP ; dump a 256 block
        LHLD    WADR
        INR     H
        SHLD    WADR
        JMP     DUMPR
OVER:   DCX     H    ; dump last parts
        MOV     A,M
        STA     WLEN
        JMP     DUMP

CRLF:   MVI     A,0DH
        CALL    WH1
        RET

PUT:    MVI     B,0
        MOV     C,A
PUT0:   MOV     A,M
        INX     H
        PUSH    PSW
        ADD     B
        MOV     B,A
        POP     PSW
        CALL    TO
        DCR     C
        JNZ     PUT0
        MOV     A,B
        CMA
        INR     A
        JMP     TO

TO:     PUSH    H
        LXI     H,TBUFF
        PUSH    PSW
TO1:    MOV     A,M
        ORA     A
        JNZ     TO1
        INX     H
        POP     PSW
        MOV     M,A
        DCX     H
        INR     M
        POP     H
        RET

TISR:   XRA     A
        STA     TBUFF
        LDA     TBUFF+1
        OUT     0
IORET:  POP     H
        POP     D
        POP     B
        POP     PSW
        EI
        RET

DUMP:   MVI     A,021H
        OUT     1
        LHLD    WADR
        XCHG
        CALL    DEOUT
        CALL    CRLF
        LXI     H,08FFFH
DELAY:  DCX     H
        MOV     A,H
        ORA     A
        JNZ     DELAY
        MVI     C,64
        MVI     A,0E6H
DUMP0:  CALL    TO
        DCR     C
        JNZ     DUMP0
        MVI     A,001H
        CALL    TO

        MVI     A,00EH
        LXI     H,WNAME
        CALL    PUT
        LDA     WLEN
        LHLD    WADR
        CALL    PUT
        LXI     H,WRN
        INR     M
OFF:    XRA     A       ; motor off
        CALL    TO
        CALL    TO
        CALL    TO
        OUT    1
        RET
ENDSAV:
 ; These are commands and functions that I have
 ;  added.
 ;  PEEK(Expr) can be used in place of any thing
 ;   that evaluates Expr, such as,  A=PEEK(1000)
 ;   Locations over 32767 are negative values.
 ;  POKE(Expr)=Expr is like the LET function and requires
 ;   an expression to be equal to. As an example,
 ;   POKE(1000)=A will set location 1000 to the value
 ;   of variable A.
 ;  SAVE is a command. It is just like the monitors
 ;   tape load. It first waits for P or B depending on
 ;   format desired. Next one enters the name of the
 ;   program to save. Start the tape before typing enter
 ;   or the last of 8 characters. It will save the text
 ;   area of TB and will save any extensions tha start
 ;   at 4000H and have the length to save at 400CH, labled
 ;   SIZEEX in this program. Save returns to TB. To load
 ;   these saved programs, use the monitor tape load.
 ;   TB will auto start. Programs can then be run with R.

CMNDEXEC:               ; Extended commands
        LXI     H,CMNDTBL-1
        JMP     EXEC
FUNCEXEC:                 ; Extended functions
        LXI     H,FUNCTBL-1
        JMP     EXEC
INITTB:                 ; Any initialization needed
DONE:   RET

POKE:   CALL    PARN    ; get expr
        JMP     SETVAL+6        ; evaluates '=' part and stores

PEEK:   CALL    PARN    ; get expr
        POP     PSW     ; purge ret addr
        JMP     NOTF+6  ; fetch like variable

CMNDTBL:
        DB      'SAVE'
        DB      SAVE SHR 8 OR 128,SAVE AND 255
        DB      'POKE'
        DB      POKE SHR 8 OR 128,POKE AND 255
        DB      DONE SHR 8 OR 128,DONE AND 255

FUNCTBL:
        DB      'PEEK'
        DB      PEEK SHR 8 OR 128,PEEK AND 255
        DB      DONE SHR 8 OR 128,DONE AND 255

ENDEX:
        END



 