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
 * WARNING:
 * This emulator is intended as something which can easily be messed about with,
 * rather than being a "polished" product for end users. Expect bugs and the need to
 * recompile often... Have fun!
 */

#include <SFML/Graphics.hpp>
#include "scmp.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
using namespace std;

class IOState {
public:
    int address_switches;
    int data_switches;
    unsigned char led_buffer;
    
    void key_press(int key);
};

void IOState::key_press(int key) {
    printf("Key press %d detected\n", key);
}

bool load_rom(const char filename[24], int start_address, int rom_length, unsigned char main_memory[]) {
    ifstream rom;
    rom.open(filename, ios::in | ios::binary);
    if (rom.is_open()) {
        rom.read ((char *) &main_memory[start_address], rom_length);
        rom.close();
    } else {
        std::cout << "Unable to load ROM\n";
        rom.close();
        return false;
    }
    
    return true;
}

int main(int argc, char **argv) {
    unsigned char main_memory[0x1000];
    StateSCMP state;
    IOState io;
    
    int i;
    int time = 0;
    
    int framerate = 25;
    int operations_per_frame;
    bool inFocus = true;
    int running_time = 0;
    
    int scale = 2; // How big the digits are
    
    unsigned char scratch_pad[8];
    
    int led_colour = 0; // Red
    
    bool flag1 = false;
    int last_pulse = 0;
    int pulse_width = 0;
    
    // 1 MHz crystal = 500,000 microcycles per second
    operations_per_frame = 500000 / framerate;
    
    state.memory = main_memory;
    state.hexkey = 0x00;
    
    reset_cpu(&state);
    
    // Initialise window
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8;
    sf::RenderWindow window(sf::VideoMode(8 * 28 * scale, 44 * scale), "Elektor SC/MP", sf::Style::Default, settings);
    window.setFramerateLimit(framerate);
    
    sf::RectangleShape halt_led;
    sf::Color ledoff = sf::Color(20,0,0);
    sf::Color ledon = sf::Color(250,0,0);
    
    // All of these shapes are the segmented display - below calculates the position of each vertex
    sf::ConvexShape sega[8];
    sf::ConvexShape segb[8];
    sf::ConvexShape segc[8];
    sf::ConvexShape segd[8];
    sf::ConvexShape sege[8];
    sf::ConvexShape segf[8];
    sf::ConvexShape segg[8];
    sf::CircleShape segdp[8];
    
    for (i = 0; i < 8; i++) {
        sega[i].setPointCount(4);
        segb[i].setPointCount(4);
        segc[i].setPointCount(4);
        segd[i].setPointCount(4);
        sege[i].setPointCount(4);
        segf[i].setPointCount(4);
        segg[i].setPointCount(6);
        
        sega[i].setPoint(0, sf::Vector2f((8 + (28 * i)) * scale, 0 * scale));
        sega[i].setPoint(1, sf::Vector2f((28 + (28 * i)) * scale, 0 * scale));
        sega[i].setPoint(2, sf::Vector2f((23 + (28 * i)) * scale, 4 * scale));
        sega[i].setPoint(3, sf::Vector2f((11 + (28 * i)) * scale, 4 * scale));
        
        segb[i].setPoint(0, sf::Vector2f((23 + (28 * i)) * scale, 4 * scale));
        segb[i].setPoint(1, sf::Vector2f((28 + (28 * i)) * scale, 0 * scale));
        segb[i].setPoint(2, sf::Vector2f((24 + (28 * i)) * scale, 16 * scale));
        segb[i].setPoint(3, sf::Vector2f((20.5 + (28 * i)) * scale, 14 * scale));
        
        segc[i].setPoint(0, sf::Vector2f((19.5 + (28 * i)) * scale, 18 * scale));
        segc[i].setPoint(1, sf::Vector2f((24 + (28 * i)) * scale, 16 * scale));
        segc[i].setPoint(2, sf::Vector2f((20 + (28 * i)) * scale, 32 * scale));
        segc[i].setPoint(3, sf::Vector2f((17 + (28 * i)) * scale, 28 * scale));
        
        segd[i].setPoint(0, sf::Vector2f((5 + (28 * i)) * scale, 28 * scale));
        segd[i].setPoint(1, sf::Vector2f((17 + (28 * i)) * scale, 28 * scale));
        segd[i].setPoint(2, sf::Vector2f((20 + (28 * i)) * scale, 32 * scale));
        segd[i].setPoint(3, sf::Vector2f((0 + (28 * i)) * scale, 32 * scale));
        
        sege[i].setPoint(0, sf::Vector2f((4 + (28 * i)) * scale, 16 * scale));
        sege[i].setPoint(1, sf::Vector2f((7.5 + (28 * i)) * scale, 18 * scale));
        sege[i].setPoint(2, sf::Vector2f((5 + (28 * i)) * scale, 28 * scale));
        sege[i].setPoint(3, sf::Vector2f((0 + (28 * i)) * scale, 32 * scale));
        
        segf[i].setPoint(0, sf::Vector2f((8 + (28 * i)) * scale, 0 * scale));
        segf[i].setPoint(1, sf::Vector2f((11 + (28 * i)) * scale, 4 * scale));
        segf[i].setPoint(2, sf::Vector2f((8.5 + (28 * i)) * scale, 14 * scale));
        segf[i].setPoint(3, sf::Vector2f((4 + (28 * i)) * scale, 16 * scale));
        
        segg[i].setPoint(0, sf::Vector2f((4 + (28 * i)) * scale, 16 * scale));
        segg[i].setPoint(1, sf::Vector2f((8.5 + (28 * i)) * scale, 14 * scale));
        segg[i].setPoint(2, sf::Vector2f((20.5 + (28 * i)) * scale, 14 * scale));
        segg[i].setPoint(3, sf::Vector2f((24 + (28 * i)) * scale, 16 * scale));
        segg[i].setPoint(4, sf::Vector2f((19.5 + (28 * i)) * scale, 18 * scale));
        segg[i].setPoint(5, sf::Vector2f((7.5 + (28 * i)) * scale, 18 * scale));
        segdp[i].setRadius(2 * scale);
        segdp[i].setPosition((22 + (28 * i)) * scale, 28 * scale);
        
        sega[i].setFillColor(ledon);
        segb[i].setFillColor(ledon);
        segc[i].setFillColor(ledon);
        segd[i].setFillColor(ledon);
        sege[i].setFillColor(ledon);
        segf[i].setFillColor(ledon);
        segg[i].setFillColor(ledon);
        segdp[i].setFillColor(ledon);
        
        sega[i].setOutlineThickness(-0.5 * scale);
        segb[i].setOutlineThickness(-0.5 * scale);
        segc[i].setOutlineThickness(-0.5 * scale);
        segd[i].setOutlineThickness(-0.5 * scale);
        sege[i].setOutlineThickness(-0.5 * scale);
        segf[i].setOutlineThickness(-0.5 * scale);
        segg[i].setOutlineThickness(-0.5 * scale);
        segdp[i].setOutlineThickness(-0.5 * scale);
        
        sega[i].setOutlineColor(sf::Color(0, 0, 0));
        segb[i].setOutlineColor(sf::Color(0, 0, 0));
        segc[i].setOutlineColor(sf::Color(0, 0, 0));
        segd[i].setOutlineColor(sf::Color(0, 0, 0));
        sege[i].setOutlineColor(sf::Color(0, 0, 0));
        segf[i].setOutlineColor(sf::Color(0, 0, 0));
        segg[i].setOutlineColor(sf::Color(0, 0, 0));
        segdp[i].setOutlineColor(sf::Color(0, 0, 0));
        
        state.display[i] = 0x00;
    }
    
    // The Halt icon shows when the machine is halted
    halt_led.setSize(sf::Vector2f(8 * scale, 8 * scale));
    halt_led.setPosition(214 * scale, 34 * scale);
    
    // Load Elbug into ROM
    if (load_rom("elbug.001", 0x00, 0x200, main_memory) == false) {
        exit(1);
    }
    if (load_rom("elbug.002", 0x200, 0x200, main_memory) == false) {
        exit(1);
    }
    if (load_rom("elbug.003", 0x400, 0x200, main_memory) == false) {
        exit(1);
    }
    
    /* 
     * To load other ROM files into memory follow this pattern. E.g.:
     * 
     * if (load_rom("CLOCK.ROM", 0xf00, 0xc0, main_memory) == false) {
     *    exit(1);
     * }
     * 
     * Parameters are:
     * "CLOCK.ROM" - the filename
     * 0xf00 - where the first byte of the file should be in SC/MP memory
     * 0xc0 - how many bytes to copy into memory
     * main_memory - a link to the RAM of the emulated machine
     * 
     */
    
    while (window.isOpen())
    {
        sf::Event event;
        
        while (window.pollEvent(event))
        {
            // Close application on request
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            
            // Don't react to keyboard input when not in focus
            if (event.type == sf::Event::LostFocus)
                inFocus = false;
            
            if (event.type == sf::Event::GainedFocus)
                inFocus = true;
                
            if (inFocus) {
                // Respond to key presses
                if (event.type == sf::Event::KeyPressed) {
                    switch(event.key.code) {
                        case sf::Keyboard::Num0:
                            state.hexkey = 0xf0;
                            break;
                        case sf::Keyboard::Num1:
                            state.hexkey = 0xf1;
                            break;
                        case sf::Keyboard::Num2:
                            state.hexkey = 0xf2;
                            break;
                        case sf::Keyboard::Num3:
                            state.hexkey = 0xf3;
                            break;
                        case sf::Keyboard::Num4:
                            state.hexkey = 0xf4;
                            break;
                        case sf::Keyboard::Num5:
                            state.hexkey = 0xf5;
                            break;
                        case sf::Keyboard::Num6:
                            state.hexkey = 0xf6;
                            break;
                        case sf::Keyboard::Num7:
                            state.hexkey = 0xf7;
                            break;
                        case sf::Keyboard::Num8:
                            state.hexkey = 0xf8;
                            break;
                        case sf::Keyboard::Num9:
                            state.hexkey = 0xf9;
                            break;
                        case sf::Keyboard::A:
                            state.hexkey = 0xfa;
                            break;
                        case sf::Keyboard::B:
                            state.hexkey = 0xfb;
                            break;
                        case sf::Keyboard::C:
                            state.hexkey = 0xfc;
                            break;
                        case sf::Keyboard::D:
                            state.hexkey = 0xfd;
                            break;
                        case sf::Keyboard::E:
                            state.hexkey = 0xfe;
                            break;
                        case sf::Keyboard::F:
                            state.hexkey = 0xff;
                            break;
                        case sf::Keyboard::F1: // C0 = UP
                            state.hexkey = 0x80;
                            break;
                        case sf::Keyboard::F2: // C1 = DOWN
                            state.hexkey = 0x90;
                            break;
                        case sf::Keyboard::F3: // C2 = CPU REGISTER
                            state.hexkey = 0xa0;
                            break;
                        case sf::Keyboard::F4: // C3 = BLOCK-TRANSFER
                            state.hexkey = 0xb0;
                            break;
                        case sf::Keyboard::F5: // C4 = CASSETTE
                            state.hexkey = 0xc0;
                            break;
                        case sf::Keyboard::F6: // C5 = SUBTRACTION
                            state.hexkey = 0xd0;
                            break;
                        case sf::Keyboard::F7: // C6 = MODIFY
                            state.hexkey = 0xe0;
                            break;
                        case sf::Keyboard::F8: // C7 = RUN
                            state.hexkey = 0xf0;
                            break;
                        case sf::Keyboard::F9: // Halt/Continue
                            state.h = !state.h;
                            break;
                        case sf::Keyboard::F10: // Reset (NRST)
                            reset_cpu(&state);
                            break;
                        case sf::Keyboard::F11: // Change Colour
                            led_colour++;
                            if (led_colour > 2) {
                                led_colour = 0;
                            }
                            switch (led_colour) {
                                case 0:
                                    ledoff = sf::Color(20,0,0);
                                    ledon = sf::Color(250,0,0);
                                    break;
                                case 1:
                                    ledoff = sf::Color(0,20,0);
                                    ledon = sf::Color(0,250,0);
                                    break;
                                case 2:
                                    ledoff = sf::Color(0,0,20);
                                    ledon = sf::Color(0,0,250);
                                    break;
                            }
                            break;
                        case sf::Keyboard::F12: // Exit
                            window.close();
                            break;
                        /* - Uncomment to detect any other key presses
                        default:
                           io.key_press(event.key.code);
                            break;
                        */
                    }
                }
                
                if (event.type == sf::Event::KeyReleased) {
                    state.hexkey = 0x00;
                }
            }
        }
        
        // Send as many clock pulses to the CPU as would happen
        // between screen frames
        running_time -= operations_per_frame;
        while (running_time < 0 && state.h == false) {
            time = EmulateSCMPOp(&state);
            running_time += time;
            
            // Audio output - will output a time stamp every time flag 1 changes state
            // Can be used alongside "makepcm.c" to make the SC/MP "Sing"!
            if (state.sr.f1 != flag1) {
                flag1 = state.sr.f1;
                pulse_width = running_time - last_pulse;
                if (pulse_width < 0) {
                    pulse_width += operations_per_frame;
                }
                last_pulse = running_time;
                printf("%d\n", pulse_width);
            }
            
            // Draw display
            window.clear();
            for (i = 0; i < 8; i++) {
                if (state.display[7 - i] & 0x01) {
                    sega[i].setFillColor(ledon);
                } else {
                    sega[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x02) {
                    segb[i].setFillColor(ledon);
                } else {
                    segb[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x04) {
                    segc[i].setFillColor(ledon);
                } else {
                    segc[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x08) {
                    segd[i].setFillColor(ledon);
                } else {
                    segd[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x10) {
                    sege[i].setFillColor(ledon);
                } else {
                    sege[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x20) {
                    segf[i].setFillColor(ledon);
                } else {
                    segf[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x40) {
                    segg[i].setFillColor(ledon);
                } else {
                    segg[i].setFillColor(ledoff);
                }
                if (state.display[7 - i] & 0x80) {
                    segdp[i].setFillColor(ledon);
                } else {
                    segdp[i].setFillColor(ledoff);
                }
                
                window.draw(sega[i]);
                window.draw(segb[i]);
                window.draw(segc[i]);
                window.draw(segd[i]);
                window.draw(sege[i]);
                window.draw(segf[i]);
                window.draw(segg[i]);
                window.draw(segdp[i]);
            }
            
            if (state.h) {
                halt_led.setFillColor(ledon);
            } else {
                halt_led.setFillColor(ledoff);
            }
            window.draw(halt_led);
            
            window.display();
        } 
    }

    return 0;
}
