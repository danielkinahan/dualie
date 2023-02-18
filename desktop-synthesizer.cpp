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

int waveType = 0;
float ctrlAmp = 0.5;
int ctrlAttack = 12;
int ctrlDecay = 30;
int ctrlSustain = 25;
int ctrlRelease = 10;

//int timeIncr = 0.05;
//int volIncr = 0.05;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++){
        osc.SetAmp(env.Process(gate));
        out[0][i] = out[1][i] = osc.Process();
    }
}

void HandleControls(int ctrlValue, int param, bool midiCC)
{
    //Process paramater value changed
    switch(param)
    {
        case 0:
        {
            if (midiCC){
                waveType = ctrlValue;
            }
            else
            {
                waveType += ctrlValue;
            }
            
            switch(waveType / 32)
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
            if (midiCC){
                ctrlAttack = ctrlValue;
            }
            else
            {
                ctrlAttack += ctrlValue;
            }
            env.SetTime(ADENV_SEG_ATTACK, (ctrlAttack / 32.f));
        }
        break;
        case 2:
        {
            if (midiCC){
                ctrlDecay = ctrlValue;
            }
            else
            {
                ctrlDecay += ctrlValue;
            }
            env.SetTime(ADENV_SEG_DECAY, (ctrlDecay / 32.f));
        }
        break;
        case 3:
        {
            if (midiCC){
                ctrlSustain = ctrlValue;
            }
            else
            {
                ctrlSustain += ctrlValue;
            }
            env.SetSustainLevel(ctrlSustain / 127.f);
        }
        break;
        case 4:
        {
            if (midiCC){
                ctrlRelease = ctrlValue;
            }
            else
            {
                ctrlRelease += ctrlValue;
            }
            env.SetTime(ADSR_SEG_RELEASE, (ctrlRelease / 64.f));
        }
        break;

        default: break;
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

    //int fixedIncr = 1;
    int param = 0;

    env.SetTime(ADENV_SEG_ATTACK, ctrlAttack / 32.f);
    env.SetTime(ADENV_SEG_DECAY, ctrlDecay / 32.f);
    env.SetSustainLevel(ctrlSustain / 127.f);
    env.SetTime(ADSR_SEG_RELEASE, ctrlRelease / 64.f);

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
        if( enc.Increment() != 0)
        {
            HandleControls(enc.Increment(), param, false);
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

                case ControlChange:
                {
                    auto ctrl_msg = msg.AsControlChange();
                    HandleControls(ctrl_msg.value, ctrl_msg.control_number, true);
                }
                break;
                

                default: break;
            }
        }

    }
}
