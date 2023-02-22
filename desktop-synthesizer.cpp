#include "daisy_seed.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

int waveType = 0;
float ctrlAmp = 0.75;
int ctrlAttack = 12;
int ctrlDecay = 30;
int ctrlSustain = 25;
int ctrlRelease = 10;
int ctrlFilterFreq = 100;
int ctrlFilterRes = 10;
int ctrlLFOFreq = 20;

struct Control
{
    std::string name;
    int value;
};

Control ControlPanel[20] = {
    {"waveType", 0},
    {"ctrlAttack", 12},
    {"ctrlDecay", 30},
    {"ctrlSustain", 25},
    {"ctrlRelease", 10},
    {"ctrlFilterFreq", 100},
    {"ctrlFilterRes", 10},
    {"ctrlLFOFreq", 20}
};

class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(ctrlAmp);
        osc_.SetWaveform(Oscillator::WAVE_SIN);

        env_.Init(samplerate);
        env_.SetTime(ADSR_SEG_ATTACK, ctrlAttack / 32.0f);
        env_.SetTime(ADSR_SEG_DECAY, ctrlDecay / 32.0f);
        env_.SetSustainLevel(ctrlSustain / 128.0f);
        env_.SetTime(ADSR_SEG_RELEASE, ctrlRelease / 64.0f);
    }

    float Process()
    {
        if(active_)
        {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if(!env_.IsRunning())
                active_ = false;
            sig = osc_.Process();
            return sig * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
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
            {
                switch(ControlPanel[param].value / 32)
                {
                    case 0:
                        osc_.SetWaveform(osc_.WAVE_SIN);
                        break;
                    case 1:
                        osc_.SetWaveform(osc_.WAVE_TRI);
                        break;
                    case 2:
                        osc_.SetWaveform(osc_.WAVE_SAW);
                        break;
                    case 3:
                        osc_.SetWaveform(osc_.WAVE_SQUARE);
                        break;
                }
            }
            case 1:
            {
                env_.SetTime(ADENV_SEG_ATTACK, (ControlPanel[param].value / 32.f));
            }
            break;
            case 2:
            {
                env_.SetTime(ADENV_SEG_DECAY, (ControlPanel[param].value / 32.f));
            }
            break;
            case 3:
            {
                env_.SetSustainLevel(ControlPanel[param].value / 127.f);
            }
            break;
            case 4:
            {
                env_.SetTime(ADSR_SEG_RELEASE, (ControlPanel[param].value / 64.f));
            }
            break;

            default: break;
        }
    }

    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc_;
    Adsr       env_;
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
static MoogLadder       flt;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    float freq, modulated_freq;
    for(size_t i = 0; i < size; i++)
    {
        freq = ctrlFilterFreq * 174;
        modulated_freq = freq + (lfo.Process() * freq);
        flt.SetFreq(modulated_freq);
        
        out[0][i] = out[1][i] = flt.Process(mgr.Process() * 0.5f);
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

    if (param < 5)
    {
        mgr.SetParam(ControlPanel[param].value, param);
    }
    else
    {
        switch(param)
        {
            case 5:
            {
                //flt.SetFreq(ControlPanel[param].value * 174);
            }
            break;
            case 6:
            {
                flt.SetRes(ControlPanel[param].value / 134.0f);
            }
            break;
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
    flt.Init(hw.AudioSampleRate());
    lfo.Init(hw.AudioSampleRate());

    int param = 0;

    //TODO: Change this from linear to logarithmic scaling
    flt.SetFreq(ctrlFilterFreq * 174);
    //Awful sounds at values past 0.95!
    flt.SetRes(ctrlFilterRes / 134.0f);

    lfo.SetAmp(1);
    lfo.SetWaveform(Oscillator::WAVE_SIN);
    //TODO: Change this from linear to logarithmic scaling
    lfo.SetFreq(ctrlLFOFreq / 6.4f);

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
