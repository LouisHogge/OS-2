.include beta.uasm  |; Include beta.uasm file for macro definition

CMOVE(stack, SP)    |; Initialize stack pointer (SP) 
MOVE(SP, BP)        |; Initialize base of frame pointer (BP)
BR(main)            |; Go to 'main' code segment

n:
  LONG(3)           |; The number to compute the factorial 
                    |; (not too big otherwise you will have inconsistencies...)

main:
  LD(R31, n, R1)    |; Load n into R1
  PUSH(R1)          |; Push parameter 1: n
  CALL(fact)        |; CALL(fact) stands for BEQ(R31, fact, LP)
  DEALLOCATE(1)     |; Check the result of R0 after factorial execution (/!\ it is in hexa)
  HALT()            |; Stop execution of the program

fact: 
  PUSH(LP)          |; Save the return address
  PUSH(BP)          |; Save the previous frame
  ADDC(SP,0,BP)     |; Initialize the current frame (same as MOVE(SP,BP))
  PUSH(R1)          |; R1 is used in the function, save it 
  LD(BP,-12,R1)     |; Load argument n into R1 
  BNE(R1,big)       |; Compare n to 0
  ADDC(R31,1,R0)    |; n=0, return 1
  BEQ(R31,rtn,R31)  |; Go to the return sequence (same as BR(rtn))

big:  
  SUBC(R1,1,R1)     |; Compute n-1 into R1
  PUSH(R1)          |; Put the argument for recursive call on stack
  BEQ(R31,fact,LP)  |; Recursive call
  DEALLOCATE(1)     |; Free space 
  LD(BP,-12,R1)     |; Load n into R1
  MUL(R1,R0,R0)     |; n*fact(n-1) into R0

rtn:  
  POP(R1)           |; Restore R1
  ADDC(BP,0,SP)     |; Restore SP
  POP(BP)           |; Restore BP
  POP(LP)           |; Restore return address
  JMP(LP,R31)       |; Return


LONG(0xDEADCAFE)    |; A readable hex number to easily locate the 
                    |; stack in the simulator
stack: 
    STORAGE(1024)
|; End of file