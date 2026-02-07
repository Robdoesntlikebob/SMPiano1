#include "stdc++.h"
#include <iostream>
#include "sndEMU/dsp.h"
#include <SFML/Audio.hpp>

unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};
#define len(x) *(&x+1)-x
#define print(x) std::cout << x << std::endl

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
private:
    SPC_DSP* dsp = spc_dsp_new();
    typedef spc_dsp_sample_t sample_t;
    std::array<char, 0x10000 * sizeof(char)> aram{};
    std::array<sample_t, 64000 * sizeof(sample_t)> out{};
    bool onGetData(Chunk& data) override {
        spc_dsp_set_output(dsp, out.data(), out.size());
        while (spc_dsp_sample_count(dsp) > out.size()) {
            run(512000);
        }
        data.sampleCount = out.size();
        data.samples = out.data();
        return true;
    }
    void onSeek(sf::Time x)override{}
    unsigned dtpos = 0;
    const unsigned dir = 0xff;
    unsigned pos = 0x200;
public:
    SPC700() {
        spc_dsp_init(dsp, aram.data());
        spc_dsp_set_output(dsp, out.data(),out.size());
        spc_dsp_reset(dsp);
        initialize(2, 32000, { sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight });
    }
    void newsample(unsigned char* sample, unsigned length, unsigned loop) {
        memcpy(aram.data() + pos, sample, length);
        aram.data()[dir + dtpos] = lobit(pos); dtpos++;
        aram.data()[dir + dtpos] = hibit(pos); dtpos++;
        aram.data()[dir + dtpos] = hibit(loop+pos); dtpos++;
        aram.data()[dir + dtpos] = hibit(loop+pos); dtpos++;
        pos += length;
    }
    void testplay() {
        w(DIR, 0xff);
        w(voll(0), 128);
        w(volr(0), 128);
        w(MVOLL, 128);
        w(MVOLR, 128);
        w(FLG, 0x20);
        w(srcn(0), 0);
        w(pl(0), 0xFF);
        w(ph(0), 0x1F);
        w(adsr1(0), 0x0F);
        w(adsr2(0), 0xE0);
        w(KON, 1);
    }
    void debug() {
        print(spc_dsp_sample_count(dsp));
    }
};

int main() {
    SPC700 emu;
    emu.newsample(BRR_SAWTOOTH, len(BRR_SAWTOOTH),0);
    emu.play();
    emu.testplay();
    while (emu.getStatus() == sf::SoundSource::Status::Playing) { emu.debug(); }
}