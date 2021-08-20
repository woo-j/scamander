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

typedef struct StatusRegister {
    bool    f0;     // User flag 0
    bool    f1;     // User flag 1
    bool    f2;     // User flag 2
    bool    ie;     // Interrupt Enable Flag
    bool    sa;     // Sense Bit A
    bool    sb;     // Sense Bit B
    bool    ov;     // Overflow
    bool    cy;     // Carry/Link
} StatusRegister;

typedef struct StateSCMP {
    unsigned char   ac;    // Accumulator
    unsigned char   e;      // Extension register
    int     pc;             // Programme Counter
    int     ptr1;           // Pointer register
    int     ptr2;
    int     ptr3;
    struct StatusRegister sr;
    bool    h;              // Halt flag
    bool    d;              // Delay flag
    bool    sin;            // Serial In
    bool    sout;           // Serial Out
    
    unsigned char display[8];   // Display on HEX I/O board
    unsigned char hexkey;       // Keyboard Input
    
    unsigned char     *memory;  // Includes RAM and ROM
} StateSCMP;

void set_memory(StateSCMP *state, int address, unsigned char data);
unsigned char get_memory(StateSCMP* state, int address);
int EmulateSCMPOp(StateSCMP* state);
void reset_cpu(StateSCMP* state);
