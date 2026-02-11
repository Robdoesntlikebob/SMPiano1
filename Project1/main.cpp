//written by: Jonny (Discord @jonnyptn), Rob (Discord @rob_doesnt_like_bob6962), Screwtape (Discord @screwtapello)

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include "sndEMU/SPC_DSP.h"
#include "stdc++.h"
#include "imgui.h"
#include "imgui-SFML.h"

enum g_regs {
	MVOLL = 0x0C,
	MVOLR = 0x1C,
	EVOLL = 0x2C,
	EVOLR = 0x3C,
	KON = 0x4C,
	KOF = 0x5C,
	FLG = 0x6C, //0x60
	ENDX = 0x7C,
	PMON = 0x2D,
	NON = 0x3D,
	EON = 0x4D,
	DIR = 0x5D, //0x67 (SIX SEVEN)
	ESA = 0x6D, //0x80
	EDL = 0x7D,
	ADSR = 128,
	GAIN = 0 << 8,
};

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
#define print(x) std::cout << x << std::endl;
#define CHANNELS { sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight }

struct SPC700 : public sf::SoundStream
{
private:
	std::array<unsigned char, 0x10000> aram{};
	unsigned dpos = 0;
	unsigned vxkon = 0;
	unsigned pos = 0x200;
public:
	SPC_DSP dsp{};
	std::array<SPC_DSP::sample_t, 2> buffer{};
	SPC700()
	{
		dsp.init(aram.data());
		initialize(2, 32000, CHANNELS);
		dsp.set_output(buffer.data(), buffer.size());
		dsp.reset();
		dsp.write(MVOLL, 127);
		dsp.write(MVOLR, 127);
		dsp.write(DIR, 0x67);
		dsp.write(FLG, 0x20);
		dsp.write(ESA, 0x80);
		dsp.write(EON, 0x00);
		dsp.write(NON, 0x00);
		dsp.write(PMON, 0x00);
	}
	bool onGetData(Chunk& data) override
	{
		dsp.set_output(buffer.data(), buffer.size());
		for (int i = 0; i < 8; i++) {
			//if (vxkon > 0) { dsp.write(vvoll(i), dsp.read(vvoll(i)) / vxkon); dsp.write(vvolr(i), dsp.read(vvolr(i)) / vxkon); }
			//else continue;
		}
		dsp.run(32 * (buffer.size() / 2));
		data.sampleCount = buffer.size();
		data.samples = buffer.data();
		return true;
		//}
	} void onSeek(sf::Time timeOffset) override {};

	void pitch(unsigned pitch, unsigned voice) {
		dsp.write(vpl(voice), lobit(pitch));
		dsp.write(vph(voice), hibit(pitch)&0x3f);
	}

	void newsample(unsigned char* sample, size_t len, unsigned loop) {
		memcpy(aram.data() + pos, sample, len);
		aram.data()[0x6700 + dpos] = lobit(pos); dpos++;
		aram.data()[0x6700 + dpos] = hibit(pos); dpos++;
		aram.data()[0x6700 + dpos] = lobit(loop+pos); dpos++;
		aram.data()[0x6700 + dpos] = hibit(loop+pos); dpos++;
		pos += len;
	}

	void note(unsigned voice, bool hz, unsigned p, unsigned srcn, unsigned voll, unsigned volr, unsigned adsr1, unsigned adsr2) {
		vxkon++;
		dsp.write(vsrcn(0), srcn);
		dsp.write(vvoll(0), voll);
		dsp.write(vvolr(0), volr);
		if (hz) pitch(pow(2, 12) * (p / 32000), voice);
		else pitch(p, voice);
		dsp.write(vadsr1(voice), adsr1);
		dsp.write(vadsr2(voice), adsr2);
		dsp.write(KON, 1 << voice);
		dsp.write(KOF, 0 << voice);
	}

	void note(unsigned voice, bool hz, unsigned p, unsigned srcn, unsigned vol, unsigned adsr1, unsigned adsr2) {
		vxkon++;
		dsp.write(vsrcn(0), srcn);
		dsp.write(vvoll(0), vol);
		dsp.write(vvolr(0), vol);
		if (hz) pitch(4096 * (p / 32000), voice);
		else pitch(p, voice);
		dsp.write(vadsr1(voice), adsr1);
		dsp.write(vadsr2(voice), adsr2);
		dsp.write(KON, 1 << voice);
		dsp.write(KOF, 0 << voice);
	}
	void endnote(unsigned voice) {
		dsp.write(KOF, 1 << voice);
		dsp.write(KON, 0 << voice);
	}
};

using namespace ImGui;
int main()
{
	SPC700 emu;
	emu.play();
	emu.newsample(c700sqwave, len(c700sqwave), 9);
	emu.newsample(BRR_SAWTOOTH, len(BRR_SAWTOOTH), 0);
	sf::RenderWindow window(sf::VideoMode({1280,720}), "SMPiano");
	ImGui::SFML::Init(window);

	sf::Clock dt;
	while (window.isOpen()) {
		/*necessary variables and theme*/
		ImGuiIO &io = ImGui::GetIO();
		ImGuiStyle &theme = ImGui::GetStyle();
		theme.FrameRounding = 0.0f;


		while (const auto event = window.pollEvent()) {
			SFML::ProcessEvent(window, *event);
#define event(e) event->is<sf::Event::##e>()
			if (event(Closed)) {
				window.close();
			}
		}
		SFML::Update(window, dt.restart());


		/*this is where the fun begins*/
		ImGui::SetNextWindowSize({ window.getSize() });
		ImGui::SetNextWindowPos({ 0,0 });
		ImGui::Begin("Main", 0, ImGuiWindowFlags_NoTitleBar bitor ImGuiWindowFlags_NoResize bitor ImGuiWindowFlags_NoMove);
		ImGui::Button("		C");
		if (IsItemActivated()) {
			emu.note(0, 0, 0x106e, 0, 128, ADSR + 10, 0xe0);
		}
		else if (IsItemDeactivated()) {
			emu.endnote(0);
		}
		ImGui::End();


		/*this is where it ends :broken_heart:*/
		window.clear();
		SFML::Render(window);
		window.display();
	}
	SFML::Shutdown();
	return 0;
}