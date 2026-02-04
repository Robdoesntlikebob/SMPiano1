#include "stdc++.h"
#include "sndemu/dsp.h"
#include "sndemu/SPC_DSP.h"
#include "olcNoiseMaker.h"
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Network.hpp>
#include <SFML/OpenGL.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

/*necessary variables*/
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
typedef unsigned int uint;

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
#define print(x) std::cout << x << std::endl
#define r(x) dsp->read(x)
#define w(x,y) dsp->write(x,y)
#define CHANNELS {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight}


SPC_DSP* dsp = new SPC_DSP;
char* aram = (char*)calloc(0x10000, sizeof(char));
SPC_DSP::sample_t* out = (SPC_DSP::sample_t*)calloc(2, sizeof(SPC_DSP::sample_t));
unsigned smppos = 0x200;
unsigned dir = 0xff;
unsigned adir = dir<<8;
unsigned vxkon = 0;

double rsamples(double dTime) {
    return out[0], out[1];
}

void newsample(unsigned char* sample, unsigned length, unsigned lppoint) {
    memcpy(aram + smppos, sample, length);
    aram[adir + 0] = lobit(smppos);
    aram[adir + 1] = hibit(smppos);
    aram[adir + 2] = lobit(lppoint + smppos);
    aram[adir + 3] = hibit(lppoint + smppos);
    smppos += length;
}

void wsample() {
    dsp->run(32); dsp->set_output(out, len(out));
}
void wsample(unsigned samples) {
    for (int i = 0; i < samples; ++i) {
        wsample();
    }
}

void tick() {
    for (int i = 0;i < 166.6875; i++) {
        wsample();
    }
}
void tick(unsigned ticks) {
    for (int i = 0; i < ticks; i++) {
        tick();
    }
}

void pitch(unsigned p, unsigned voice) {
    w(vpl(voice), lobit(p));
    w(vph(voice), hibit(p) & 0x3f);
}//(4096*(p/32000))&0x3fff



//usage: tick(note(length_in_ticks, voice, hz, p, srcn, voll, volr, adsr1, adsr2))
int note(uint length_in_ticks, uint voice, bool hz, uint p, uint srcn, uint voll, uint volr, uint adsr1, uint adsr2) {
    vxkon++;
    w(vvoll(voice), std::trunc(voll / vxkon));
    w(vvolr(voice), std::trunc(volr / vxkon));
    w(vsrcn(voice), 1);
    if (hz == 1) pitch((4096 * (p / 32000)) & 0x3fff, voice);
    else pitch(p & 0x3fff, voice);
    w(vadsr1(voice), adsr1);
    w(vadsr2(voice), adsr2);
    w(KOF, 0 << voice); w(KON, 1 << voice);
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
void endnote(uint voice) {
    w(KOF, 1 << voice); w(KON, 0 << voice);
    tick(2);
}

/*main*/
using namespace ImGui;
using namespace sf;
int main() {
    std::vector<std::wstring> devices = olcNoiseMaker<SPC_DSP::sample_t>::Enumerate();
    olcNoiseMaker<SPC_DSP::sample_t> sound(devices[0],32000U,2U,1U,2U);
    dsp->init(aram);
    dsp->set_output(out, len(out));
    dsp->reset();
    for (unsigned i = 0; i < 128; i++) {
        if (i == FLG) w(FLG, 0x60);
        else if (i == ESA) w(ESA, 0x80);
        else if (i == KOF) w(KOF, 0xff);
        else w(i, 0);
    }w(MVOLL, 127); w(MVOLR, 127); w(DIR, dir);
    sound.SetUserFunction(rsamples);
    std::cout << "SPC700 playing note B3 Square wave..." << std::endl;
    newsample(c700sqwave, len(c700sqwave), 9);
    tick(note(190,0,1,32000,0,127,ADSR+0xA,0xE0));
    endnote(2);
    while (1) {

    }
}

/*
    sf::RenderWindow window(sf::VideoMode({ 1280, 720 }), "SMPiano");
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SFML::Init(window, false);
    io.Fonts->Clear();
    ImFont* SNES = io.Fonts->AddFontFromFileTTF("SNES.ttf", 18);
    ImFont* REG = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/Arial.ttf", 11);
    SFML::UpdateFontTexture();
    sf::Clock deltaClock;
    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
#define event(e) event->is<sf::Event::##e>()
            if (event(Closed)) {
                window.close();
            }
        }


        SFML::Update(window, deltaClock.restart());

        SetNextWindowSize(ImVec2(window.getSize()));
        SetNextWindowPos(ImVec2(0,0));
        Begin("Main", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        //PushFont(SNES);
        if (Button("Delete System32")) print("System32 Deleted bwahahha >:3");
        if (Button("Load C700SQWAVE")) emu.newsample(c700sqwave, len(c700sqwave), 9);
        if (Button("Play")) {
            print("Note supposedly playing...");
        }
        //PopFont();

        //End
        End();
        window.clear();
        ImGui::SFML::Render(window);
        window.display();
    }
    ImGui::SFML::Shutdown();
*/

