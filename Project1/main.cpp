#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sndEMU/dsp.h"
#include <SFML/Audio.hpp>

unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};
#define len(x) *(&x+1)-x

// Constants for the DSP registers.
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
    KON = 0x4C,
};

class SPC700 : public sf::SoundStream {
public:
    SPC_DSP* dsp = spc_dsp_new();
    char* aram = (char*)calloc(0x1000, sizeof(char));
    spc_dsp_sample_t* out = (spc_dsp_sample_t*)calloc(2 * 32, sizeof(spc_dsp_sample_t));
    unsigned dtpos = 0;
    unsigned dir = 0xff;
    unsigned pos = 0x200;
    #define lobit(x) x&255
    #define hibit(x) x>>8
    #define voll(x) (x<<4)
    #define volr(x) (x<<4)+1
    #define pl(x) (x<<4)+2
    #define ph(x) (x<<4)+3
    #define srcn(x) (x<<4)+4
    #define adsr1(x) (x<<4)+5
    #define adsr2(x) (x<<4)+6
    #define gain(x) (x<<4)+7
    #define envx(x) (x<<4)+8
    #define outx(x) (x<<4)+9
    #define r(x) spc_dsp_read(dsp,x)
    #define w(x,y) spc_dsp_write(dsp,x,y);
    #define run(x) spc_dsp_run(dsp,x);
    SPC700() {
        spc_dsp_init(dsp, aram);
        spc_dsp_set_output(dsp, out, 32);
        spc_dsp_reset(dsp);
        w(DIR, 0xff);
        w(voll(0), 128);
        w(volr(0), 128);
        w(MVOLL, 128);
        w(MVOLR, 128);
        w(FLG, 0x20);
        w(srcn(0), 0);
        w(pl(0), 0x00);
        w(ph(0), 0x10);
        w(adsr1(0), 0x0F);
        w(adsr2(0), 0xE0); //move some of them to note function if succeeded
    }
    void newsample(unsigned char* sample, size_t length, unsigned loop) {
        memcpy(aram + pos, sample, length);
        aram[dir + dtpos] = (pos) & 0xff;
    }
    void testplay(unsigned times) {
        w(KON, 0);
        run(32 * times);
    }
private:
    bool onGetData(Chunk& data) override { spc_dsp_set_output(dsp, out, 32); return true; }
    void onSeek(sf::Time x)override{}
};

int main(int argc, char* argv[]) {
    SPC700* emu = new SPC700;
    emu->newsample(BRR_SAWTOOTH, len(BRR_SAWTOOTH),0);
    emu->testplay(128);
}