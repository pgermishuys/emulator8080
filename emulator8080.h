#ifndef _EMULATOR8080_H_
#define _EMULATOR8080_H_
#include <iostream>

class Emulator8080 {
    private:
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

        void LoadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset) {
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
        void InitializeProcessorState() {
            state = new State8080();
            state->memory = (uint8_t *)malloc(0x10000);
        }

        void NotImplementedInstruction(State8080* state){
            printf("Error: Not Implemented Instruction\n");
            exit(1);
        }

        void DumpProcessorState(State8080* state) {
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

        int Disassemble8080Opcodes(unsigned char *codebuffer, int pc);
        int Emulate8080Operation(State8080* state);

    public:
        void Initialize() {
            InitializeProcessorState();
            LoadFileIntoMemoryAt(state, "invaders.h", 0);
            LoadFileIntoMemoryAt(state, "invaders.g", 0x800);
            LoadFileIntoMemoryAt(state, "invaders.f", 0x1000);
            LoadFileIntoMemoryAt(state, "invaders.e", 0x1800);
        }
        void AdvanceEmulationStep() {
            Emulate8080Operation(state);
        }
};

#endif
