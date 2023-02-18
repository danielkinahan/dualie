#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

static DaisySeed hw;
static Oscillator osc;
static Encoder enc;
static MidiUsbHandler midi;
static AdEnv env;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    float env_out;
    for(size_t i = 0; i < size; i++){
        
        env_out = env.Process();
        osc.SetAmp(env_out);
    
        out[0][i] = out[1][i] = osc.Process();
    }
}

int main(void)
{
    hw.Init();
    hw.StartLog();
    hw.PrintLine("Hello world!");

    // Initialize USB Midi 
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    enc.Init(hw.GetPin(0), hw.GetPin(2), hw.GetPin(1));

    // Initialize test tone
    osc.Init(hw.AudioSampleRate());
    env.Init(hw.AudioSampleRate());

    env.SetTime(ADENV_SEG_ATTACK, 0.15);
    env.SetTime(ADENV_SEG_DECAY, 0.35);
    env.SetMin(0.0);
    env.SetMax(0.25);
    env.SetCurve(0); // linear

    osc.SetWaveform(osc.WAVE_TRI);
    osc.SetFreq(220);

    float decay = 0.35;
    float timeIncr = 0.05;

    // start the audio callback
    hw.StartAudio(AudioCallback);
    while(1)
    {
        enc.Debounce();
        int incrVal = enc.Increment();
        if( incrVal != 0){
            decay += (incrVal * timeIncr);
        }
        env.SetTime(ADENV_SEG_DECAY, decay);
        // Listen to MIDI
        midi.Listen();

        // When message waiting
        while(midi.HasEvents())
        {
            // Take oldest one
            auto msg = midi.PopEvent();
            switch(msg.type)
            {
                case NoteOn:
                {
                    // change freq of oscillator
                    auto note_msg = msg.AsNoteOn();
                    if(note_msg.velocity != 0){
                        osc.SetFreq(mtof(note_msg.note));
                        env.Trigger();
                    }
                }
                break;
                    // Since we only care about note-on messages in this example
                    // we'll ignore all other message types
                default: break;
            }
        }
    }
}
