#ifndef EMULATOR_H__
#define EMULATOR_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* suggested parameters for init_computer() calls by the GUI
   Editing PROGRAM_MEMORY (32MB is plenty) and VIDEO_MEMORY most likely will
   not be necessary but you may want to adapt KERNEL_MEMORY
   according to the size of your interrupt handler and other
   kernel facilities */ 
#define PROGRAM_MEMORY_SZ (32 * 1024 * 1024)
#define VIDEO_MEMORY_SZ (600 * 400 * 4) // must be 3:2 aspect ratio
#define KERNEL_MEMORY_SZ 800

typedef struct{
	 
    long program_counter;
    
    // add your own fields here !
    int registers[32];
    int backup;
    
} CPU;

typedef struct{

    CPU cpu;
    
    long memory_size; // = program_memory_size + video_memory_size + kernel_memory_size
    long program_memory_size;
    long video_memory_size;
    long kernel_memory_size;
    long latest_accessed; // address of the word most recently loaded/stored from/into memory
    bool halted; // was the HALT() instruction executed (stopping the program's execution)
    unsigned program_size; // user-space program size (code + stack)
    
    // add your own fields here !
    unsigned char* memory;
    bool interrupt_raised;
    char interrupt_type;
    char interrupt_keyval;
    
} Computer;

static char* reg_symbols[32] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9",
                                "R10", "R11", "R12", "R13", "R14", "R15", "R16", "R17", "R18",
                                "R19", "R20", "R21", "R22", "R23", "R24", "R25", "R26", "BP",
                                "LP", "SP", "XP", "R31"};

/* Initializes the computer data structure, must be run before any other function
   manipulating the computer */
void init_computer(Computer* c, long program_memory_size, 
                                long video_memory_size, long kernel_memory_size);

/*  Reads a 32-bit word at the address $addr from the computer's 
    memory.
    Return value: the word found at addr. If addr > c -> 
    memory_size, 0 will be returned. If addr is a valid address 
    found at the boundary of the computer's memory (i.e. there is 
    less than a full 4-byte word to return, then the function 
    will return the valid bytes followed by a padding of 0-bytes. */
int get_word(Computer* c, long addr);

/* Returns the value of a given register of computer c, reg is 
the register's number between 0 and 31. */
int get_register(Computer* c, int reg);

/* Frees all resources allocated for the computer data structure. */
void free_computer(Computer* c);

/* Loads the binary at the beginning of the computer's memory,
   c -> program_size becomes the size of the binary in bytes. */
void load(Computer* c, FILE* binary);

/* Loads the interrupt handler binary in $c's kernel memory.
   $binary can be NULL, in which case the function does nothing.
   The $binary is placed after the kernel's data structures
   (see statement for a diagram of kernel memory). */
void load_interrupt_handler(Computer* c, FILE* binary);

/* Runs one fetch + decode + execute cycle of $c's CPU,
   If an interrupt line is raised (and the computer is not
   already executing the interrupt handler), the program counter
   becomes the start address of the interrupt handler 
   and its first instruction is executed. 
   Before handing control to the interrupt handler, the CPU
   places the interrupt number and associated character at
   the adequate place in kernel memory (see statement) and     
   stores PC into XP so that the interrupt handler is able to 
   return. */
void execute_step(Computer* c);

/* Raise an interrupt line of computer $c if no other already is. 
   Otherwise, this does nothing.  $type is the interrupt number
   while $keyval is the associated character. */
void raise_interrupt(Computer* c, char type, char keyval);

/* Stores a textual representation of the disassembly of 
   $instruction in the buffer $buf. We assume that $buf
   is large enough to store any disassembled instruction.
   If $instruction is not a valid instruction (see slides
   on the Beta-assembly instruction set), then the 
   "INVALID" string is stored instead.
   Returns 0 if $instruction is valid, and a negative
   value otherwise. */
int disassemble(int instruction, char* buf);

/* Extracts a 16-bit literal value from a 32-bit input. */
int32_t extract_literal(int32_t input);

/* Arithmetic right shift operation. */
int arithmetic_right_shift(int x, int y);

#endif
