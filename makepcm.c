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

/*
 * This tool is for taking the "audio" output from the Scamander emulator (i.e. the
 * timing between each flip of flag 1 and converting it into a PWM file. This can then
 * be converted into something useful, like a FLAC file, using FFmpeg like so:
 * 
 * ffmpeg -f s8 -ar 44.1k -ac 1 -i ./audio.pcm audio.flac
 * 
 * Data from the emulator is timed according to CPU microcycles so some guesswork is
 * needed to get it to fit with the 44.1kHz sample rate of "CD-quality" audio.
*/

#include <stdio.h>

int main()
{
    FILE *file_pointer;
    FILE *out_file_pointer;
    int letter;
    int raw_pulse_width = 0;
    int pulse_width;
    float modifier = 11.34; // (Approx 500,000 / 44,100)
    float pad = 0.0;
    int i;
    int flag = 0;
    
    file_pointer = fopen("audio_raw.txt", "r");
    out_file_pointer = fopen("audio.pcm", "w");
    letter = getc(file_pointer);
    
    while (letter != EOF) {
        
        if (letter == '\n') {
            pulse_width = (int)((float) raw_pulse_width / modifier);
            pad += (float) pulse_width - ((float) raw_pulse_width / modifier);
            
            if (pad >= 1.0) {
                pad -= 1.0;
                pulse_width -= 1;
            }
            
            if (pad <= -1.0) {
                pad += 1.0;
                pulse_width += 1;
            }
            
            for (i = 0; i < pulse_width; i++) {
                if (flag == 0) {
                    putc(0xf8, out_file_pointer);
                } else {
                    putc(0x08, out_file_pointer);
                }
            }
            raw_pulse_width = 0;
            if (flag == 0) {
                flag = 1;
            } else {
                flag = 0;
            }
        } else {
            raw_pulse_width *= 10;
            raw_pulse_width += (letter - '0');
        }
        
        //putchar(letter);
        letter = getc(file_pointer);
    }
    
    fclose(file_pointer);
    fclose(out_file_pointer);
    
    return 0;
}
