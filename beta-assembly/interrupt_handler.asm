.include beta.uasm  |; Include beta.uasm file for macro definition

PUSH(LP) PUSH(BP) 
MOVE(SP, BP)        |; Initialize base of frame pointer (BP)
BR(main)            |; Go to 'main' code segment

main:    
    |; save registers
    PUSH(R0) PUSH(R1)
    PUSH(R2) PUSH(R3) PUSH (R4)
    LD(BP, -12, R3) |; R3 <- PROGRAM_MEMORY_SZ + VIDEO_MEMORY_SZ

    |; R0 <- interrupt_nb
    LD(R3, 13, R0)
    ANDC(R0,0xff,R0)

    |; R1 <- char
    LD(R3, 14, R1)
    ANDC(R1,0xff,R1)

    |; R2 <- buf_index
    LD(R3, 15, R2)
    ANDC(R2,0xff,R2)

    |; if interrupt_nb = 0 go to interrupt_1
    BF(R0, interrupt_0)

|; if interrupt_nb = 1
interrupt_1:
    |; Pressed[Char] = 0
    ADD(R3, R1, R0)
    LD(R0,272,R4)
    ANDC(R4,0xffffff00,R4)
    OR(R4,R31,R4)
    ST(R4, 272,R0)

    BR(rtn)

interrupt_0:
    |; Buf[buf_index] = Char
    ADD(R3, R2, R0)
    LD(R0,16,R4)
    ANDC(R4,0xffffff00,R4)
    OR(R4,R1,R4)
    ST(R4, 16, R0)
    
    |; Pressed[Char] = 1
    ADD(R3, R1, R0)
    ADDC(R31, 1, R1)
    LD(R0,272,R4)
    ANDC(R4,0xffffff00,R4)
    OR(R4,R1,R4)
    ST(R4, 272,R0)
    
    |; if buf_index = 255
    CMPEQC(R2, 255, R0)
    BT(R0, circ_buf_end)

    |; buf_index++
    ADDC(R2, 1, R2)
    LD(R3, 15, R4)
    ANDC(R4,0xffffff00,R4)
    OR(R4, R2, R4)
    ST(R4, 15, R3)

    BR(rtn)

circ_buf_end:
    |; buf_index = 0
    LD(R3, 15, R2)
    ANDC(R2,0xffffff00,R2)
    ST(R2, 15, R3)

rtn:
    POP(R4) POP(R3) POP(R2)
    POP(R1) POP(R0)
    ADDC(BP, 0, SP) |; Restore SP
    POP(BP) |; Restore BP
    POP(LP) |; Restore return address
    JMP(XP) |; Return

