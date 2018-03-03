#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>

//Emulator (Processor State, etc)
typedef struct Flags {
    uint8_t     z:1; // (zero) set to 1 when the result is 0
    uint8_t     s:1; // (sign) set to 1 when bit 7 (the most significant bit or MSB) of the math instruction is set
    uint8_t     p:1; // (parity) is set when the answer has even parity, clear when odd parity. PS: Trivia, have a look at (SEU) single event upsets.
    uint8_t     cy:1; // (carry) set to 1 when the instruction resulted in a carry out or borrow into the high order bit
    uint8_t     ac:1; // auxillary carry) is used mostly for BCD (binary coded decimal) math.
    uint8_t     pad:3;
} Flags;

typedef struct State8080 {
    // 7 x 8 bit registers
    uint8_t     a;
    uint8_t     b;
    uint8_t     c;
    uint8_t     d;
    uint8_t     e;
    uint8_t     h;
    uint8_t     l;

    //the stack pointer and program counter are 16 bits each
    uint16_t    sp; //stack pointer
    uint16_t    pc; //program counter

    uint8_t     *memory;
    struct      Flags flags;
    uint8_t     int_enable;
} State8080;

State8080* state;

//SDL
const int SCREEN_WIDTH   = 640;
const int SCREEN_HEIGHT  = 480;
const char* SCREEN_TITLE = "8080 Emulator";

SDL_Window* window;
SDL_Renderer* renderer;

int Parity(int x, int size)
{
    int i;
    int p = 0;
    x = (x & ((1<<size)-1));
    for (i=0; i<size; i++)
    {
        if (x & 0x1) p++;
        x = x >> 1;
    }
    return (0 == (p & 0x1));
}

int Disassemble8080Opcodes(unsigned char *codebuffer, int pc)
{
    unsigned char *code = &codebuffer[pc];
    int opbytes = 1;
    printf("%04x ", pc);
    switch (*code)
    {
        case 0x00: printf("NOP"); break;
        case 0x01: printf("LXI    B,#$%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x02: printf("STAX   B"); break;
        case 0x03: printf("INX    B"); break;
        case 0x04: printf("INR    B"); break;
        case 0x05: printf("DCR    B"); break;
        case 0x06: printf("MVI    B,#$%02x", code[1]); opbytes=2; break;
        case 0x07: printf("RLC"); break;
        case 0x08: printf("NOP"); break;
        case 0x09: printf("DAD    B"); break;
        case 0x0a: printf("LDAX   B"); break;
        case 0x0b: printf("DCX    B"); break;
        case 0x0c: printf("INR    C"); break;
        case 0x0d: printf("DCR    C"); break;
        case 0x0e: printf("MVI    C,#$%02x", code[1]); opbytes = 2;    break;
        case 0x0f: printf("RRC"); break;
            
        case 0x10: printf("NOP"); break;
        case 0x11: printf("LXI    D,#$%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x12: printf("STAX   D"); break;
        case 0x13: printf("INX    D"); break;
        case 0x14: printf("INR    D"); break;
        case 0x15: printf("DCR    D"); break;
        case 0x16: printf("MVI    D,#$%02x", code[1]); opbytes=2; break;
        case 0x17: printf("RAL"); break;
        case 0x18: printf("NOP"); break;
        case 0x19: printf("DAD    D"); break;
        case 0x1a: printf("LDAX   D"); break;
        case 0x1b: printf("DCX    D"); break;
        case 0x1c: printf("INR    E"); break;
        case 0x1d: printf("DCR    E"); break;
        case 0x1e: printf("MVI    E,#$%02x", code[1]); opbytes = 2; break;
        case 0x1f: printf("RAR"); break;
            
        case 0x20: printf("NOP"); break;
        case 0x21: printf("LXI    H,#$%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x22: printf("SHLD   $%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x23: printf("INX    H"); break;
        case 0x24: printf("INR    H"); break;
        case 0x25: printf("DCR    H"); break;
        case 0x26: printf("MVI    H,#$%02x", code[1]); opbytes=2; break;
        case 0x27: printf("DAA"); break;
        case 0x28: printf("NOP"); break;
        case 0x29: printf("DAD    H"); break;
        case 0x2a: printf("LHLD   $%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x2b: printf("DCX    H"); break;
        case 0x2c: printf("INR    L"); break;
        case 0x2d: printf("DCR    L"); break;
        case 0x2e: printf("MVI    L,#$%02x", code[1]); opbytes = 2; break;
        case 0x2f: printf("CMA"); break;
            
        case 0x30: printf("NOP"); break;
        case 0x31: printf("LXI    SP,#$%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x32: printf("STA    $%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x33: printf("INX    SP"); break;
        case 0x34: printf("INR    M"); break;
        case 0x35: printf("DCR    M"); break;
        case 0x36: printf("MVI    M,#$%02x", code[1]); opbytes=2; break;
        case 0x37: printf("STC"); break;
        case 0x38: printf("NOP"); break;
        case 0x39: printf("DAD    SP"); break;
        case 0x3a: printf("LDA    $%02x%02x", code[2], code[1]); opbytes=3; break;
        case 0x3b: printf("DCX    SP"); break;
        case 0x3c: printf("INR    A"); break;
        case 0x3d: printf("DCR    A"); break;
        case 0x3e: printf("MVI    A,#$%02x", code[1]); opbytes = 2; break;
        case 0x3f: printf("CMC"); break;
            
        case 0x40: printf("MOV    B,B"); break;
        case 0x41: printf("MOV    B,C"); break;
        case 0x42: printf("MOV    B,D"); break;
        case 0x43: printf("MOV    B,E"); break;
        case 0x44: printf("MOV    B,H"); break;
        case 0x45: printf("MOV    B,L"); break;
        case 0x46: printf("MOV    B,M"); break;
        case 0x47: printf("MOV    B,A"); break;
        case 0x48: printf("MOV    C,B"); break;
        case 0x49: printf("MOV    C,C"); break;
        case 0x4a: printf("MOV    C,D"); break;
        case 0x4b: printf("MOV    C,E"); break;
        case 0x4c: printf("MOV    C,H"); break;
        case 0x4d: printf("MOV    C,L"); break;
        case 0x4e: printf("MOV    C,M"); break;
        case 0x4f: printf("MOV    C,A"); break;
            
        case 0x50: printf("MOV    D,B"); break;
        case 0x51: printf("MOV    D,C"); break;
        case 0x52: printf("MOV    D,D"); break;
        case 0x53: printf("MOV    D.E"); break;
        case 0x54: printf("MOV    D,H"); break;
        case 0x55: printf("MOV    D,L"); break;
        case 0x56: printf("MOV    D,M"); break;
        case 0x57: printf("MOV    D,A"); break;
        case 0x58: printf("MOV    E,B"); break;
        case 0x59: printf("MOV    E,C"); break;
        case 0x5a: printf("MOV    E,D"); break;
        case 0x5b: printf("MOV    E,E"); break;
        case 0x5c: printf("MOV    E,H"); break;
        case 0x5d: printf("MOV    E,L"); break;
        case 0x5e: printf("MOV    E,M"); break;
        case 0x5f: printf("MOV    E,A"); break;

        case 0x60: printf("MOV    H,B"); break;
        case 0x61: printf("MOV    H,C"); break;
        case 0x62: printf("MOV    H,D"); break;
        case 0x63: printf("MOV    H.E"); break;
        case 0x64: printf("MOV    H,H"); break;
        case 0x65: printf("MOV    H,L"); break;
        case 0x66: printf("MOV    H,M"); break;
        case 0x67: printf("MOV    H,A"); break;
        case 0x68: printf("MOV    L,B"); break;
        case 0x69: printf("MOV    L,C"); break;
        case 0x6a: printf("MOV    L,D"); break;
        case 0x6b: printf("MOV    L,E"); break;
        case 0x6c: printf("MOV    L,H"); break;
        case 0x6d: printf("MOV    L,L"); break;
        case 0x6e: printf("MOV    L,M"); break;
        case 0x6f: printf("MOV    L,A"); break;

        case 0x70: printf("MOV    M,B"); break;
        case 0x71: printf("MOV    M,C"); break;
        case 0x72: printf("MOV    M,D"); break;
        case 0x73: printf("MOV    M.E"); break;
        case 0x74: printf("MOV    M,H"); break;
        case 0x75: printf("MOV    M,L"); break;
        case 0x76: printf("HLT");        break;
        case 0x77: printf("MOV    M,A"); break;
        case 0x78: printf("MOV    A,B"); break;
        case 0x79: printf("MOV    A,C"); break;
        case 0x7a: printf("MOV    A,D"); break;
        case 0x7b: printf("MOV    A,E"); break;
        case 0x7c: printf("MOV    A,H"); break;
        case 0x7d: printf("MOV    A,L"); break;
        case 0x7e: printf("MOV    A,M"); break;
        case 0x7f: printf("MOV    A,A"); break;

        case 0x80: printf("ADD    B"); break;
        case 0x81: printf("ADD    C"); break;
        case 0x82: printf("ADD    D"); break;
        case 0x83: printf("ADD    E"); break;
        case 0x84: printf("ADD    H"); break;
        case 0x85: printf("ADD    L"); break;
        case 0x86: printf("ADD    M"); break;
        case 0x87: printf("ADD    A"); break;
        case 0x88: printf("ADC    B"); break;
        case 0x89: printf("ADC    C"); break;
        case 0x8a: printf("ADC    D"); break;
        case 0x8b: printf("ADC    E"); break;
        case 0x8c: printf("ADC    H"); break;
        case 0x8d: printf("ADC    L"); break;
        case 0x8e: printf("ADC    M"); break;
        case 0x8f: printf("ADC    A"); break;

        case 0x90: printf("SUB    B"); break;
        case 0x91: printf("SUB    C"); break;
        case 0x92: printf("SUB    D"); break;
        case 0x93: printf("SUB    E"); break;
        case 0x94: printf("SUB    H"); break;
        case 0x95: printf("SUB    L"); break;
        case 0x96: printf("SUB    M"); break;
        case 0x97: printf("SUB    A"); break;
        case 0x98: printf("SBB    B"); break;
        case 0x99: printf("SBB    C"); break;
        case 0x9a: printf("SBB    D"); break;
        case 0x9b: printf("SBB    E"); break;
        case 0x9c: printf("SBB    H"); break;
        case 0x9d: printf("SBB    L"); break;
        case 0x9e: printf("SBB    M"); break;
        case 0x9f: printf("SBB    A"); break;

        case 0xa0: printf("ANA    B"); break;
        case 0xa1: printf("ANA    C"); break;
        case 0xa2: printf("ANA    D"); break;
        case 0xa3: printf("ANA    E"); break;
        case 0xa4: printf("ANA    H"); break;
        case 0xa5: printf("ANA    L"); break;
        case 0xa6: printf("ANA    M"); break;
        case 0xa7: printf("ANA    A"); break;
        case 0xa8: printf("XRA    B"); break;
        case 0xa9: printf("XRA    C"); break;
        case 0xaa: printf("XRA    D"); break;
        case 0xab: printf("XRA    E"); break;
        case 0xac: printf("XRA    H"); break;
        case 0xad: printf("XRA    L"); break;
        case 0xae: printf("XRA    M"); break;
        case 0xaf: printf("XRA    A"); break;

        case 0xb0: printf("ORA    B"); break;
        case 0xb1: printf("ORA    C"); break;
        case 0xb2: printf("ORA    D"); break;
        case 0xb3: printf("ORA    E"); break;
        case 0xb4: printf("ORA    H"); break;
        case 0xb5: printf("ORA    L"); break;
        case 0xb6: printf("ORA    M"); break;
        case 0xb7: printf("ORA    A"); break;
        case 0xb8: printf("CMP    B"); break;
        case 0xb9: printf("CMP    C"); break;
        case 0xba: printf("CMP    D"); break;
        case 0xbb: printf("CMP    E"); break;
        case 0xbc: printf("CMP    H"); break;
        case 0xbd: printf("CMP    L"); break;
        case 0xbe: printf("CMP    M"); break;
        case 0xbf: printf("CMP    A"); break;

        case 0xc0: printf("RNZ"); break;
        case 0xc1: printf("POP    B"); break;
        case 0xc2: printf("JNZ    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xc3: printf("JMP    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xc4: printf("CNZ    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xc5: printf("PUSH   B"); break;
        case 0xc6: printf("ADI    #$%02x",code[1]); opbytes = 2; break;
        case 0xc7: printf("RST    0"); break;
        case 0xc8: printf("RZ"); break;
        case 0xc9: printf("RET"); break;
        case 0xca: printf("JZ     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xcb: printf("JMP    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xcc: printf("CZ     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xcd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xce: printf("ACI    #$%02x",code[1]); opbytes = 2; break;
        case 0xcf: printf("RST    1"); break;

        case 0xd0: printf("RNC"); break;
        case 0xd1: printf("POP    D"); break;
        case 0xd2: printf("JNC    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xd3: printf("OUT    #$%02x",code[1]); opbytes = 2; break;
        case 0xd4: printf("CNC    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xd5: printf("PUSH   D"); break;
        case 0xd6: printf("SUI    #$%02x",code[1]); opbytes = 2; break;
        case 0xd7: printf("RST    2"); break;
        case 0xd8: printf("RC");  break;
        case 0xd9: printf("RET"); break;
        case 0xda: printf("JC     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xdb: printf("IN     #$%02x",code[1]); opbytes = 2; break;
        case 0xdc: printf("CC     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xdd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xde: printf("SBI    #$%02x",code[1]); opbytes = 2; break;
        case 0xdf: printf("RST    3"); break;

        case 0xe0: printf("RPO"); break;
        case 0xe1: printf("POP    H"); break;
        case 0xe2: printf("JPO    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xe3: printf("XTHL");break;
        case 0xe4: printf("CPO    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xe5: printf("PUSH   H"); break;
        case 0xe6: printf("ANI    #$%02x",code[1]); opbytes = 2; break;
        case 0xe7: printf("RST    4"); break;
        case 0xe8: printf("RPE"); break;
        case 0xe9: printf("PCHL");break;
        case 0xea: printf("JPE    $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xeb: printf("XCHG"); break;
        case 0xec: printf("CPE     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xed: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xee: printf("XRI    #$%02x",code[1]); opbytes = 2; break;
        case 0xef: printf("RST    5"); break;

        case 0xf0: printf("RP");  break;
        case 0xf1: printf("POP    PSW"); break;
        case 0xf2: printf("JP     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xf3: printf("DI");  break;
        case 0xf4: printf("CP     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xf5: printf("PUSH   PSW"); break;
        case 0xf6: printf("ORI    #$%02x",code[1]); opbytes = 2; break;
        case 0xf7: printf("RST    6"); break;
        case 0xf8: printf("RM");  break;
        case 0xf9: printf("SPHL");break;
        case 0xfa: printf("JM     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xfb: printf("EI");  break;
        case 0xfc: printf("CM     $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xfd: printf("CALL   $%02x%02x",code[2],code[1]); opbytes = 3; break;
        case 0xfe: printf("CPI    #$%02x",code[1]); opbytes = 2; break;
        case 0xff: printf("RST    7"); break;
    }
	printf("\n"); 
    return opbytes;
}


void NotImplementedInstruction(State8080* state){
    printf("Error: Not Implemented Instruction\n");
    exit(1);
}

void DumpProcessorState(State8080* state){
	printf("------------------------\n");
	printf("Flags:\n");
	printf("\tC (carry)=%d,P (parity)=%d,S (sign)=%d,Z (zero)=%d\n", state->flags.cy, state->flags.p,    
	   state->flags.s, state->flags.z);    
	printf("Registers:\n");
	printf("\t\tAF \t\tBC \t\tDE \t\tHL \t\tPC (program counter) \tSP (stack pointer)\n");
	printf("\t\t%02x \t\t%02x%02x \t\t%02x%02x \t\t%02x%02x \t\t%02x \t\t\t%02x\n",
           state->a,
           state->b, state->c,
           state->d, state->e,
           state->h, state->l,
           state->pc, state->sp
    );
}

int Emulate8080Operation(State8080* state){
    unsigned char *opcode = &state->memory[state->pc];
	Disassemble8080Opcodes(state->memory, state->pc);
	state->pc+=1;
    switch(*opcode){
		case 0x00: break; //NOP
		case 0x01: //LXI    B,word
                   state->c = opcode[1];
                   state->b = opcode[2];
                   state->pc += 2;
                   break;
		case 0x02: NotImplementedInstruction(state); break;
		case 0x03: NotImplementedInstruction(state); break;
		case 0x04: NotImplementedInstruction(state); break;
		case 0x05: {
                uint8_t res = state->b - 1;
                state->flags.z = (res == 0);
                state->flags.s = (0x80 == (res & 0x80));
                state->flags.p = Parity(res, 8);
                state->b = res;
                break;
        }
		case 0x06: { //MVI	B,word
				state->b = opcode[1];
				state->pc += 1;
				break;
        }
		case 0x07: NotImplementedInstruction(state); break;
		case 0x08: NotImplementedInstruction(state); break;
		case 0x09: {
			uint32_t hl = (state->h << 8) | state->l;
			uint32_t bc = (state->b << 8) | state->c;
			uint32_t res = hl + bc;
			state->h = (res & 0xff00) >> 8;
			state->l = res & 0xff;
			state->flags.cy = ((res & 0xffff0000) > 0);
			break;
        }
		case 0x0a: NotImplementedInstruction(state); break;
		case 0x0b: NotImplementedInstruction(state); break;
		case 0x0c: NotImplementedInstruction(state); break;
		case 0x0d: {
			uint8_t res = state->c - 1;
			state->flags.z = (res == 0);
			state->flags.s = (0x80 == (res & 0x80));
			state->flags.p = Parity(res, 8);
			state->c = res;
			break;
        }
		case 0x0e: {
            state->c = opcode[1];
			state->pc++;
			break;
        }
    	case 0x0f: { 
            uint8_t x = state->a;
            state->a = ((x & 1) << 7) | (x >> 1);
            state->flags.cy = (1 == (x&1));
            break;
        }
		case 0x10: NotImplementedInstruction(state); break;
		case 0x11: { //LXI D
            state->e = opcode[1];
            state->d = opcode[2];
            state->pc += 2;
            break;
        }
		case 0x12: NotImplementedInstruction(state); break;
		case 0x13: {
            state->e++;
            if (state->e == 0)
                state->d++;
            break;	
        }
		case 0x14: NotImplementedInstruction(state); break;
		case 0x15: NotImplementedInstruction(state); break;
		case 0x16: NotImplementedInstruction(state); break;
		case 0x17: NotImplementedInstruction(state); break;
		case 0x18: NotImplementedInstruction(state); break;
		case 0x19: {
			uint32_t hl = (state->h << 8) | state->l;
			uint32_t de = (state->d << 8) | state->e;
			uint32_t res = hl + de;
			state->h = (res & 0xff00) >> 8;
			state->l = res & 0xff;
			state->flags.cy = ((res & 0xffff0000) != 0);
			break;
        }
		case 0x1a:{ //LDAX D
            uint16_t offset = (state->d << 8) | state->e;
            state->a = state->memory[offset];
            break;
        }
		case 0x1b: NotImplementedInstruction(state); break;
		case 0x1c: NotImplementedInstruction(state); break;
		case 0x1d: NotImplementedInstruction(state); break;
		case 0x1e: NotImplementedInstruction(state); break;
		case 0x1f: NotImplementedInstruction(state); break;
		case 0x20: NotImplementedInstruction(state); break;
		case 0x21: //LXI H
            state->l = opcode[1];
            state->h = opcode[2];
            state->pc += 2;
            break;
		case 0x22: NotImplementedInstruction(state); break;
		case 0x23: { // INX H
            state->l++;
            if (state->l == 0)
                state->h++;
            break;
        }
		case 0x24: NotImplementedInstruction(state); break;
		case 0x25: NotImplementedInstruction(state); break;
		case 0x26: {
            state->h = opcode[1];
			state->pc++;
			break;
        }
		case 0x27: NotImplementedInstruction(state); break;
		case 0x28: NotImplementedInstruction(state); break;
		case 0x29: {
			uint32_t hl = (state->h << 8) | state->l;
			uint32_t res = hl + hl;
			state->h = (res & 0xff00) >> 8;
			state->l = res & 0xff;
			state->flags.cy = ((res & 0xffff0000) != 0);
			break;
        }
		case 0x2a: NotImplementedInstruction(state); break;
		case 0x2b: NotImplementedInstruction(state); break;
		case 0x2c: NotImplementedInstruction(state); break;
		case 0x2d: NotImplementedInstruction(state); break;
		case 0x2e: NotImplementedInstruction(state); break;
		case 0x2f: // CMA (not)
			state->a = ~state->a;
			break;
		case 0x30: NotImplementedInstruction(state); break;
		case 0x31: // LXI SP,word
			state->sp = (opcode[2] << 8 | opcode[1]);
			state->pc += 2;
			break;
		case 0x32: {
            uint16_t offset = (opcode[2]<<8) | (opcode[1]);
			state->memory[offset] = state->a;
			state->pc += 2;
            break;
        }
		case 0x33: NotImplementedInstruction(state); break;
		case 0x34: NotImplementedInstruction(state); break;
		case 0x35: NotImplementedInstruction(state); break;
		case 0x36: {
            uint16_t offset = (state->h<<8) | state->l;
			state->memory[offset] = opcode[1];
			state->pc++;
            break;
        } 
		case 0x37: NotImplementedInstruction(state); break;
		case 0x38: NotImplementedInstruction(state); break;
		case 0x39: NotImplementedInstruction(state); break;
		case 0x3a: {
            uint16_t offset = (opcode[2]<<8) | (opcode[1]);
			state->a = state->memory[offset];
			state->pc+=2;
            break;
        }
		case 0x3b: NotImplementedInstruction(state); break;
		case 0x3c: NotImplementedInstruction(state); break;
		case 0x3d: NotImplementedInstruction(state); break;
		case 0x3e: {
            state->a = opcode[1];
			state->pc++;
            break;
        }
		case 0x3f: NotImplementedInstruction(state); break;
		case 0x40: NotImplementedInstruction(state); break;
		case 0x41: 
                   state->b = state->c; //MOV B,C
                   break;
		case 0x42: 
                   state->b = state->d; //MOV B,D
                   break;
		case 0x43: 
                   state->b = state->e; //MOV B,E
                   break;
		case 0x44: NotImplementedInstruction(state); break;
		case 0x45: NotImplementedInstruction(state); break;
		case 0x46: NotImplementedInstruction(state); break;
		case 0x47: NotImplementedInstruction(state); break;
		case 0x48: NotImplementedInstruction(state); break;
		case 0x49: NotImplementedInstruction(state); break;
		case 0x4a: NotImplementedInstruction(state); break;
		case 0x4b: NotImplementedInstruction(state); break;
		case 0x4c: NotImplementedInstruction(state); break;
		case 0x4d: NotImplementedInstruction(state); break;
		case 0x4e: NotImplementedInstruction(state); break;
		case 0x4f: NotImplementedInstruction(state); break;
		case 0x50: NotImplementedInstruction(state); break;
		case 0x51: NotImplementedInstruction(state); break;
		case 0x52: NotImplementedInstruction(state); break;
		case 0x53: NotImplementedInstruction(state); break;
		case 0x54: NotImplementedInstruction(state); break;
		case 0x55: NotImplementedInstruction(state); break;
		case 0x56: {
            uint16_t offset = (state->h<<8) | (state->l);
			state->d = state->memory[offset];
            break;
        }
		case 0x57: NotImplementedInstruction(state); break;
		case 0x58: NotImplementedInstruction(state); break;
		case 0x59: NotImplementedInstruction(state); break;
		case 0x5a: NotImplementedInstruction(state); break;
		case 0x5b: NotImplementedInstruction(state); break;
		case 0x5c: NotImplementedInstruction(state); break;
		case 0x5d: NotImplementedInstruction(state); break;
		case 0x5e: {
            uint16_t offset = (state->h<<8) | (state->l);
			state->e = state->memory[offset];
            break;
        }
		case 0x5f: NotImplementedInstruction(state); break;
		case 0x60: NotImplementedInstruction(state); break;
		case 0x61: NotImplementedInstruction(state); break;
		case 0x62: NotImplementedInstruction(state); break;
		case 0x63: NotImplementedInstruction(state); break;
		case 0x64: NotImplementedInstruction(state); break;
		case 0x65: NotImplementedInstruction(state); break;
		case 0x66: {
            uint16_t offset = (state->h<<8) | (state->l);
			state->h = state->memory[offset];
            break;
        }
		case 0x67: NotImplementedInstruction(state); break;
		case 0x68: NotImplementedInstruction(state); break;
		case 0x69: NotImplementedInstruction(state); break;
		case 0x6a: NotImplementedInstruction(state); break;
		case 0x6b: NotImplementedInstruction(state); break;
		case 0x6c: NotImplementedInstruction(state); break;
		case 0x6d: NotImplementedInstruction(state); break;
		case 0x6e: NotImplementedInstruction(state); break;
		case 0x6f: {
            state->l = state->a;
            break;
        }
		case 0x70: NotImplementedInstruction(state); break;
		case 0x71: NotImplementedInstruction(state); break;
		case 0x72: NotImplementedInstruction(state); break;
		case 0x73: NotImplementedInstruction(state); break;
		case 0x74: NotImplementedInstruction(state); break;
		case 0x75: NotImplementedInstruction(state); break;
		case 0x76: NotImplementedInstruction(state); break;
		case 0x77: {
            uint16_t offset = (state->h << 8) | (state->l);
            state->memory[offset] = state->a;
            break;
        }
		case 0x78: NotImplementedInstruction(state); break;
		case 0x79: NotImplementedInstruction(state); break;
		case 0x7a: {
            state->a  = state->d;
            break;
        }
		case 0x7b: {
            state->a  = state->e;
            break;
        }
		case 0x7c: {
            state->a  = state->h;
            break;
        }
		case 0x7d: NotImplementedInstruction(state); break;
		case 0x7e: {
            uint16_t offset = (state->h<<8) | (state->l);
            state->a = state->memory[offset];
            break;
        }
		case 0x7f: NotImplementedInstruction(state); break;
		case 0x80: { //ADD B
				uint16_t answer = (uint16_t) state->a + (uint16_t) state->b;
				state->flags.z = ((answer & 0xff) == 0);
				state->flags.s = ((answer & 0x80) != 0); //MSB (Most significant bit)    
				state->flags.cy = (answer > 0xff);
				//state->flags.p = Parity(answer&0xff);    
				state->a = answer & 0xff;  
				break;  
		}
		case 0x81: { //ADD C
				uint16_t answer = (uint16_t) state->a + (uint16_t) state->c;    
				state->flags.z = ((answer & 0xff) == 0);    
				state->flags.s = ((answer & 0x80) != 0);    
				state->flags.cy = (answer > 0xff);
				//state->flags.p = Parity(answer&0xff);    
				state->a = answer & 0xff;  
				break;  
		}
		case 0x82: NotImplementedInstruction(state); break;
		case 0x83: NotImplementedInstruction(state); break;
		case 0x84: NotImplementedInstruction(state); break;
		case 0x85: NotImplementedInstruction(state); break;
		case 0x86: {
				uint16_t offset = (state->h<<8) | (state->l);    
				uint16_t answer = (uint16_t) state->a + state->memory[offset];    
				state->flags.z = ((answer & 0xff) == 0);    
				state->flags.s = ((answer & 0x80) != 0);    
				state->flags.cy = (answer > 0xff);    
				//state->flags.p = Parity(answer&0xff);    
				state->a = answer & 0xff;  
				break;  
		}
		case 0x87: NotImplementedInstruction(state); break;
		case 0x88: NotImplementedInstruction(state); break;
		case 0x89: NotImplementedInstruction(state); break;
		case 0x8a: NotImplementedInstruction(state); break;
		case 0x8b: NotImplementedInstruction(state); break;
		case 0x8c: NotImplementedInstruction(state); break;
		case 0x8d: NotImplementedInstruction(state); break;
		case 0x8e: NotImplementedInstruction(state); break;
		case 0x8f: NotImplementedInstruction(state); break;
		case 0x90: NotImplementedInstruction(state); break;
		case 0x91: NotImplementedInstruction(state); break;
		case 0x92: NotImplementedInstruction(state); break;
		case 0x93: NotImplementedInstruction(state); break;
		case 0x94: NotImplementedInstruction(state); break;
		case 0x95: NotImplementedInstruction(state); break;
		case 0x96: NotImplementedInstruction(state); break;
		case 0x97: NotImplementedInstruction(state); break;
		case 0x98: NotImplementedInstruction(state); break;
		case 0x99: NotImplementedInstruction(state); break;
		case 0x9a: NotImplementedInstruction(state); break;
		case 0x9b: NotImplementedInstruction(state); break;
		case 0x9c: NotImplementedInstruction(state); break;
		case 0x9d: NotImplementedInstruction(state); break;
		case 0x9e: NotImplementedInstruction(state); break;
		case 0x9f: NotImplementedInstruction(state); break;
		case 0xa0: NotImplementedInstruction(state); break;
		case 0xa1: NotImplementedInstruction(state); break;
		case 0xa2: NotImplementedInstruction(state); break;
		case 0xa3: NotImplementedInstruction(state); break;
		case 0xa4: NotImplementedInstruction(state); break;
		case 0xa5: NotImplementedInstruction(state); break;
		case 0xa6: NotImplementedInstruction(state); break;
		case 0xa7: {
           state->a = state->a & state->a;
           state->flags.cy = state->flags.ac = 0;
           state->flags.z = (state->a == 0);
           state->flags.s = (0x80 == (state->a & 0x80));
           state->flags.p = Parity(state->a, 8);
           break;
        }
		case 0xa8: NotImplementedInstruction(state); break;
		case 0xa9: NotImplementedInstruction(state); break;
		case 0xaa: NotImplementedInstruction(state); break;
		case 0xab: NotImplementedInstruction(state); break;
		case 0xac: NotImplementedInstruction(state); break;
		case 0xad: NotImplementedInstruction(state); break;
		case 0xae: NotImplementedInstruction(state); break;
		case 0xaf: {
           state->a = state->a ^ state->a;
           state->flags.cy = state->flags.ac = 0;
           state->flags.z = (state->a == 0);
           state->flags.s = (0x80 == (state->a & 0x80));
           state->flags.p = Parity(state->a, 8);
           break;
        }
		case 0xb0: NotImplementedInstruction(state); break;
		case 0xb1: NotImplementedInstruction(state); break;
		case 0xb2: NotImplementedInstruction(state); break;
		case 0xb3: NotImplementedInstruction(state); break;
		case 0xb4: NotImplementedInstruction(state); break;
		case 0xb5: NotImplementedInstruction(state); break;
		case 0xb6: NotImplementedInstruction(state); break;
		case 0xb7: NotImplementedInstruction(state); break;
		case 0xb8: NotImplementedInstruction(state); break;
		case 0xb9: NotImplementedInstruction(state); break;
		case 0xba: NotImplementedInstruction(state); break;
		case 0xbb: NotImplementedInstruction(state); break;
		case 0xbc: NotImplementedInstruction(state); break;
		case 0xbd: NotImplementedInstruction(state); break;
		case 0xbe: NotImplementedInstruction(state); break;
		case 0xbf: NotImplementedInstruction(state); break;
		case 0xc0: NotImplementedInstruction(state); break;
		case 0xc1: {
            state->c = state->memory[state->sp];
            state->b = state->memory[state->sp+1];
            state->sp += 2;
			break;
        }
		case 0xc2: // JNZ
			if(state->flags.z == 0)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
                state->pc += 2;	
			break;
		case 0xc3: // JMP
			state->pc = (opcode[2] << 8) | opcode[1];
			break;
		case 0xc4: NotImplementedInstruction(state); break;
		case 0xc5: {
			state->memory[state->sp-1] = state->b;
			state->memory[state->sp-2] = state->c;
			state->sp = state->sp - 2;
			break;
        }
		case 0xc6: {
			uint16_t answer = (uint16_t) state->a + (uint16_t) opcode[1];    
            state->flags.z = ((answer & 0xff) == 0);    
            state->flags.s = ((answer & 0x80) != 0);    
            state->flags.cy = (answer > 0xff);    
            state->flags.p = Parity((answer&0xff), 8);
            state->a = (uint8_t)answer;   
            state->pc += 1;
            break;  
		}	
		case 0xc7: NotImplementedInstruction(state); break;
		case 0xc8: NotImplementedInstruction(state); break;
		case 0xc9: { // RET
			state->pc = state->memory[state->sp] | (state->memory[state->sp+1] << 8);    
            state->sp += 2;    
            break;  
		}
		case 0xca: NotImplementedInstruction(state); break;
		case 0xcb: NotImplementedInstruction(state); break;
		case 0xcc: NotImplementedInstruction(state); break;
		case 0xcd: { // CALL address
			uint16_t    ret = state->pc+2;    
            state->memory[state->sp-1] = (ret >> 8) & 0xff;    
            state->memory[state->sp-2] = (ret & 0xff);    
            state->sp = state->sp - 2;    
            state->pc = (opcode[2] << 8) | opcode[1];
			break;
		}
		case 0xce: NotImplementedInstruction(state); break;
		case 0xcf: NotImplementedInstruction(state); break;
		case 0xd0: NotImplementedInstruction(state); break;
		case 0xd1: {
            state->e = state->memory[state->sp];
            state->d = state->memory[state->sp+1];
            state->sp += 2;
            break;
        }
		case 0xd2: NotImplementedInstruction(state); break;
		case 0xd3: {
			state->pc++;
			break;
        }
		case 0xd4: NotImplementedInstruction(state); break;
		case 0xd5: {
			state->memory[state->sp-1] = state->d;
			state->memory[state->sp-2] = state->e;
			state->sp = state->sp - 2;
            break;
        }
		case 0xd6: NotImplementedInstruction(state); break;
		case 0xd7: NotImplementedInstruction(state); break;
		case 0xd8: NotImplementedInstruction(state); break;
		case 0xd9: NotImplementedInstruction(state); break;
		case 0xda: NotImplementedInstruction(state); break;
		case 0xdb: NotImplementedInstruction(state); break;
		case 0xdc: NotImplementedInstruction(state); break;
		case 0xdd: NotImplementedInstruction(state); break;
		case 0xde: NotImplementedInstruction(state); break;
		case 0xdf: NotImplementedInstruction(state); break;
		case 0xe0: NotImplementedInstruction(state); break;
		case 0xe1: {
            state->l = state->memory[state->sp];
            state->h = state->memory[state->sp+1];
            state->sp += 2;
			break;
        }
		case 0xe2: NotImplementedInstruction(state); break;
		case 0xe3: NotImplementedInstruction(state); break;
		case 0xe4: NotImplementedInstruction(state); break;
		case 0xe5: {
			state->memory[state->sp-1] = state->h;
			state->memory[state->sp-2] = state->l;
			state->sp = state->sp - 2;
			break;
        }
		case 0xe6: { //ANI byte
            state->a = state->a & opcode[1];
            state->flags.cy = state->flags.ac = 0;
            state->flags.z = (state->a == 0);
            state->flags.s = (0x80 == (state->a & 0x80));
            state->flags.p = Parity(state->a, 8);
			state->pc++;
			break;
		}
		case 0xe7: NotImplementedInstruction(state); break;
		case 0xe8: NotImplementedInstruction(state); break;
		case 0xe9: NotImplementedInstruction(state); break;
		case 0xea: NotImplementedInstruction(state); break;
		case 0xeb: {
            uint8_t save1 = state->d;
            uint8_t save2 = state->e;
            state->d = state->h;
            state->e = state->l;
            state->h = save1;
            state->l = save2;
			break;
        }
		case 0xec: NotImplementedInstruction(state); break;
		case 0xed: NotImplementedInstruction(state); break;
		case 0xee: NotImplementedInstruction(state); break;
		case 0xef: NotImplementedInstruction(state); break;
		case 0xf0: NotImplementedInstruction(state); break;
		case 0xf1: {
            state->a = state->memory[state->sp+1];
            uint8_t psw = state->memory[state->sp];
            state->flags.z  = (0x01 == (psw & 0x01));
            state->flags.s  = (0x02 == (psw & 0x02));
            state->flags.p  = (0x04 == (psw & 0x04));
            state->flags.cy = (0x05 == (psw & 0x08));
            state->flags.ac = (0x10 == (psw & 0x10));
            state->sp += 2;
			break;
        }
		case 0xf2: NotImplementedInstruction(state); break;
		case 0xf3: NotImplementedInstruction(state); break;
		case 0xf4: NotImplementedInstruction(state); break;
		case 0xf5: {
			state->memory[state->sp-1] = state->a;
			uint8_t psw = (state->flags.z |
						state->flags.s << 1 |
						state->flags.p << 2 |
						state->flags.cy << 3 |
						state->flags.ac << 4 );
			state->memory[state->sp-2] = psw;
			state->sp = state->sp - 2;
			break;
        }
		case 0xf6: NotImplementedInstruction(state); break;
		case 0xf7: NotImplementedInstruction(state); break;
		case 0xf8: NotImplementedInstruction(state); break;
		case 0xf9: NotImplementedInstruction(state); break;
		case 0xfa: NotImplementedInstruction(state); break;
		case 0xfb: {
           state->int_enable = 1;
           break;
        }
		case 0xfc: NotImplementedInstruction(state); break;
		case 0xfd: NotImplementedInstruction(state); break;
		case 0xfe: {
			uint8_t x = state->a - opcode[1];
			state->flags.z = (x == 0);
			state->flags.s = (0x80 == (x & 0x80));
			state->flags.p = Parity(x, 8);
			state->flags.cy = (state->a < opcode[1]);
			state->pc++;
			break;
        }
		case 0xff: NotImplementedInstruction(state); break;
	}
	DumpProcessorState(state);
	return 0;
}

void LoadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset){
    FILE *romFile = fopen(filename, "rb");
    if (romFile == NULL){
        printf("error: Couldn't open %s\n", filename);
        exit(1);
    }
    fseek(romFile, 0L, SEEK_END);
    int fsize = ftell(romFile);
    fseek(romFile, 0L, SEEK_SET);

    unsigned char *buffer = &state->memory[offset];
    fread(buffer, fsize, 1, romFile);
    fclose(romFile); 
}

State8080* InitProcessor(){
	State8080* state = new State8080();
	state->memory = (uint8_t *)malloc(0x10000);
	return state;
}

int InitialiseWindow(){
    if (SDL_Init(SDL_INIT_VIDEO) != 0){
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow(SCREEN_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr){
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr){
        SDL_DestroyWindow(window);
        std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Event e;
    bool quit = false;
    while (!quit){
        while (SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT){
                quit = true;
            }
        }
        Emulate8080Operation(state);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void InitialiseState() {
	state = InitProcessor();

	LoadFileIntoMemoryAt(state, "invaders.h", 0);
	LoadFileIntoMemoryAt(state, "invaders.g", 0x800);
	LoadFileIntoMemoryAt(state, "invaders.f", 0x1000);
	LoadFileIntoMemoryAt(state, "invaders.e", 0x1800);
}

int main(int argc, char **argv){
    InitialiseState();
    return InitialiseWindow();
}
