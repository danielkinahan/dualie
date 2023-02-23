#include "daisy_seed.h"
#include "daisysp.h"
#include "desktop-synthesizer.h"

using namespace std;
using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

class Control {
    public:
        Control(const string& name, int value) : name(name), value(value) {}
        string name;
        int value;
};

Control ControlPanel[] = {
    {"Osc1Waveform", 0}, //0
    {"Osc1PulseWidth", 0},
    {"Osc1FrequencyMod", 0},
    {"Osc1PWMod", 0},
    {"Osc2Waveform", 0},
    {"Osc2PulseWidth", 0}, //5
    {"Osc2FrequencyMod", 0},
    {"Osc2PWMod", 0},
    {"Osc2TuneCents", 0},
    {"Osc2TuneOctave", 0},
    {"Osc2Sync", 0}, //10
    {"Noise", 0},
    {"OscMix", 64},
    {"OscSplit", 0},
    {"FilterCutoff", 127},
    {"FilterResonance", 0}, //15
    {"FilterLFOMod", 0},
    {"FilterVelocityMod", 0},
    {"FilterKeybedTrack", 0},
    {"FilterAttack", 0},
    {"FilterDecay", 0}, //20
    {"FilterSustain", 127},
    {"FilterRelease", 0},
    {"AmpAttack", 0},
    {"AmpDecay", 0},
    {"AmpSustain", 127}, //25
    {"AmpRelease", 0},
    {"AmpLFOMod", 0},
    {"LFOWaveform", 0},
    {"LFOFrequency", 0},
    {"LFOTempoSync", 0}, //30
    {"FXType", 0},
    {"FXParam1", 0},
    {"FXParam2", 0},
    {"FXMix", 0} //34
    //Don't think I need to add preset controls here
    //TODO: Map the params to meaningful indeces of MIDI CC
};
class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        osc1_.Init(samplerate);
        osc1_.SetAmp(0.5);
        osc1_.SetWaveform(Oscillator::WAVE_SIN);
        osc1_.SetPw(0.5);

        osc2_.Init(samplerate);
        osc2_.SetAmp(0.5);
        osc2_.SetWaveform(Oscillator::WAVE_SIN);
        osc2_.SetPw(0.5);

        amp_env_.Init(samplerate);
        amp_env_.SetTime(ADSR_SEG_ATTACK, 0);
        amp_env_.SetTime(ADSR_SEG_DECAY, 0);
        amp_env_.SetSustainLevel(1);
        amp_env_.SetTime(ADSR_SEG_RELEASE, 0);

        filt_.Init(samplerate);
        filt_.SetFreq(samplerate / 3);
        filt_.SetRes(0);
        //filt_.SetDrive(0.8f);
    }

    float Process()
    {
        if(active_)
        {
            float sig, amp;
            amp = amp_env_.Process(env_gate_); //change to account for both envelopes
            if(!amp_env_.IsRunning())
                active_ = false;
            sig = osc1_.Process(); //add other oscillator
            filt_.Process(sig);
            return filt_.Low() * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        osc1_.SetFreq(mtof(note_));
        active_   = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetParam(int ctrlValue, int param) 
    {
        //Process paramater value changed
        switch(param)
        {
            case 0:
                switch(ControlPanel[param].value / 32)
                {
                    case 0: osc1_.SetWaveform(osc1_.WAVE_SIN); break;
                    case 1: osc1_.SetWaveform(osc1_.WAVE_TRI); break;
                    case 2: osc1_.SetWaveform(osc1_.WAVE_SAW); break;
                    case 3: osc1_.SetWaveform(osc1_.WAVE_SQUARE); break;
                    default: break;
                }
            break;
            case 23: amp_env_.SetTime(ADENV_SEG_ATTACK, (ControlPanel[param].value / 32.f)); break;
            case 24: amp_env_.SetTime(ADENV_SEG_DECAY, (ControlPanel[param].value / 32.f)); break;
            // Stops working after sustain set to 0 and reset
            case 25: amp_env_.SetSustainLevel(ControlPanel[param].value / 127.f); break;
            case 26: amp_env_.SetTime(ADSR_SEG_RELEASE, (ControlPanel[param].value / 64.f));break;
            //TODO: Change this from linear to logarithmic scaling
            case 14: filt_.SetFreq(ControlPanel[param].value * 174); break;
            //Awful sounds at values past 0.95!
            case 15: filt_.SetRes(ControlPanel[param].value / 134.0f); break;
            default: break;
        }
    }

    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc1_;
    Oscillator osc2_;
    Svf        filt_;
    Adsr       filt_env_;
    Adsr       amp_env_;
    float      note_, velocity_;
    bool       active_;
    bool       env_gate_;
};

template <size_t max_voices>
class VoiceManager
{
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float samplerate)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].Init(samplerate);
        }
    }

    float Process()
    {
        float sum;
        sum = 0.f;
        for(size_t i = 0; i < max_voices; i++)
        {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity)
    {
        Voice *v = FindFreeVoice();
        if(v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            Voice *v = &voices[i];
            if(v->IsActive() && v->GetNote() == notenumber)
            {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices()
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].OnNoteOff();
        }
    }

    void SetParam(int ctrlValue, int param)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetParam(ctrlValue, param);
        }
    }


  private:
    Voice  voices[max_voices];
    Voice *FindFreeVoice()
    {
        Voice *v = NULL;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(!voices[i].IsActive())
            {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};

static VoiceManager<24> mgr;
static DaisySeed        hw;
static Oscillator       lfo;
static Encoder          enc;
static MidiUsbHandler   midi;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    //float osc_out, lfo_out;
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = out[1][i] = mgr.Process() * 0.5f;
    }
}

void HandleControls(int ctrlValue, int param, bool midiCC)
{
    //Process paramater value changed
    if (midiCC)
    {
        ControlPanel[param].value = ctrlValue;
    }
        else
    {
        ControlPanel[param].value += ctrlValue;
    }

    if (param < 7)
    {
        mgr.SetParam(ControlPanel[param].value, param);
    }
    else
    {
        switch(param)
        {
            case 7:
            {
                lfo.SetFreq(ControlPanel[param].value / 6.4f);
            }
            break;

            default: break;
        }
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
    mgr.Init(hw.AudioSampleRate());
    lfo.Init(hw.AudioSampleRate());

    int param = 0;

    lfo.SetAmp(1);
    lfo.SetWaveform(Oscillator::WAVE_SIN);
    //TODO: Change this from linear to logarithmic scaling
    lfo.SetFreq(0);

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
                    if(note_msg.velocity != 0)
                    {
                        mgr.OnNoteOn(note_msg.note, note_msg.velocity);
                    }
                    else
                    {
                        mgr.OnNoteOff(note_msg.note, note_msg.velocity);
                    }
                }
                break;

                case NoteOff:
                {
                    auto note_msg = msg.AsNoteOn();
                    mgr.OnNoteOff(note_msg.note, note_msg.velocity);
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
