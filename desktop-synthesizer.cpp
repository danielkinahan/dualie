#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

static DaisySeed hw;
static Oscillator osc;
static Encoder enc;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in, AudioHandle::InterleavingOutputBuffer out, size_t size)
{
    float sig;
    for(size_t i = 0; i < size; i += 2)
    {
        sig = osc.Process();

        // left out
        out[i] = sig;

        // right out
        out[i + 1] = sig;
    }
}

int main(void)
{
	float sampleRate;
	hw.Init();
	hw.StartLog();
	hw.SetAudioBlockSize(4); // number of samples handled per callback
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    sampleRate = hw.AudioSampleRate();
    osc.Init(sampleRate);
	
	Encoder enc;
  	enc.Init(hw.GetPin(0), hw.GetPin(2), hw.GetPin(1));

	int tone = 440;

	// Set parameters for oscillator
    osc.SetWaveform(osc.WAVE_SIN);
    osc.SetFreq(tone);
    osc.SetAmp(0);
	bool noteOn = false;

	hw.StartAudio(AudioCallback);

	while(1) {
		enc.Debounce();

    	tone += enc.Increment()*100;
		osc.SetFreq(tone);
		hw.PrintLine("%i", tone);

		if(enc.Pressed()){
			if(noteOn){
				noteOn = false;
				osc.SetAmp(0);
			}
			else{
				noteOn = true;
				osc.SetAmp(0.5);
			}
			hw.DelayMs(200);
		}
	}
}
