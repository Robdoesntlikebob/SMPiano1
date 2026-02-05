#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sndEMU/dsp.h"
#include <SFML/Audio.hpp>

unsigned char BRR_SAWTOOTH[] = {
    0xB0, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0xB3, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
};

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

int main(int argc, char* argv[]) {
    // Allocate a DSP object
    SPC_DSP* dsp = spc_dsp_new();
    if (dsp == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate DSP");
        return 1;
    }

    // Allocate some RAM for it.
    char* aram = (char*)calloc(0x10000, sizeof(char)); // 64KB
    if (aram == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate ARAM");
        return 1;
    }

    // Connect the ARAM to the DSP.
    spc_dsp_init(dsp, aram);

    // Allocate an output buffer for audio samples.
    // The DSP generates stereo samples at 32kHz,
    // let's reserve room for a second of audio.
    int sample_count = 32000;
    spc_dsp_sample_t* output =
        (spc_dsp_sample_t*)calloc(2 * sample_count, sizeof(spc_dsp_sample_t));
    if (output == NULL) {
        fprintf(stderr, "%s\n", "Could not allocate output buffer");
        return 1;
    }

    // Connect the output buffer to the DSP.
    spc_dsp_set_output(dsp, output, sample_count);

    // Now everything's connected, turn it on.
    spc_dsp_reset(dsp);

    // Load our instrument into the beginning of ARAM.
    memcpy(aram, BRR_SAWTOOTH, sizeof(BRR_SAWTOOTH) / sizeof(unsigned char));

    // Set up an instrument table that tells the DSP
    // where to find the each instrument in ARAM.
    // The table has to start on an address that's a multiple of 0x100,
    // and since BRR_SAWTOOTH is a lot less than 0x100 bytes long,
    // we can put the table at 0x100.

    // There's only one entry in our table.
    aram[0x100] = 0x00; // sample start address low byte
    aram[0x101] = 0x00; // sample start address high byte
    aram[0x102] = 0x00; // sample loop address low byte
    aram[0x103] = 0x00; // sample loop address high byte

    // Tell the DSP that the table of samples starts at 0x0100
    spc_dsp_write(dsp, DIR, 0x01);

    // Tell the DSP to set the channel volume to max.
    spc_dsp_write(dsp, V0VOLL, 0x80);
    spc_dsp_write(dsp, V0VOLR, 0x80);

    // Tell the DSP to set the master volume to max.
    spc_dsp_write(dsp, MVOLL, 0x80);
    spc_dsp_write(dsp, MVOLR, 0x80);

    // Tell the DSP to unmute audio output.
    spc_dsp_write(dsp, FLG, 0x20);

    // Tell the DSP that voice 0 should play sample 0.
    spc_dsp_write(dsp, V0SRCN, 0);

    // Tell the DSP to play the sample at the original pitch.
    spc_dsp_write(dsp, V0PITCHL, 0x00);
    spc_dsp_write(dsp, V0PITCHH, 0x10);

    // Set up an ADSR envelope for this voice.
    // We don't want anything complicated,
    // just start at max volume and stay there until release.
    spc_dsp_write(dsp, V0ADSR1, 0x0F);
    spc_dsp_write(dsp, V0ADSR2, 0xE0);

    // Tell the DSP to activate voice 0.
    spc_dsp_write(dsp, KON, 0x01);

    // Every 32 clocks, a two-channel sample is written to the output buffer,
    // so if we run for 320 clocks, we should get 10 two-channel samples.
    // The DSP only checks for KON every other sample,
    // after KON it spends five samples doing setup work,
    // our instrument is 16 samples long,
    // and the DSP outputs a sample every 32 clocks.
    // So to see a full loop of our instrument:
    spc_dsp_run(dsp, 32000*32);

    // How many samples did we actually get?
    // Note that the library counts a two-channel sample as "two samples".
    int generated_count = spc_dsp_sample_count(dsp) / 2;
    printf("Generated %d samples\n", generated_count);

    // Display the samples the DSP produced
    for (int i = 0; i < generated_count; i++) {
        printf("%02d: %04hx %04hx\n", i, output[i * 2], output[1 + i * 2]);
    }
    printf("\n");

    sf::SoundBuffer buf(output, 32000, 2, 32000, { sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight });
    sf::Sound snd(buf);
    snd.play();
    sf::sleep(sf::seconds(0.5));
    spc_dsp_set_output(dsp, output, sample_count);
    spc_dsp_run(dsp, 32000 * 32);
    buf.loadFromSamples(output, 32000, 2, 32000, { sf::SoundChannel::FrontLeft,sf::SoundChannel::FrontRight });
    snd.setBuffer(buf);
    sf::sleep(sf::seconds(0.5));

    // We're all done!
    return 0;
}