#include "daisy_seed.h"
#include "daisysp.h"
#include "desktop-synthesizer.h"

#include "oscillator.h"
#include "adsr.h"
#include "moogladder.h"
#include "whitenoise.h"
#include "arm_math.h"

using namespace std;
using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

static custom::Oscillator       lfo;

//Midi control values (0-127) which are stored in EEPROM as a preset
uint8_t ControlPanel[35] = {
    0, // Osc1Waveform
    127, // Osc1PulseWidth
    0, // Osc1FrequencyMod
    0, // Osc1PWMod
    0, // Osc2Waveform
    127, // Osc2PulseWidth
    0, // Osc2FrequencyMod
    0, // Osc2PWMod
    64, // Osc2TuneCents
    64, // Osc2TuneOctave
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
    2, // AmpAttack
    2, // AmpDecay
    127, // AmpSustain
    2, // AmpRelease
    0, // AmpLFOMod
    0, // LFOWaveform
    0, // LFOFrequency
    0, // LFOTempoSync
    0, // FXType
    0, // FXParam1
    0, // FXParam2
    0 // FXMix
};

//Computed values of controls in respective units
//These are computed outside of the Audio Callback for efficiency
float ValuePanel[35] = {
    0, // Osc1Waveform
    0.5, // Osc1PulseWidth
    0, // Osc1FrequencyMod
    0, // Osc1PWMod
    0, // Osc2Waveform
    0.5, // Osc2PulseWidth
    0, // Osc2FrequencyMod
    0, // Osc2PWMod
    0, // Osc2TuneCents
    0, // Osc2TuneOctave
    0, // Osc2Sync
    0, // Noise
    0.5, // OscMix
    0, // OscSplit
    40000, // FilterCutoff
    0, // FilterResonance
    0, // FilterLFOMod
    0, // FilterVelocityMod
    0, // FilterKeybedTrack
    0, // FilterAttack
    0, // FilterDecay
    1, // FilterSustain
    0, // FilterRelease
    0.0625, // AmpAttack
    0.0625, // AmpDecay
    1, // AmpSustain
    0.0625, // AmpRelease
    0, // AmpLFOMod
    0, // LFOWaveform
    0, // LFOFrequency
    0, // LFOTempoSync
    0, // FXType
    0, // FXParam1
    0, // FXParam2
    0 // FXMix
};
class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        osc1_.Init(samplerate);
        osc1_.SetWaveform(ValuePanel[CTRL_OSC1WAVEFORM]);
        osc1_.SetPw(ValuePanel[CTRL_OSC1PULSEWIDTH]);

        osc2_.Init(samplerate);
        osc2_.SetWaveform(ValuePanel[CTRL_OSC2WAVEFORM]);
        osc2_.SetPw(ValuePanel[CTRL_OSC2PULSEWIDTH]);

        noise_.Init();

        amp_env_.Init(samplerate);
        amp_env_.SetTime(ADENV_SEG_ATTACK, ValuePanel[CTRL_AMPATTACK]);
        amp_env_.SetTime(ADENV_SEG_DECAY, ValuePanel[CTRL_AMPDECAY]);
        amp_env_.SetSustainLevel(ValuePanel[CTRL_AMPSUSTAIN]);
        amp_env_.SetTime(ADSR_SEG_RELEASE, ValuePanel[CTRL_AMPRELEASE]);

        filt_.Init(samplerate);
        filt_.SetFreq(ValuePanel[CTRL_FILTERCUTOFF]);
        filt_.SetRes(ValuePanel[CTRL_FILTERRESONANCE]);
    }

    void ProcessBlock(float *buf, size_t size)
    {
        float amp_out[size], osc1_out[size], osc2_out[size];
        amp_env_.ProcessBlock(amp_out, size, env_gate_);
        
        osc1_.ProcessBlock(osc1_out, size);
        osc2_.ProcessBlock(osc2_out, size);

        arm_add_f32(osc1_out, osc2_out, buf, size);
        arm_mult_f32(buf, amp_out, buf, size);
    }

    float Process()
    {
        /*TODO: Optimize branching
                refactor DSP code to process 16 blocks at a time
                barrel shift
        */
        float sig, amp, lfo_out;
        amp = amp_env_.Process(env_gate_); //change to account for both envelopes
        if(!amp_env_.IsRunning())
        {
            return 0;
        }

        lfo_out = lfo.Process();

        osc1_.SetAmp(0);
        osc2_.SetAmp(0);

        if (!ValuePanel[CTRL_OSCSPLIT] || note_ < 64 )
        {
            osc1_.SetAmp(velocity_ * (1-ValuePanel[CTRL_OSCMIX]));
            osc1_.SetPw(ValuePanel[CTRL_OSC1PULSEWIDTH] 
                            + (lfo_out * ValuePanel[CTRL_OSC1PWMOD] 
                            * (0.5-ValuePanel[CTRL_OSC1PULSEWIDTH])));
            osc1_.PhaseAdd(lfo_out * ValuePanel[CTRL_OSC1FREQUENCYMOD]);  //Not sure if PhaseAdd is the best way to do frequency modulation
        }

        if (!ValuePanel[CTRL_OSCSPLIT] || note_ > 63 )
        {
            osc2_.SetAmp(velocity_ * ValuePanel[CTRL_OSCMIX]);
            osc2_.SetPw(ValuePanel[CTRL_OSC2PULSEWIDTH] 
                            + (lfo_out * ValuePanel[CTRL_OSC2PWMOD] 
                            * (0.5-ValuePanel[CTRL_OSC2PULSEWIDTH])));
            osc2_.SetFreq(mtof(note_ + ValuePanel[CTRL_OSC2TUNECOARSE] + ValuePanel[CTRL_OSC2TUNEFINE]));
            osc2_.PhaseAdd(lfo_out * ValuePanel[CTRL_OSC2FREQUENCYMOD]);
            if(ValuePanel[CTRL_OSC2SYNC] && osc1_.IsEOC())
            {
                osc2_.Reset();
            }
        }

        noise_.SetAmp(velocity_ * ValuePanel[CTRL_NOISE]);

        sig = osc1_.Process() + osc2_.Process() + noise_.Process();

        //doesn't sound very good
        //filt_.SetFreq(ValuePanel[CTRL_FILTERCUTOFF] - (ValuePanel[CTRL_FILTERLFOMOD] * lfo_out * ValuePanel[CTRL_FILTERCUTOFF]));

        return filt_.Process(sig * amp);
    }

    void OnNoteOn(uint8_t note, uint8_t velocity)
    {
        note_     = note;
        velocity_ = velocity / 127.f;
        osc1_.SetFreq(mtof(note_));
        osc2_.SetFreq(mtof(note_));
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetParam(int param) 
    {
        //Process paramater value changed
        //TODO: investigate ways to calculate without using expensive divisions
        switch(param)
        {
            case CTRL_OSC1WAVEFORM: 
                ValuePanel[CTRL_OSC1WAVEFORM] = ControlPanel[CTRL_OSC1WAVEFORM] / 16;
                osc1_.SetWaveform(ValuePanel[CTRL_OSC1WAVEFORM]); 
                break;
            case CTRL_OSC1PULSEWIDTH:
                ValuePanel[CTRL_OSC1PULSEWIDTH] = ControlPanel[CTRL_OSC1PULSEWIDTH] / 254.f;
                break;
            case CTRL_OSC1FREQUENCYMOD:
                //change the scaling on this
                ValuePanel[CTRL_OSC1FREQUENCYMOD] = ControlPanel[CTRL_OSC1FREQUENCYMOD] / 127.f;
                break;
            case CTRL_OSC1PWMOD:
                ValuePanel[CTRL_OSC1PWMOD] = ControlPanel[CTRL_OSC1PWMOD] / 127.f;
                break;
            case CTRL_OSC2WAVEFORM:
                ValuePanel[CTRL_OSC2WAVEFORM] = ControlPanel[CTRL_OSC2WAVEFORM] / 16;
                osc2_.SetWaveform(ValuePanel[CTRL_OSC2WAVEFORM]); 
                break;
            case CTRL_OSC2FREQUENCYMOD:
                //change the scaling on this
                ValuePanel[CTRL_OSC2FREQUENCYMOD] = ControlPanel[CTRL_OSC2FREQUENCYMOD] / 127.f;
                break;
            case CTRL_OSC2PULSEWIDTH:
                ValuePanel[CTRL_OSC2PULSEWIDTH] = ControlPanel[CTRL_OSC2PULSEWIDTH] / 254.f;
                break;
            case CTRL_OSC2PWMOD:
                ValuePanel[CTRL_OSC2PWMOD] = ControlPanel[CTRL_OSC2PWMOD] / 127.f;
                break;
            case CTRL_OSC2TUNEFINE:
                ValuePanel[CTRL_OSC2TUNEFINE] = ((ControlPanel[CTRL_OSC2TUNEFINE] / 64.f) - 1); //cents
                break;
            case CTRL_OSC2TUNECOARSE:
                ValuePanel[CTRL_OSC2TUNECOARSE] = ((int) (ControlPanel[CTRL_OSC2TUNECOARSE] / 2.646)) - 24.18; //semitones
                break;
            case CTRL_OSC2SYNC:
                ValuePanel[CTRL_OSC2SYNC] = ControlPanel[CTRL_OSC2SYNC] ? 1 : 0;
                break;
            case CTRL_NOISE:
                ValuePanel[CTRL_NOISE] = ControlPanel[CTRL_NOISE] / 127.f;
                break;
            case CTRL_OSCMIX:
                ValuePanel[CTRL_OSCMIX] = ControlPanel[CTRL_OSCMIX] / 127.f;
            case CTRL_OSCSPLIT:
                ValuePanel[CTRL_OSCSPLIT] = ControlPanel[CTRL_OSCSPLIT] ? 1 : 0;
                break;
            case CTRL_FILTERCUTOFF:
                //Use a Michaelis-Menten equation to scale frequency y = (-606.0853*x)/(-130.4988 + x)
                ValuePanel[CTRL_FILTERCUTOFF] = (ControlPanel[CTRL_FILTERCUTOFF] * -606.0853) / (ControlPanel[CTRL_FILTERCUTOFF] - 130.4988);
                filt_.SetFreq(ValuePanel[CTRL_FILTERCUTOFF]); 
                break;
            case CTRL_FILTERRESONANCE:
                ValuePanel[CTRL_FILTERRESONANCE] = ControlPanel[CTRL_FILTERRESONANCE] / 134.0f;
                filt_.SetRes(ValuePanel[CTRL_FILTERRESONANCE]); 
                break;
            case CTRL_FILTERLFOMOD:
                ValuePanel[CTRL_FILTERLFOMOD] = ControlPanel[CTRL_FILTERLFOMOD] / 127.f;
                break;
            case CTRL_AMPATTACK:
                ValuePanel[CTRL_AMPATTACK] = ControlPanel[CTRL_AMPATTACK] / 32.f;
                amp_env_.SetTime(ADENV_SEG_ATTACK, (ValuePanel[CTRL_AMPATTACK])); 
                break;
            case CTRL_AMPDECAY:
                ValuePanel[CTRL_AMPDECAY] = ControlPanel[CTRL_AMPDECAY] / 32.f;
                amp_env_.SetTime(ADENV_SEG_DECAY, (ValuePanel[CTRL_AMPDECAY])); 
                break;
            case CTRL_AMPSUSTAIN:
                ValuePanel[CTRL_AMPSUSTAIN] = ControlPanel[CTRL_AMPSUSTAIN] / 127.f;
                amp_env_.SetSustainLevel(ValuePanel[CTRL_AMPSUSTAIN]); 
                break;
            case CTRL_AMPRELEASE: 
                ValuePanel[CTRL_AMPRELEASE] = ControlPanel[CTRL_AMPRELEASE] / 64.f;
                amp_env_.SetTime(ADSR_SEG_RELEASE, ValuePanel[CTRL_AMPRELEASE]); 
                break;
            default: break;
        }
    }

    inline bool  IsActive() const { return amp_env_.IsRunning(); }
    inline float GetNote() const { return note_; }

  private:
    custom::Oscillator osc1_;
    custom::Oscillator osc2_;
    custom::WhiteNoise noise_;
    custom::MoogLadder filt_;
    custom::Adsr       filt_env_;
    custom::Adsr       amp_env_;
    uint8_t            note_;
    float              velocity_;
    bool               env_gate_;
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

    void ProcessBlock(float *buf, size_t size)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            float temp[size];
            voices[i].ProcessBlock(temp, size);
            arm_add_f32(buf, temp, buf, size);
        }
    }

    void OnNoteOn(uint8_t notenumber, uint8_t velocity)
    {
        Voice *v = FindFreeVoice();
        if(v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(uint8_t notenumber, uint8_t velocity)
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

    uint8_t GetNumActiveVoices()
    {
        uint8_t count = 0;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(voices[i].IsActive())
            {
                count++;
            }
        }
        return count;
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
static Encoder          enc;
static MidiUsbHandler   midi;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = out[1][i] = mgr.Process() * 0.1;
    }
}

void AudioCallbackBlock(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    float buf[size];
    mgr.ProcessBlock(buf, size);

    arm_scale_f32(buf, 0.1, buf, size);

    out[0] = out[1] = buf;
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

    if (param < 27)
    {
        mgr.SetParam(param);
    }
    else
    {
        switch(param)
        {
            case CTRL_LFOFREQUENCY:
                ValuePanel[CTRL_LFOFREQUENCY] = ControlPanel[CTRL_LFOFREQUENCY] / 6.4f;
                lfo.SetFreq(ValuePanel[CTRL_LFOFREQUENCY]);
                break;

            default: break;
        }
    }
}

int main(void)
{
    hw.Init(true);
    //hw.SetAudioBlockSize(16);

    // Initialize USB Midi 
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;

    midi.Init(midi_cfg);
    enc.Init(hw.GetPin(0), hw.GetPin(2), hw.GetPin(1));
    mgr.Init(hw.AudioSampleRate());
    lfo.Init(hw.AudioSampleRate());

    uint8_t param = 0;

    lfo.SetAmp(1);
    lfo.SetWaveform(Oscillator::WAVE_SIN);
    //TODO: Change this from linear to logarithmic scaling
    lfo.SetFreq(0);

    // start the audio callback
    hw.StartAudio(AudioCallbackBlock);
    while(1)
    {
        //enc.Debounce();
        //Process parameter selection
        if(enc.Pressed())
        {
            param++;
            if (param >= 35){
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
