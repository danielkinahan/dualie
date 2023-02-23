#include "daisy_seed.h"
#include "daisysp.h"
#include "desktop-synthesizer.h"

using namespace std;
using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

const int ControlPanelSize = 35;

int ControlPanel[ControlPanelSize] = {
    0, // Osc1Waveform
    0, // Osc1PulseWidth
    0, // Osc1FrequencyMod
    0, // Osc1PWMod
    0, // Osc2Waveform
    0, // Osc2PulseWidth
    0, // Osc2FrequencyMod
    0, // Osc2PWMod
    0, // Osc2TuneCents
    0, // Osc2TuneOctave
    0, // Osc2Sync
    0, // Noise
    64, // OscMix
    0, // OscSplit
    127, // FilterCutoff
    0, // FilterResonance
    0, // FilterLFOMod
    0, // FilterVelocityMod
    0, // FilterKeybedTrack
    0, // FilterAttack
    0, // FilterDecay
    127, // FilterSustain
    0, // FilterRelease
    0, // AmpAttack
    0, // AmpDecay
    127, // AmpSustain
    0, // AmpRelease
    0, // AmpLFOMod
    0, // LFOWaveform
    0, // LFOFrequency
    0, // LFOTempoSync
    0, // FXType
    0, // FXParam1
    0, // FXParam2
    0 // FXMix
};
    //Don't think I need to add preset controls here
    //TODO: Map the params to meaningful indeces of MIDI CC

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

    void SetParam(int param) 
    {
        //Process paramater value changed
        switch(param)
        {
            case CTRL_OSC1WAVEFORM:
                switch(ControlPanel[CTRL_OSC1WAVEFORM] / 32)
                {
                    case 0: osc1_.SetWaveform(osc1_.WAVE_SIN); break;
                    case 1: osc1_.SetWaveform(osc1_.WAVE_TRI); break;
                    case 2: osc1_.SetWaveform(osc1_.WAVE_SAW); break;
                    case 3: osc1_.SetWaveform(osc1_.WAVE_SQUARE); break;
                    default: break;
                }
            break;
            case CTRL_AMPATTACK: amp_env_.SetTime(ADENV_SEG_ATTACK, (ControlPanel[CTRL_AMPATTACK] / 32.f)); break;
            case CTRL_AMPDECAY: amp_env_.SetTime(ADENV_SEG_DECAY, (ControlPanel[CTRL_AMPDECAY] / 32.f)); break;
            // Stops working after sustain set to 0 and reset
            case CTRL_AMPSUSTAIN: amp_env_.SetSustainLevel(ControlPanel[CTRL_AMPSUSTAIN] / 127.f); break;
            case CTRL_AMPRELEASE: amp_env_.SetTime(ADSR_SEG_RELEASE, (ControlPanel[CTRL_AMPRELEASE] / 64.f));break;
            //TODO: Change this from linear to logarithmic scaling
            case CTRL_FILTERCUTOFF: filt_.SetFreq(ControlPanel[CTRL_FILTERCUTOFF] * 174); break;
            //Awful sounds at values past 0.95!
            case CTRL_FILTERRESONANCE: filt_.SetRes(ControlPanel[CTRL_FILTERRESONANCE] / 134.0f); break;
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

    void SetParam(int param)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetParam(param);
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
        ControlPanel[param] = ctrlValue;
    }
        else
    {
        ControlPanel[param] += ctrlValue;
    }

    if (param < 7)
    {
        mgr.SetParam(param);
    }
    else
    {
        switch(param)
        {
            case CTRL_LFOFREQUENCY:
            {
                lfo.SetFreq(ControlPanel[CTRL_LFOFREQUENCY] / 6.4f);
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
