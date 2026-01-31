#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "sndemu/dsp.h"
#include "sndemu/SPC_DSP.h"
#include "sndemu/SPC_Filter.h"
#include <SFML/Audio.hpp>
using namespace sf;
#define print(x) std::cout << x << std::endl

/*necessary variables*/
typedef unsigned int uint;
uint smppos = 0x200;
SPC_DSP* dsp = new SPC_DSP;
char* aram = (char*)malloc(0x10000);
uint dirposition = 0;
uint smplimit = 0;
int sample_count = 32 * 32000;
spc_dsp_sample_t* output = (spc_dsp_sample_t*)malloc(2 * sample_count * sizeof(spc_dsp_sample_t)); //buffer
int out() {
    dsp->set_output(output, 2 * sample_count);
    if (output == NULL) { print("you are fucking stupid"); return -1; }
}



unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};

unsigned char c700sqwave[] = {
    0b10000100, 0x00, 0x00, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0b11000000, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,
    0b11000011, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77
};

// Constants for the DSP registers.

#define vvoll(n) (n<<4)
#define vvolr(n) (n<<4)+1
#define vpl(n) (n<<4)+2
#define vph(n) (n<<4)+3
#define vsrcn(n) (n<<4)+4
#define vadsr1(n) (n<<4)+5
#define vadsr2(n) (n<<4)+6
#define vgain(n) (n<<4)+7
#define venvx(n) (n<<4)+8
#define voutx(n) (n<<4)+9
#define len(x) *(&x+1)-x
#define lobit(x) x&0xff
#define hibit(x) x>>8
#define r(x) dsp->read(x)
#define w(x,y) dsp->write(x,y)
int vxkon;

enum {
    MVOLL = 0x0C,
    MVOLR = 0x1C,
    FLG = 0x6C,
    DIR = 0x5D,
    KON = 0x4C,
    KOF = 0x5C,
    ADSR = 128,
    PMON = 0x2D,
    NON = 0x3D,
    ESA = 0x6D,
    RESET = 128,
    MUTE = 64,
    ECEN = 32,
    EON = 0x4D,
};


/*functions*/

void smplimitcheck(uint amount) {
    smplimit += amount;
    if (smplimit > sample_count) {
        out();
        smplimit = 0;
    }
}
int getsmplimit() { return smplimit; }

void pitch(int p, int voice) {
    w(vpl(voice), p & 0xff);
    w(vph(voice), (p >> 8)&0x3f);
}


void advance(unsigned int samples) { dsp->run(32 * samples); /*smplimitcheck(32*samples);*/ }
void advance() { dsp->run(32); /*smplimitcheck(32);*/
}


void smp2aram(unsigned char* sample, unsigned int length, unsigned int lppoint) {
    memcpy(aram+smppos, sample, length);
    aram[0x1000 + dirposition++] = lobit(smppos);
    aram[0x1000 + dirposition++] = hibit(smppos);
    aram[0x1000 + dirposition++] = lobit(lppoint+smppos);
    aram[0x1000 + dirposition++] = hibit(lppoint+smppos);
    smppos += length;
}

void tick(unsigned int ticks) {dsp->run(5334 * ticks); /*smplimitcheck(5334*ticks);*/
}
void tick() {
    dsp->run(5334); /*smplimitcheck(5334);*/
}

//usage: tick(note(length_in_ticks, voice, hz, p, srcn, voll, volr, adsr1, adsr2))
int note(uint length_in_ticks, uint voice, bool hz, uint p, uint srcn, uint voll, uint volr, uint adsr1, uint adsr2) {
    vxkon++;
    w(vvoll(voice), std::trunc(voll/vxkon));
    w(vvolr(voice), std::trunc(volr / vxkon));
    w(vsrcn(voice), 1);
    if (hz == 1) pitch((4096 * (p / 32000)) & 0x3fff, voice);
    else pitch(p & 0x3fff, voice);
    w(vadsr1(voice), adsr1);
    w(vadsr2(voice), adsr2);
    w(KOF, 0<<voice); w(KON, 1<<voice);
    return length_in_ticks;
}

//usage: tick(note(length_in_ticks, voice, hz, p, srcn, vol, adsr1, adsr2))
int note(uint length_in_ticks, uint voice, bool hz, uint p, uint srcn, uint vol, uint adsr1, uint adsr2) {
    vxkon++;
    w(vvoll(voice), std::trunc(vol / vxkon));
    w(vvolr(voice), std::trunc(vol / vxkon));
    w(vsrcn(voice), 1);
    if (hz == 1) pitch((4096 * (p / 32000)) & 0x3fff, voice);
    else pitch(p & 0x3fff, voice);
    w(vadsr1(voice), adsr1);
    w(vadsr2(voice), adsr2);
    w(KOF, 0 << voice); w(KON, 1 << voice);
    return length_in_ticks;
}

void cmajor() {
    w(vvoll(0), 0x3F); w(vvolr(0), 0x3F); //1. VXVOL
    w(vsrcn(0), 1); w(vsrcn(1), 1); //2. VXSRCN
    w(vadsr1(0), ADSR + 0xA); //3. VXADSR(1+2)
    w(vadsr2(0), 0xe0);
    pitch(0x10bf, 0); //4. VXP (function)
    w(KOF, 0b11111110); //5. KOF
    w(KON, 1); //6. KON
    dsp->run(32 * 32000 / 4 - (256 * 32)); //RUN
    w(KOF, 3); advance(256); pitch(0x12bf, 0); pitch(0x1000, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x14ff, 0); pitch(0x10bf, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x167f, 0); pitch(0x12bf, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x18ff, 0); pitch(0x14ff, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x1c40, 0); pitch(0x167f, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x2000, 0); pitch(0x18ff, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(256); pitch(0x217f, 0); pitch(0x14ff, 1); w(KOF, 0); w(KON, 3);
    dsp->run(32 * 32000 / 4 - (256 * 32)); w(KON, 0);
    w(KOF, 3); advance(16384);
}

/*structures/classes*/

class SPC700 : public SoundStream {
public:
    void load(const SoundBuffer& out) {
        sfOut.assign(out.getSamples(), out.getSamples() + out.getSampleCount());
        sfCSample = 0;
        initialize(out.getChannelCount(), out.getSampleRate(), { SoundChannel::FrontLeft,SoundChannel::FrontRight });
    }
    struct sample {
        uint dirstart, loloop, hiloop;
    };

    SPC700::sample newsample(unsigned char* sample, uint length, uint lppoint) {
        SPC700::sample x = { 0,0,0 };
        memcpy(ar + smppos, sample, length);
        x.dirstart = 0xff00 + dirposition;
        ar[0xff00 + dirposition++] = lobit(smppos);
        ar[0xff00 + dirposition++] = hibit(smppos);
        ar[0xff00 + dirposition++] = lobit(lppoint + smppos); x.loloop = hibit(smppos);
        ar[0xff00 + dirposition++] = hibit(lppoint + smppos); x.hiloop = hibit(smppos);
        smppos += length;
        return x;
    }

    void changeloop(SPC700::sample &sample, uint newloop) {
        ar[sample.dirstart + 2] = lobit(newloop); sample.loloop = lobit(newloop);
        ar[sample.dirstart + 3] = hibit(newloop); sample.loloop = hibit(newloop);
    }

    void pitch(uint voice, uint pitch) {
        w(vpl(voice), lobit(pitch));
        w(vph(voice), hibit(pitch) & 0x3f);
    }

    void run(uint clocks) {
        d->run(clocks);
        bufplace += clocks;
        if (bufplace > 5334) {
            bufplace = 0;
            d->set_output(out, 2 * 5334);
        }
    }
    void run() {
        d->run(1);
        bufplace ++;
        if (bufplace > 5334) {
            bufplace = 0;
            d->set_output(out, 2 * 5334);
        }
    }

    void advance() { run(32); }
    void advance(uint samples) { run(32 * samples); }

    void tick() { run(5334); }
    void tick(uint ticks) { run(5334 * ticks); }

    int note(uint length_in_ticks, uint voice, uint pitch_in_hex, uint srcn, uint voll, uint volr) {
        if (voice > 8 || voice < 0) print("can't write to this voice"); return -1;
        if (voll > 127) print("VOLL too loud"); return -1; if (volr > 127) print("VOLR too loud"); return -1;
        vxkon++;
        w(vsrcn(voice), srcn);
        w(vvoll(voice), std::trunc(voll/vxkon));
        w(vvolr(voice), std::trunc(volr/vxkon));
        return 0;
    }

protected:
    typedef SPC_DSP::sample_t sample_t;
    sample_t* out = (sample_t*)malloc(2 * 5334 * sizeof(sample_t));
    uint smppos = 0x200;
    uint vxkon = 0;
    uint bufplace = 0;
private:
    /*DSP*/
    SPC_DSP* d = new SPC_DSP;
    char* ar = (char*)malloc(0x10000);
    uint dirposition = 0;

    SPC700() {
    #undef w(x,y)
    #undef r(x)
    #undef lobit(x)
    #undef hibit(x)
    #define w(x,y) d->write(x,y)
    #define r(x) d->read(x)
    #define lobit(x) x&255
    #define hibit(x) x>>8
        d->init(ar);
        d->set_output(out, 2*5334);
        for (int i = 0; i < SPC_DSP::register_count; i++) {
            if (i == FLG) w(FLG, 0x60);
            else if (i == ESA) w(ESA, 0x80);
            else if (i == KOF) w(KOF, 0xff);
            else w(i, 0);
        }
        w(DIR, 0xff); w(MVOLL, 127); w(MVOLR, 127); w(FLG, ECEN);
    }


    /*SFML*/
    bool onGetData(Chunk &data) override {
        const int count = 2;
        data.samples = &sfOut[sfCSample];
        if (sfCSample + count <= sfOut.size()) {
            data.sampleCount = count;
            data.samples += count;
            return true;
        }
        else {
            data.sampleCount = sfOut.size() - sfCSample;
            sfCSample = sfOut.size();
            return false;
        }
    }
    void onSeek(Time offset) override {
        sfCSample = static_cast<std::size_t>(offset.asSeconds() * getSampleRate() * getChannelCount());
    }
    std::vector<std::int16_t> sfOut;
    std::size_t sfCSample{};
};


/*main*/


int main() {
#define r(x) dsp->read(x)
#define w(x,y) dsp->write(x,y)

    /*initialising DSP and registers*/
    if (dsp == NULL) { std::cout << "no DSP lol" << std::endl; }
    dsp->init(aram);
    if (aram == NULL) { std::cout << "no ARAM lol" << std::endl; }

    /*DSP*/
    out();
    dsp->reset();


    /*load instruments*/
    smp2aram(BRR_SAWTOOTH, len(BRR_SAWTOOTH), 0);
    smp2aram(c700sqwave, len(c700sqwave), 9);

    /*initialisation*/
    for (int i = 0; i < 0x80; i++) {
        if (i == FLG) w(FLG, 0x60);
        else if (i == ESA) w(ESA, 0x80);
        else if (i == KOF) w(KOF, 0xff);
        else w(i, 0);
    }

    /*hopefully creating the DSP sound*/
    w(FLG, ECEN); w(DIR, 0x10); w(MVOLL, 127); w(MVOLR, 127);
    tick(note(190, 0, 0, 0x107f, 1, 127, ADSR + 0xA, 0xE0));
    w(KON, 0); w(KOF, 1); tick(2);
    tick(note(190, 0, 0, 0x12af, 1, 127, ADSR + 0xA, 0xE0));
    w(KON, 0); w(KOF, 1); tick(64);
    

    /*debug (remove when not needed)*/
    int generated_count = dsp->sample_count() / 2;
    printf("Generated %d samples\n", generated_count);

    for (int i = 0; i < 10; i++) {
        printf("%02d: %04hx %04hx\n", i, output[i * 2], output[1 + i * 2]);
    }


    /*SFML side of playing sound*/
    sf::SoundBuffer buf(output, 32 * 32000, 2, 32000, { sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight });
    sf::SoundBuffer bufwav("its gonna sound like shit trust me bro.wav");
    sf::Sound snd(buf);
    snd.play();
    sf::sleep(sf::milliseconds(3000));

}