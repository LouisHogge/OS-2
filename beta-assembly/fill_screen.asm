.include beta.uasm  |; Include beta.uasm file for macro definition

BR(main)

video_memory_start:
    LONG(33554432)

n:
    LONG(240000)
    
blue:
    LONG(0xFF9933)

main:
    LD(R31, video_memory_start, R1)
    LD(R31, n, R2)
    LD(R31, blue, R3)
    ADDC(R31, 5, R0)
loop:
    ST(R3, 0, R1)
    SUBC(R2, 1, R2)
    ADDC(R1, 4, R1)
    BNE(R2, loop)
    HALT()

|; End of file