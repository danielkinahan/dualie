#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

static DaisySeed      hw;
static Oscillator     osc;
static Encoder        enc;
static MidiUsbHandler midi;
static Adsr           env;
bool                  gate;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++){
        osc.SetAmp(env.Process(gate));
        out[0][i] = out[1][i] = osc.Process();
    }
}

int main(void)
{
    hw.Init();

    // Initialize USB Midi 
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    enc.Init(hw.GetPin(0), hw.GetPin(2), hw.GetPin(1));

    // Initialize test tone
    osc.Init(hw.AudioSampleRate());
    env.Init(hw.AudioSampleRate());

    int waveType = 0;
    float ctrlAmp = 0.5;
    float ctrlAttack = 0.1;
    float ctrlDecay = 0.35;
    float ctrlSustain = 0.25;
    float ctrlRelease = 0.01;
    float timeIncr = 0.05;
    float volIncr = 0.05;
    int fixedIncr = 1;
    int param = 0;

    env.SetTime(ADENV_SEG_ATTACK, ctrlAttack);
    env.SetTime(ADENV_SEG_DECAY, ctrlDecay);
    env.SetSustainLevel(ctrlSustain);
    env.SetTime(ADSR_SEG_RELEASE, ctrlRelease);

    osc.SetWaveform(osc.WAVE_SIN);
    osc.SetAmp(ctrlAmp);

    // start the audio callback
    hw.StartAudio(AudioCallback);
    while(1)
    {
        enc.Debounce();

        //Process parameter selection
        if(enc.Pressed())
        {
            param++;
            if (param >= 4){
                param = 0;
            }
            hw.DelayMs(300);
        }

        //Process paramater value changed
        int incrVal = enc.Increment();
        if( incrVal != 0){
            switch(param)
            {
                case 0:
                {
                    waveType += (incrVal * fixedIncr);
                    waveType = waveType % 4;
                    switch(waveType)
                    {
                        case 0:
                            osc.SetWaveform(osc.WAVE_SIN);
                            break;
                        case 1:
                            osc.SetWaveform(osc.WAVE_TRI);
                            break;
                        case 2:
                            osc.SetWaveform(osc.WAVE_SAW);
                            break;
                        case 3:
                            osc.SetWaveform(osc.WAVE_SQUARE);
                            break;
                    }
                }
                case 1:
                {
                    ctrlAttack += (incrVal * timeIncr);
                    env.SetTime(ADENV_SEG_ATTACK, ctrlAttack);
                }
                break;
                case 2:
                {
                    ctrlDecay += (incrVal * timeIncr);
                    env.SetTime(ADENV_SEG_DECAY, ctrlDecay);
                }
                break;
                case 3:
                {
                    if ((ctrlSustain>=ctrlAmp) & (incrVal>0))
                    {
                        break;
                    }
                    ctrlSustain += (incrVal * volIncr);
                    env.SetSustainLevel(ctrlSustain);
                }
                break;
                case 4:
                {
                    ctrlRelease += (incrVal * timeIncr);
                    env.SetTime(ADSR_SEG_RELEASE, ctrlRelease);
                }
                break;

                default: break;
            }
        }
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
                        gate = true;
                        env.Retrigger(false);
                    }
                    else
                    {
                        gate = false;
                    }
                }
                break;
                case NoteOff:
                {
                    gate = false;
                }
                break;

                default: break;
            }
        }

    }
}
