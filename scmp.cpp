/*
    Scamander - An Elektor (Elektuur) SC/MP emulator
    Copyright (C) 2021 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/

#include "scmp.hpp"
#include <iostream>
#include <string>

void set_memory(StateSCMP *state, int address, unsigned char data) {
    // Set the contents of a memory address
    
    if ((address < 0x00) || (address > 0xffff)) {
        std::cout << "Error writing to memory location " << address << "\n";
        address = address % 0xffff;
    }
    
    if ((address >= 0x800) && (address <= 0xfff)) {
        // RAM
        state->memory[address] = data;
    }
    
    if ((address >= 0x700) && (address <= 0x707)) {
        // Display
        state->display[address & 0x0f] = data;
    }
}

unsigned char get_memory(StateSCMP *state, int address) {
    // Retrieve the contents of a memory address
    
    unsigned char retval = 0xff;
    
    if ((address < 0x00) || (address > 0xffff)) {
        std::cout << "Error reading from memory location " <<  address << "\n";
        address = address % 0xffff;
    }
    
    if ((address >= 0x00) && (address <= 0x5ff)) {
        // ROM
        retval = state->memory[address];
    }
    
    if ((address >= 0x708) && (address <= 0x70f)) {
        // Keyboard
        retval = state->hexkey;
    }
    
    if ((address >= 0xc00) && (address <= 0xfff)) {
        // RAM
        retval = state->memory[address];
    }
    
    return retval;
}

void reset_cpu(StateSCMP* state) {
    // Reset all internal registers
    state->sr.f0 = false;
    state->sr.f1 = false;
    state->sr.f2 = false;
    state->sr.ie = false;
    state->sr.sa = false;
    state->sr.sb = false;
    state->sr.ov = false;
    state->sr.cy = false;
    state->ac = 0x00;
    state->e = 0x00;
    state->pc = 0x00;
    state->ptr1 = 0x00;
    state->ptr2 = 0x00;
    state->ptr3 = 0x00;
    state->h = false;
    state->sin = false;
    state->sout = false;
}

int dispadd(int address, int disp) {
    // Wraps around a 4k page
    return (address & 0xf000) + ((address + (int) disp) & 0x0fff);
}

void dad(StateSCMP* state, unsigned char num) {
    // Decimal Add
    unsigned char ah, al, nh, nl;
    ah = state->ac >> 4;
    al = state->ac & 0x0f;
    nh = num >> 4;
    nl = num & 0x0f;
    if (state->sr.cy) al++;
    al += nl;
    if (al > 9) {
        ah++;
        al -= 10;
    }
    ah += nh;
    if (ah > 9) {
        state->sr.cy = true;
        ah -= 10;
    } else {
        state->sr.cy = false;
    }
    state->ac = ((ah & 0x0f) << 4) + (al & 0x0f);
}

int sign(unsigned char arg) {
    // Convert unsigned char to (signed) int for "disp"
    int i;
    int answer = 0;
    
    if (arg > 0x7f) {
        // Negative number 
        for (i = 0; i < 7; i++) {
            if (!(arg & (0x40 >> i))) answer -= 0x40 >> i;
        }
        answer--;
    } else {
        // Positive number
        for (i = 0; i < 7; i++) {
            if (arg & (0x40 >> i)) answer += 0x40 >> i;
        }
    }
    
    return answer;
}

void printState(StateSCMP* state, int disp, std::string disc, int ea) {
    // Output the status of the CPU
    std::cout << disc;
    printf("  %4x  ", ea);
    printf("AC %02x, P1 %04x, P2 %04x, P3 %04x, E %02x SR ", state->ac, state->ptr1, state->ptr2, state->ptr3, state->e);
    // Yes, I could use ternary operators - but I just don't like them!
    if (state->sr.f0) printf("0"); else printf(" ");
    if (state->sr.f1) printf("1"); else printf(" ");
    if (state->sr.f2) printf("2"); else printf(" ");
    if (state->sr.ie) printf("I"); else printf(" ");
    if (state->sr.sa) printf("A"); else printf(" ");
    if (state->sr.sb) printf("B"); else printf(" ");
    if (state->sr.ov) printf("O"); else printf(" ");
    if (state->sr.cy) printf("C"); else printf(" ");
    if (state->h) printf(" HLT"); else printf("    ");
    if (state->sout) printf(" SOUT"); else printf("     ");
    printf("\n");
}

int EmulateSCMPOp(StateSCMP* state)
{
    // Perform Fetch-Execute
    
    int cycles; // Microcycles required
    int ea = 0; // Effective Address
    unsigned char temp;
    int answer, store;
    bool t;
    int disp = 0;
    std::string desc;
    
    state->pc = dispadd(state->pc, 1); // Increment PC
    unsigned char *opcode = &state->memory[state->pc];
    
    //printf("PC %04x,  OP %02x", state->pc, *opcode);
    
    if (*opcode > 0x7f) {
        // Two byte instruction
        state->pc = dispadd(state->pc, 1); // Increment PC again
        disp = sign(state->memory[state->pc]);
        if (disp == -128) {
            // "If disp = -128, then (E) is substituted for disp..."
            disp = state->e;
        }
        //printf(" %d, ", disp);
    } else {
        //printf(",    ");
    }
    
    cycles = 7; // "Undefined opcodes ... may take 5 to 10 microcycles to execute depending on the code."
    
    if (*opcode < 0xC0) {
        switch(*opcode)
        {
            // Single-Byte Instruction
            case 0x00: // HLT - Halt
                state->h = true;
                cycles = 8;
                desc = "HLT";
                break;
            case 0x01: // XAE - Exchange AC and Extension
                temp = state->ac;
                state->ac = state->e;
                state->e = temp;
                cycles = 7;
                desc = "XAE";
                break;
            case 0x02: // CCL - Clear Carry/Link
                state->sr.cy = false;
                cycles = 5;
                desc = "CCL";
                break;
            case 0x03: // SCL - Set Carry/Link
                state->sr.cy = true;
                cycles = 5;
                desc = "SCL";
                break;
            case 0x04: // DINT - Disable Interrupt
                state->sr.ie = false;
                cycles = 6;
                desc = "DINT";
                break;
            case 0x05: // IEN - Enable Interrupt
                state->sr.ie = true;
                cycles = 6;
                desc = "IEN";
                break;
            case 0x06: // CSA - Copy Status to AC
                state->ac = 0x00;
                if (state->sr.f0) state->ac += 0x01;
                if (state->sr.f1) state->ac += 0x02;
                if (state->sr.f2) state->ac += 0x04;
                if (state->sr.ie) state->ac += 0x08;
                if (state->sr.sa) state->ac += 0x10;
                if (state->sr.sb) state->ac += 0x20;
                if (state->sr.ov) state->ac += 0x40;
                if (state->sr.cy) state->ac += 0x80;
                cycles = 5;
                desc = "CSA";
                break;
            case 0x07: // CAS - Copy AC to Status
                state->sr.f0 = state->ac & 0x01;
                state->sr.f1 = state->ac & 0x02;
                state->sr.f2 = state->ac & 0x04;
                state->sr.ie = state->ac & 0x08;
                // state->sr.sa is read-only
                // state->sr.sb is read-only
                state->sr.ov = state->ac & 0x40;
                state->sr.cy = state->ac & 0x80;
                cycles = 6;
                desc = "CAS";
                break;
            case 0x08: // NOP - No Operation
                cycles = 5;
                desc = "NOP";
                break;
            case 0x19: // SIO - Serial Input/Output
                state->sout = state->e & 0x01;
                state->e = state->e >> 1;
                if (state->sin) state->e += 0x80;
                cycles = 5;
                desc = "SIO";
                break;
            case 0x1c: // SR - Shift Right
                state->ac = state->ac >> 1;
                cycles = 5;
                desc = "SR";
                break;
            case 0x1d: // SRL - Shift Right with Link
                state->ac = state->ac >> 1;
                if (state->sr.cy) state->ac += 0x80;
                cycles = 5;
                desc = "SRL";
                break;
            case 0x1e: // RR - Rotate Right
                t = state->ac & 0x01;
                state->ac = state->ac >> 1;
                if (t) state->ac += 0x80;
                cycles = 5;
                desc = "RR";
                break;
            case 0x1f: // RRL - Rotate Right with Link
                t = state->sr.cy;
                state->sr.cy = state->ac & 0x01;
                state->ac = state->ac >> 1;
                if (t) state->ac += 0x80;
                cycles = 5;
                desc = "RRL";
                break;
            case 0x30: // XPAL ptr0 - Exchange Pointer Low
                temp = state->ac;
                state->ac = state->pc & 0xff;
                state->pc = (state->pc & 0xff00) + temp;
                cycles = 8;
                desc = "XPAL pc";
                break;
            case 0x31: // XPAL ptr1 - Exchange Pointer Low
                temp = state->ac;
                state->ac = state->ptr1 & 0xff;
                state->ptr1 = (state->ptr1 & 0xff00) + temp;
                cycles = 8;
                desc = "XPAL p1";
                break;
            case 0x32: // XPAL ptr2 - Exchange Pointer Low
                temp = state->ac;
                state->ac = state->ptr2 & 0xff;
                state->ptr2 = (state->ptr2 & 0xff00) + temp;
                cycles = 8;
                desc = "XPAL p2";
                break;
            case 0x33: // XPAL ptr3 - Exchange Pointer Low
                temp = state->ac;
                state->ac = state->ptr3 & 0xff;
                state->ptr3 = (state->ptr3 & 0xff00) + temp;
                cycles = 8;
                desc = "XPAL p3";
                break;
            case 0x34: // XPAH ptr0 - Exchange Pointer High
                temp = state->ac;
                state->ac = state->pc >> 8;
                state->pc = temp << 8 | (state->pc & 0xff);
                cycles = 8;
                desc = "XPAH pc";
                break;
            case 0x35: // XPAH ptr1 - Exchange Pointer High
                temp = state->ac;
                state->ac = state->ptr1 >> 8;
                state->ptr1 = temp << 8 | (state->ptr1 & 0xff);
                cycles = 8;
                desc = "XPAH p1";
                break;
            case 0x36: // XPAH ptr2 - Exchange Pointer High
                temp = state->ac;
                state->ac = state->ptr2 >> 8;
                state->ptr2 = temp << 8 | (state->ptr2 & 0xff);
                cycles = 8;
                desc = "XPAH p2";
                break;
            case 0x37: // XPAH ptr3 - Exchange Pointer High
                temp = state->ac;
                state->ac = state->ptr3 >> 8;
                state->ptr3 = temp << 8 | (state->ptr3 & 0xff);
                cycles = 8;
                desc = "XPAH p3";
                break;
            case 0x3c: // XPPC ptr0 - Exchange Pointer with PC
                // NOP
                cycles = 7;
                desc = "XPPC pc (nop)";
                break;
            case 0x3d: // XPPC ptr1 - Exchange Pointer with PC
                store = state->pc;
                state->pc = state->ptr1;
                state->ptr1 = store;
                cycles = 7;
                desc = "XPPC p1";
                break;
            case 0x3e: // XPPC ptr2 - Exchange Pointer with PC
                store = state->pc;
                state->pc = state->ptr2;
                state->ptr2 = store;
                cycles = 7;
                desc = "XPPC p2";
                break;
            case 0x3f: // XPPC ptr3 - Exchange Pointer with PC
                store = state->pc;
                state->pc = state->ptr3;
                state->ptr3 = store;
                cycles = 7;
                desc = "XPPC p3";
                break;
            case 0x40: // LDE - Load AC from Extension
                state->ac = state->e;
                cycles = 6;
                desc = "LDE";
                break;
            case 0x50: // ANE - AND Extension
                state->ac &= state->e;
                cycles = 6;
                desc = "ANE";
                break;
            case 0x58: // ORE - OR Extension
                state->ac |= state->e;
                cycles = 6;
                desc = "ORE";
                break;
            case 0x60: // XRE - Exclusive-OR Extension
                state->ac ^= state->e;
                cycles = 6;
                desc = "XRE";
                break;
            case 0x68: // DAE - Decimal Add Extension
                dad(state, state->e);
                cycles = 11;
                desc = "DAE";
                break;
            case 0x70: // ADE - Add Extension
                answer = sign(state->ac) + sign(state->e) + (int) state->sr.cy;
                state->sr.cy = answer > 0xff;
                state->sr.ov = (answer >> 7 == 1 | answer >> 7 == 2);
                state->ac = answer & 0xff;
                cycles = 7;
                desc = "ADE";
                break;
            case 0x78: // CAE - Complement and Add Extension
                answer = sign(state->ac) + sign(state->e ^ 0xff) + (int) state->sr.cy;
                state->sr.cy = answer > 0xff;
                state->sr.ov = (answer >> 7 == 1 | answer >> 7 == 2);
                state->ac = answer & 0xff;
                cycles = 8;
                desc = "CAE";
                break;
            // Double-Byte Instructions
            case 0x8f: // DLY - Delay
                cycles = 13 + (2 * (int) state->ac) + (2 * (int) state->memory[state->pc]) + (512 * (int) state->memory[state->pc]);
                //printf("Delay %d\n", cycles);
                desc = "DLY";
                break;
            case 0x90: // JMP - Jump
                ea = dispadd(state->pc, disp);
                state->pc = ea;
                cycles = 11;
                desc = "JMP pc";
                break;
            case 0x91: // JMP - Jump
                ea = dispadd(state->ptr1, disp);
                state->pc = ea;
                cycles = 11;
                desc = "JMP p1";
                break;
            case 0x92: // JMP - Jump
                ea = dispadd(state->ptr2, disp);
                state->pc = ea;
                cycles = 11;
                desc = "JMP p2";
                break;
            case 0x93: // JMP - Jump
                ea = dispadd(state->ptr3, disp);
                state->pc = ea;
                cycles = 11;
                desc = "JMP p3";
                break;
            case 0x94: // JP - Jump If Positive
                ea = dispadd(state->pc, disp);
                if (sign(state->ac) >= 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JP pc";
                break;
            case 0x95: // JP - Jump If Positive
                ea = dispadd(state->ptr1, disp);
                if (sign(state->ac) >= 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JP p1";
                break;
            case 0x96: // JP - Jump If Positive
                ea = dispadd(state->ptr2, disp);
                if (sign(state->ac) >= 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JP p2";
                break;
            case 0x97: // JP - Jump If Positive
                ea = dispadd(state->ptr3, disp);
                if (sign(state->ac) >= 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JP p3";
                break;
            case 0x98: // JZ - Jump If Zero
                ea = dispadd(state->pc, disp);
                if (sign(state->ac) == 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JZ pc";
                break;
            case 0x99: // JZ - Jump If Zero
                ea = dispadd(state->ptr1, disp);
                if (sign(state->ac) == 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JZ p1";
                break;
            case 0x9a: // JZ - Jump If Zero
                ea = dispadd(state->ptr2, disp);
                if (sign(state->ac) == 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JZ p2";
                break;
            case 0x9b: // JZ - Jump If Zero
                ea = dispadd(state->ptr3, disp);
                if (sign(state->ac) == 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JZ p3";
                break;
            case 0x9c: // JNZ - Jump If Not Zero
                ea = dispadd(state->pc, disp);
                if (sign(state->ac) != 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JNZ pc";
                break;
            case 0x9d: // JNZ - Jump If Not Zero
                ea = dispadd(state->ptr1, disp);
                if (sign(state->ac) != 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JNZ p1";
                break;
            case 0x9e: // JNZ - Jump If Not Zero
                ea = dispadd(state->ptr2, disp);
                if (sign(state->ac) != 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JNZ p2";
                break;
            case 0x9f: // JNZ - Jump If Not Zero
                ea = dispadd(state->ptr3, disp);
                if (sign(state->ac) != 0) {
                    state->pc = ea;
                    cycles = 11;
                } else {
                    cycles = 9;
                }
                desc = "JNZ p3";
                break;
            case 0xa8: // ILD - Increment and Load
                ea = dispadd(state->pc, disp);
                state->ac = get_memory(state, ea) + 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "ILD pc";
                break;
            case 0xa9: // ILD - Increment and Load
                ea = dispadd(state->ptr1, disp);
                state->ac = get_memory(state, ea) + 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "ILD p1";
                break;
            case 0xaa: // ILD - Increment and Load
                ea = dispadd(state->ptr2, disp);
                state->ac = get_memory(state, ea) + 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "ILD p2";
                break;
            case 0xab: // ILD - Increment and Load
                ea = dispadd(state->ptr3, disp);
                state->ac = get_memory(state, ea) + 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "ILD p3";
                break;
            case 0xb8: // DLD - Decrement and Load
                ea = dispadd(state->pc, disp);
                state->ac = get_memory(state, ea) - 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "DLD pc";
                break;
            case 0xb9: // DLD - Decrement and Load
                ea = dispadd(state->ptr1, disp);
                state->ac = get_memory(state, ea) - 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "DLD p1";
                break;
            case 0xba: // DLD - Decrement and Load
                ea = dispadd(state->ptr2, disp);
                state->ac = get_memory(state, ea) - 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "DLD p2";
                break;
            case 0xbb: // DLD - Decrement and Load
                ea = dispadd(state->ptr3, disp);
                state->ac = get_memory(state, ea) - 1;
                set_memory(state, ea, state->ac);
                cycles = 22;
                desc = "DLD p3";
                break;
        }
    } else {          
        // Memory Reference Instructions and Immediate Instructions
        switch (*opcode & 0x07) {
                case 0x00: // PC-relative
                    ea = dispadd(state->pc, disp);
                    break;
                case 0x01: // Indexed
                    ea = dispadd(state->ptr1, disp);
                    break;
                case 0x02: // Indexed
                    ea = dispadd(state->ptr2, disp);
                    break;
                case 0x03: // Indexed
                    ea = dispadd(state->ptr3, disp);
                    break;
                case 0x04: // Immediate
                    ea = state->pc;
                    break;
                case 0x05: // Auto-indexed
                    if (disp < 0) ea = dispadd(state->ptr1, disp); else ea = state->ptr1;
                    state->ptr1 = dispadd(state->ptr1, disp);
                    break;
                case 0x06: // Auto-indexed
                    if (disp < 0) ea = dispadd(state->ptr2, disp); else ea = state->ptr2;
                    state->ptr2 = dispadd(state->ptr2, disp);
                    break;
                case 0x07: // Auto-indexed
                    if (disp < 0) ea = dispadd(state->ptr3, disp); else ea = state->ptr3;
                    state->ptr3 = dispadd(state->ptr3, disp);
                    break;
        }
            
        switch (*opcode & 0xf8) {
                case 0xc0: // LD - Load
                    state->ac = get_memory(state, ea);
                    cycles = 18;
                    desc = "LD";
                    break;
                case 0xc8: // ST - Store
                    set_memory(state, ea, state->ac);
                    cycles = 18;
                    desc = "ST";
                    break;
                case 0xd0: // AND - AND
                    state->ac &= get_memory(state, ea);
                    cycles = 18;
                    desc = "AND";
                    break;
                case 0xd8: // OR - OR
                    state->ac |= get_memory(state, ea);
                    cycles = 18;
                    desc = "OR";
                    break;
                case 0xe0: // XOR - Exclusive-OR
                    state->ac ^= get_memory(state, ea);
                    cycles = 18;
                    desc = "XOR";
                    break;
                case 0xe8: // DAD - Decimal Add
                    dad(state, get_memory(state, ea));
                    cycles = 23;
                    desc = "DAD";
                    break;
                case 0xf0: // ADD - Add
                    answer = sign(state->ac) + sign(get_memory(state, ea)) + (int) state->sr.cy;
                    state->sr.cy = answer > 0xff;
                    state->sr.ov = (answer >> 7 == 1 | answer >> 7 == 2);
                    state->ac = answer & 0xff;
                    cycles = 19;
                    desc = "ADD";
                    break;
                case 0xf8: // CAD - Complement and Add
                    answer = sign(state->ac) + sign(get_memory(state, ea) ^ 0xff) + (int) state->sr.cy;
                    state->sr.cy = answer > 0xff;
                    state->sr.ov = (answer >> 7 == 1 | answer >> 7 == 2);
                    state->ac = answer & 0xff;
                    cycles = 20;
                    desc = "CAD";
                    break;
        }
    }
    
    //printState(state, disp, desc, ea);
    
    return cycles;
}

