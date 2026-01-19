#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "sndemu/dsp.h"
#include "sndemu/SPC_DSP.h"
#include "sndemu/SPC_Filter.h"
#include <SFML/Audio.hpp>

unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x7f,
};

unsigned char c700sqwave[] = {
    0b10000100, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000011, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77
};

// Constants for the DSP registers.

#define vvoll(n) (n<<4)+0
#define vvolr(n) (n<<4)+1
#define vpl(n) (n<<4)+2
#define vph(n) (n<<4)+3
#define vsrcn(n) (n<<4)+4
#define vadsr1(n) (n<<4)+5
#define vadsr2(n) (n<<4)+6
#define vgain(n) (n<<4)+7
#define len(x) *(&x+1)-x
#define lobit(x) x&0xff
#define hibit(x) x>>8
#define print(x) std::cout << x << std::endl;
#define r(x) dsp->read(x)
#define w(x,y) dsp->write(x,y)


enum {
    MVOLL = 0x0C,
    MVOLR = 0x1C,
    FLG = 0x6C,
    DIR = 0x5D,
    V0VOLL = 0x00,
    V0VOLR = 0x01,
    V0PITCHL = 0x02,
    V0PITCHH = 0x03,
    V0SRCN = 0x04,
    V0ADSR1 = 0x05,
    V0ADSR2 = 0x06,
    V0GAIN = 0x07,
    KON = 0x4C,
    KOF = 0x5C,
    RESET = 128,
    MUTE = 64,
    ECEN = 32,
    EDL = 0x7D,
};

/*necessary variables*/
typedef unsigned int uint;
uint smppos = 0x200;
SPC_DSP* dsp = new SPC_DSP;
SPC_Filter* f = new SPC_Filter;
char* aram = (char*)malloc(0x10000);
unsigned int dirposition = 0;


/*functions*/

void pitch(int p, int voice) {
    dsp->write(vpl(voice), p & 0xff);
    dsp->write(vph(voice), (p >> 8) & 0x3f);
}

void advance(unsigned int samples) {dsp->run(32*samples);}
void advance() { dsp->run(32); }

void pitchbendDEMO(spc_dsp_sample_t* out) {
    spc_dsp_write(dsp, V0SRCN, 0);

    pitch(0x042f, 0);

    spc_dsp_write(dsp, V0ADSR1, 0x85);
    spc_dsp_write(dsp, V0ADSR2, 0x0E);
    dsp->write(V0GAIN, 0x80);

    spc_dsp_write(dsp, KON, 0x01);
    spc_dsp_run(dsp, (32 * 16000) - 64); f->run(out, (32 * 16000)-64);
    for (int i = 0; i < 136; i++) {
        pitch(0x042f + i, 0);
        dsp->run(32 * 64); f->run(out, 32 * 64);
    }
    spc_dsp_run(dsp, 32 * 8000); f->run(out, 32 * 8000);
    pitch(0x054f, 0);
    spc_dsp_write(dsp, KOF, 0x01);
    spc_dsp_write(dsp, FLG, 0x20+128);
    advance(2);    spc_dsp_write(dsp, FLG, 0x20);
    spc_dsp_write(dsp, KON, 0x01);
    spc_dsp_run(dsp, 32 * 64000); f->run(out, 32 * 64000);

}




void smp2aram(short* sample, uint length, uint lppoint) {
    memcpy(aram+smppos, sample, length);
    aram[0x1000 + dirposition++] = lobit(smppos);
    aram[0x1000 + dirposition++] = hibit(smppos);
    aram[0x1000 + dirposition++] = lobit(lppoint+smppos);
    aram[0x1000 + dirposition++] = hibit(lppoint+smppos);
    smppos += length;
}

void tick(unsigned int ticks) {dsp->run(8534 * ticks);}
void tick() { dsp->run(8534); }

void load() {
    dsp->write(KOF, 0xff);
    for (int i = 0x7F; i >= 0; i--) {
        if (i == FLG) { dsp->write(FLG, 0x60); }
        else if (i == 0x6D) { dsp->write(0x6D, 0x80); }
        else { dsp->write(i, 0x00); }
    }
    dsp->write(FLG, 0b00100000);
    for (int x = 0; x < 8; x++) {
        dsp->write(vvoll(x), 0x7f);
        dsp->write(vvolr(x), 0x7f);
    }
    dsp->write(MVOLL, 0x7F);
    dsp->write(MVOLR, 0x7F);
}

void note(uint length_in_ticks, uint P, uint voice) {
    w(FLG, 128+64);
    pitch(P, voice);
    w(FLG, ECEN); //allows user to run and play
    w(KON, 0x01);
}

/*ch_mask: example: channels 7, 8 and 1
    V7  V6  V5  V4  V3  V2  V1  V0
    1   1   0   0   0   0   0   1  */
void stopnote(int ch_mask) { w(KON, 0); w(KOF, ch_mask); advance(2); }


/*main*/

int main(int argc, char* argv[]) {
    load();
    if (dsp == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate DSP");
        return 1;
    }

    unsigned char aram[0x10000]; // 64KB
    if (aram == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate ARAM");
        return 1;
    }

    spc_dsp_init(dsp, aram);
    dsp->write(DIR, 0x01);

    int sample_count = 32 * 32000 * 2;
    spc_dsp_sample_t* output =
        (spc_dsp_sample_t*)malloc(2 * sample_count * sizeof(spc_dsp_sample_t));

    if (output == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate output buffer");
        return 1;
    }

    spc_dsp_set_output(dsp, output, sample_count);

    spc_dsp_reset(dsp);

    // Load our instrument into the beginning of ARAM.
    memcpy(aram + 0x200, BRR_SAWTOOTH, sizeof(BRR_SAWTOOTH) / sizeof(unsigned char));


    // There's only one entry in our table.
    aram[0x1000] = 0x00; // sample start address low byte
    aram[0x1000 + 1] = 0x02; // sample start address high byte
    aram[0x1000 + 2] = 0x09; // sample loop address low byte
    aram[0x1000 + 3] = 0x02; // sample loop address high byte

    spc_dsp_write(dsp, DIR, 0x10);

    spc_dsp_write(dsp, V0VOLL, 0x7F);
    spc_dsp_write(dsp, V0VOLR, 0x7F);

    spc_dsp_write(dsp, MVOLL, 0x7f);
    spc_dsp_write(dsp, MVOLR, 0x7f);

    spc_dsp_write(dsp, V0ADSR1, 0x85);
    spc_dsp_write(dsp, V0ADSR2, 0x0e);
    spc_dsp_write(dsp, KON, 0x01);
    spc_dsp_write(dsp, V0SRCN, 0);

    pitch(0x27f, 0);
    dsp->run(32 * 32000); /*f->run(output, 32 * 32000);*/
    spc_dsp_write(dsp, KON, 0x00);
    spc_dsp_write(dsp, KOF, 0x01);
    //spc_dsp_write(dsp, FLG, RESET+MUTE);
    advance(2);
    spc_dsp_write(dsp, KOF, 0x00);
    spc_dsp_write(dsp, KON, 0x01);
    //spc_dsp_write(dsp, FLG, 0);
    pitch(0x57f, 0); 
    dsp->run(32 * 32000); /*f->run(output, 32 * 32000);*/


    int generated_count = spc_dsp_sample_count(dsp) / 2;
    printf("Generated %d samples\n", generated_count);
    std::cout << "V4SRCN = " << vvoll(4) << std::endl;

    for (int i = 32000; i < generated_count/2+16; i++) {
        printf("%02d: %04hx %04hx\n", i, output[i * 2], output[1 + i * 2]);
    }

    sf::SoundBuffer buf(output, 32*32000*2, 2, 32000, {sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight});
    sf::SoundBuffer bufwav("its gonna sound like shit trust me bro.wav");
    sf::Sound snd(buf);
    snd.setLooping(0);
    snd.play();
    sf::sleep(sf::milliseconds(2000));

    // We're all done!
    return 0;
}