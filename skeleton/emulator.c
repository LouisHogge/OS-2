#include "emulator.h"
#include <string.h>
#include <sys/stat.h>

void init_computer(Computer* c, long program_memory_size, long video_memory_size, long kernel_memory_size){

    c->memory_size = program_memory_size + video_memory_size + kernel_memory_size;
    c->program_memory_size = program_memory_size;
    c->video_memory_size = video_memory_size;
    c->kernel_memory_size = kernel_memory_size;
    c->cpu.program_counter = 0;
    c->program_size = 0;
    c->latest_accessed = -1;
    c->halted = false;
    c->interrupt_raised = false;
    c->memory = (unsigned char*) malloc(c->memory_size * sizeof(unsigned char));
}

int get_word(Computer* c, long addr){
    if(addr >= c->memory_size) {
	fprintf(stderr, "Error: Invalid memory address\n");
        return 0;
    } else if( addr + 4 > c->memory_size) {
        int word = 0;
        memcpy(&word,&c->memory[addr],c->memory_size - addr);
        c->latest_accessed = addr;
        return word;
    } else {
        int word = 0;
        memcpy(&word,&c->memory[addr],4);
        c->latest_accessed = addr;
        return word;
    }
}

int get_register(Computer* c, int reg){
    if(reg >= 0 && reg < 31) { 
        return c->cpu.registers[reg];
    } else if(reg == 31) {
        return 0;
    } else {
        fprintf(stderr, "Error: Invalid register index.\n");
        return -1; 
    }
}

void free_computer(Computer* c){
    if (c->memory != NULL) {
        free(c->memory);
        c->memory = NULL; 
    }
}

void load(Computer* c, FILE* binary){
    
    if(binary == NULL) {
        fprintf(stderr, "Error: Cannot load from NULL binary.\n");
        return;
    }
    
    fseek(binary, 0, SEEK_END);
    long filesize = ftell(binary);
    fseek(binary, 0, SEEK_SET);
    
    if(filesize > c->program_memory_size) {
        fprintf(stderr, "Error: Binary file is too large for the program memory.\n");
        return;
    }
    
    size_t read_size = fread(c->memory, 1, filesize, binary);

    if(read_size != filesize) {
        fprintf(stderr, "Error: Only read %ld bytes from binary, expected %ld bytes.\n", read_size, filesize);
        return;
    }
    c->program_size = filesize;

    if(read_size >= 4) {
        c->latest_accessed = (long)(read_size - 4);
    } else {
        c->latest_accessed = (long)0;
    }
}

void load_interrupt_handler(Computer* c, FILE* binary){
    
    if (binary == NULL) {
        return;
    }

    // Check if binary is empty
    fseek(binary, 0, SEEK_END);
    size_t size = ftell(binary);
    fseek(binary, 0, SEEK_SET);

    if (size > c->kernel_memory_size - 400) {
        fprintf(stderr, "Interrupt handler too large for kernel memory.\n");
        return;
    }
    
    size_t read = fread(c->memory + c->program_memory_size + c->video_memory_size + 400, 1, size, binary);
    if (read < size) {
        fprintf(stderr, "Could not read the entire kernel binary.\n");
        exit(1);
    }

    long progVidMemSize = c->program_memory_size + c->video_memory_size;
    if(read >= 4) {
        c->latest_accessed = (long)(progVidMemSize + 400 + read - 4);
    } else {
        c->latest_accessed = (long)(progVidMemSize + 400);
    }
    
}

int arithmetic_right_shift(int x, int y) {
    if (x < 0) {
        x = x >> y;
        int32_t mask = 2147483648; 

        return x | mask;
    }

    return x >> y;
}

void execute_step(Computer* c){
    if(c->cpu.program_counter < c->program_memory_size) {
        c->interrupt_raised = false;
    }

    int instruction = get_word(c, c->cpu.program_counter);
    int32_t opcode = (instruction >> 26) & 0x3F;
    int32_t Rc = (instruction >> 21) & 0x1F;
    int32_t Ra = (instruction >> 16) & 0x1F;
    int32_t Rb = (instruction >> 11) & 0x1F;
    int32_t int32 = instruction;
    int32_t literal = extract_literal(int32);

    int temp = 0;
    int temp2 = 0;

    switch(opcode) {
        case 0x00:  // HALT
            c->halted = true;
            break;
        case 0x18: // LD
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = *((int32_t*) &(c->memory[temp + literal]));
            c->latest_accessed = (long)(temp + literal);
            break;
    	case 0x19: // ST
            c->cpu.program_counter += 4;
            temp2 = get_register(c,Rc);
            temp = get_register(c,Ra);
            *((int32_t*) &(c->memory[temp + literal])) = temp2;
            c->latest_accessed = (long)(temp + literal);
            break;
        case 0x1B: // JMP
            c->cpu.program_counter += 4;
            c->cpu.registers[Rc]=c->cpu.program_counter;
            temp = get_register(c,Ra);
            c->cpu.program_counter = temp & 0xFFFFFFFC; 
            break;
        case 0x1D: // BEQ
            c->cpu.program_counter += 4;
            c->cpu.registers[Rc]=c->cpu.program_counter;
            temp = get_register(c,Ra);
            if(temp == 0) {
                c->cpu.program_counter = c->cpu.program_counter + 4 * literal; 
            }
            break;
        case 0x1E: // BNE
            c->cpu.program_counter += 4;
            c->cpu.registers[Rc]=c->cpu.program_counter;
            temp = get_register(c,Ra);
            if(temp != 0) {
                c->cpu.program_counter = c->cpu.program_counter + 4 * literal; 
            }
            break;
        case 0x1F: // LDR
            if(c->cpu.program_counter + 4 + 4 * literal > c->program_memory_size + c->video_memory_size) {
                c->cpu.program_counter += 4;
                temp2 = get_register(c,Rc);
                *((int32_t*) &(c->memory[c->cpu.program_counter + 4*literal])) = temp2;
                c->latest_accessed = (long)(c->cpu.program_counter + 4*literal);
            } else {
                c->cpu.program_counter += 4;
                c->cpu.registers[Rc] = *((int32_t*) &(c->memory[c->cpu.program_counter + 4*literal]));
                c->latest_accessed = (long)(c->cpu.program_counter + 4*literal);
            }
            break;
        case 0x20:  // ADD
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp + temp2;
            break;
        case 0x21:  // SUB
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp - temp2;
            break;
        case 0x22:  // MUL
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp * temp2;
            break;
        case 0x23:  // DIV
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp / temp2;
            break;
        case 0x24:  // CMPEQ
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp == temp2;
            break;
        case 0x25:  // CMPLT
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp < temp2;
            break;
        case 0x26:  // CMPLE
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp <= temp2;
            break;
        case 0x28:  // AND
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp & temp2;
            break;
        case 0x29:  // OR
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp | temp2;
            break;
        case 0x2A:  // XOR
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = temp ^ temp2;
            break;
        case 0x2C:  // SHL
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            temp2 = temp2 & 0x1F;
            c->cpu.registers[Rc] = temp << temp2;
            break;
        case 0x2D:  // SHR 
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            temp2 = temp2 & 0x1F;
            c->cpu.registers[Rc] = temp >> temp2;
            break;
        case 0x2E:  // SRA
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            temp2 = get_register(c,Rb);
            c->cpu.registers[Rc] = arithmetic_right_shift(temp,temp2);
            break;
        case 0x30:  // ADDC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp + literal;
            break;
        case 0x31:  // SUBC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp - literal;
            break;
        case 0x32:  // MULC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp * literal;
            break;
        case 0x33:  // DIVC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp / literal;
            break;
        case 0x34:  // CMPEQC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp == literal;
            break;
        case 0x35:  // CMPLTC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp < literal;
            break;
        case 0x36:  // CMPLEC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp <= literal;
            break;
        case 0x38:  // ANDC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp & literal;
            break; 
        case 0x39:  // ORC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp | literal;
            break; 
        case 0x3A:  // XORC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = temp ^ literal;
            break; 
        case 0x3C:  // SHLC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            literal = literal & 0x1F;
            c->cpu.registers[Rc] = temp << literal;
            break; 
        case 0x3D:  // SHRC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            literal = literal & 0x1F;
            c->cpu.registers[Rc] = temp >> literal;
            break; 
        case 0x3E:  // SRAC
            c->cpu.program_counter += 4;
            temp = get_register(c,Ra);
            c->cpu.registers[Rc] = arithmetic_right_shift(temp,literal);
            break;         
        default:
            fprintf(stderr, "Error: Opcode %d not yet implemented.\n",opcode);
    }
}

void raise_interrupt(Computer* c, char type, char keyval){
    if(!c->interrupt_raised) {
        c->interrupt_raised =  true;

        long addr = c->program_memory_size + c->video_memory_size;
        c->cpu.registers[30] = c->cpu.program_counter;
        c->cpu.program_counter = addr + 400;

        c->cpu.registers[29] = c->program_size+4;
        *((int32_t*) (c->memory + c->program_size)) = c->program_memory_size + c->video_memory_size;
 
        if(type == 0) {
            c->memory[addr+13] = type;
            c->latest_accessed = (long)(addr+13);
            c->memory[addr+13+1] = keyval;
            c->latest_accessed = (long)(addr+14);
        } else if (type == 1) {
            c->memory[addr+13] = type;
            c->latest_accessed = (long)(addr+13);
        }
    }
}

int32_t extract_literal(int32_t input) {
    int16_t literal = input & 0xFFFF;

    if (literal & 0x8000) {
        literal |= 0xFFFF0000;
    }

    return literal;
}

int disassemble(int instruction, char* buf) {
    int opcode = (instruction >> 26) & 0x3F;
    int Rc = (instruction >> 21) & 0x1F;
    int Ra = (instruction >> 16) & 0x1F;
    int Rb = (instruction >> 11) & 0x1F;
    int32_t int32 = instruction;
    int32_t literal = extract_literal(int32);

    switch (opcode) {
        case 0x00:
            sprintf(buf, "HALT");
            break;
        case 0x18:
            sprintf(buf, "LD(R%d,%d,R%d)", Ra, literal, Rc);
            break;
        case 0x19:
            sprintf(buf, "ST(R%d,%d,R%d)", Rc, literal, Ra);
            break;
        case 0x1B: 
            sprintf(buf, "JMP(R%d,R%d)", Ra, Rc);
            break;
        case 0x1D:
            sprintf(buf, "BEQ(R%d,%d,R%d)", Ra, literal, Rc);
            break;
        case 0x1E:
            sprintf(buf, "BNE(R%d,%d,R%d)", Ra, literal, Rc);
            break;
        case 0x1F:
            sprintf(buf, "LDR(%d,R%d)", literal, Rc);
            break;
        case 0x20:
            sprintf(buf, "ADD(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x21:
            sprintf(buf, "SUB(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x22:
            sprintf(buf, "MUL(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x23:
            sprintf(buf, "DIV(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x24:
            sprintf(buf, "CMPEQ(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x25:
            sprintf(buf, "CMPLT(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x26:
            sprintf(buf, "CMPLE(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x28:
            sprintf(buf, "AND(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x29:
            sprintf(buf, "OR(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x2A:
            sprintf(buf, "XOR(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x2C:
            sprintf(buf, "SHL(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x2D:
            sprintf(buf, "SHR(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x2E:
            sprintf(buf, "SRA(R%d,R%d,R%d)", Ra, Rb, Rc);
            break;
        case 0x30:
            sprintf(buf, "ADDC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x31:
            sprintf(buf, "SUBC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x32:
            sprintf(buf, "MULC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x33:
            sprintf(buf, "DIVC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x34:
            sprintf(buf, "CMPEQC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x35:
            sprintf(buf, "CMPLTC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x36:
            sprintf(buf, "CMPLEC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x38:
            sprintf(buf, "ANDC(R%d,%d,R%d)",Ra,literal,Rc);
            break; 
        case 0x39:
            sprintf(buf, "ORC(R%d,%d,R%d)",Ra,literal,Rc);
            break; 
        case 0x3A:
            sprintf(buf, "XORC(R%d,%d,R%d)",Ra,literal,Rc);
            break; 
        case 0x3C:
            sprintf(buf, "SHLC(R%d,%d,R%d)",Ra,literal,Rc);
            break; 
        case 0x3D:
            sprintf(buf, "SHRC(R%d,%d,R%d)",Ra,literal,Rc);
            break;
        case 0x3E:
            sprintf(buf, "SRAC(R%d,%d,R%d)",Ra,literal,Rc);
            break;  
        default:
            sprintf(buf, "INVALID");
	    return -1;
    }

    return 0;
}

