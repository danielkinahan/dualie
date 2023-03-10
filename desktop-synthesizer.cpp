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

#define BLOCK_SIZE 16

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
    void Init(float sample_rate)
    {
        osc1_.Init(sample_rate);
        osc2_.Init(sample_rate);
        noise_.Init();
        amp_env_.Init(sample_rate);
        filt_env_.Init(sample_rate);
        filt_.Init(sample_rate);
    }

    void ProcessBlock(float *buf, float *pw1_out, float *pw2_out, 
                        float *fm1_out, float *fm2_out, 
                        float *filt_lfo, float *amp_lfo, size_t size)
    {
        float osc1_out[BLOCK_SIZE], osc2_out[BLOCK_SIZE], noise_out[BLOCK_SIZE], 
            filt_freq[BLOCK_SIZE], filt_env_out[BLOCK_SIZE], filt_mod[BLOCK_SIZE], 
            amp_out[BLOCK_SIZE], amp_env_out[BLOCK_SIZE], reset_vector[BLOCK_SIZE];
        float velocity_freq, kbd_freq;

        //Process osc1, resets disabled
        osc1_.ProcessBlock(osc1_out, pw1_out, fm1_out, reset_vector, false, BLOCK_SIZE);

        //Adjust tuning
        osc2_.SetFreq(mtof(note_ + ValuePanel[CTRL_OSC2TUNECOARSE] + ValuePanel[CTRL_OSC2TUNEFINE]));

        //Process osc2 with reset value set to ValuePanel[CTRL_OSC2SYNC]
        osc2_.ProcessBlock(osc2_out, pw2_out, fm2_out, reset_vector, ValuePanel[CTRL_OSC2SYNC], BLOCK_SIZE);

        //If CTRL_OSCSPLIT enabled, silence each oscillator on oposite sides
        arm_scale_f32(osc1_out, split_high_, osc1_out, BLOCK_SIZE);
        arm_scale_f32(osc2_out, split_low_, osc2_out, BLOCK_SIZE);

        //Noise
        noise_.ProcessBlock(noise_out, BLOCK_SIZE);

        //Mixer
        arm_scale_f32(osc1_out, (1-ValuePanel[CTRL_OSCMIX]), osc1_out, BLOCK_SIZE);
        arm_scale_f32(osc2_out, ValuePanel[CTRL_OSCMIX], osc2_out, BLOCK_SIZE);
        arm_add_f32(osc1_out, osc2_out, buf, BLOCK_SIZE);
        arm_scale_f32(noise_out, ValuePanel[CTRL_NOISE], noise_out, BLOCK_SIZE);
        arm_add_f32(buf, noise_out, buf, BLOCK_SIZE);

        //Filter
        arm_fill_f32(ValuePanel[CTRL_FILTERCUTOFF], filt_freq, BLOCK_SIZE);
        //Note filter modulated by Envelope, Velocity and Keybed
        //Velocity and keybed can add to the cutoff frequency
        //Velocity - add 20khz * (velocity mod * velocity)
        velocity_freq = 20000.f * ValuePanel[CTRL_FILTERVELOCITYMOD] * velocity_;
        //Keybed - leaving this simple for now will refine later
        kbd_freq = freq_ * ValuePanel[CTRL_FILTERKEYBEDTRACK];
        //Add them to existing cutoff
        arm_offset_f32(filt_freq, velocity_freq+kbd_freq, filt_freq, BLOCK_SIZE);
        //Calculate filter envelope
        filt_env_.ProcessBlock(filt_env_out, BLOCK_SIZE, env_gate_);
        arm_mult_f32(filt_lfo, filt_env_out, filt_mod, BLOCK_SIZE);
        arm_mult_f32(filt_mod, filt_freq, filt_freq, BLOCK_SIZE);
        filt_.ProcessBlock(buf, filt_freq, BLOCK_SIZE);

        //Amplifier
        amp_env_.ProcessBlock(amp_env_out, BLOCK_SIZE, env_gate_);
        arm_mult_f32(amp_env_out, amp_lfo, amp_out, BLOCK_SIZE);
        arm_scale_f32(amp_out, velocity_, amp_out, BLOCK_SIZE);
        arm_mult_f32(buf, amp_out, buf, BLOCK_SIZE);
    }

    float Process()
    {
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
        freq_ = mtof(note_);
        osc1_.SetFreq(freq_);
        osc2_.SetFreq(freq_);
        env_gate_ = true;
        split_high_ = !ValuePanel[CTRL_OSCSPLIT] || note_ > 63;
        split_low_ = !ValuePanel[CTRL_OSCSPLIT] || note_ < 64;
        //Get envelope started so we can check if its active right away
        //amp_env_.Process(env_gate_);
        //filt_env_.Process(env_gate_);
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetParam(int param) 
    {
        //Process paramater value changed
        //TODO: investigate ways to calculate without using expensive divisions
        //TODO: remove unecessary controls from the voice class and move to voice mgr
        switch(param)
        {
            case CTRL_OSC1WAVEFORM: 
                //Disabling polybleps until I optimize them
                ValuePanel[CTRL_OSC1WAVEFORM] = ControlPanel[CTRL_OSC1WAVEFORM] / 26;
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
                ValuePanel[CTRL_OSC2WAVEFORM] = ControlPanel[CTRL_OSC2WAVEFORM] / 26;
                osc2_.SetWaveform(ValuePanel[CTRL_OSC2WAVEFORM]); 
                break;
            case CTRL_OSC2PULSEWIDTH:
                ValuePanel[CTRL_OSC2PULSEWIDTH] = ControlPanel[CTRL_OSC2PULSEWIDTH] / 254.f;
                break;
            case CTRL_OSC2FREQUENCYMOD:
                ValuePanel[CTRL_OSC2FREQUENCYMOD] = ControlPanel[CTRL_OSC2FREQUENCYMOD] / 127.f;
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
            case CTRL_FILTERVELOCITYMOD:
                ValuePanel[CTRL_FILTERVELOCITYMOD] = ControlPanel[CTRL_FILTERVELOCITYMOD] / 127.f;
                break;
            case CTRL_FILTERKEYBEDTRACK:
                ValuePanel[CTRL_FILTERKEYBEDTRACK] = ControlPanel[CTRL_FILTERKEYBEDTRACK] / 127.f;
                break;
            case CTRL_FILTERATTACK:
                ValuePanel[CTRL_FILTERATTACK] = ControlPanel[CTRL_FILTERATTACK] / 32.f;
                filt_env_.SetTime(ADENV_SEG_ATTACK, (ValuePanel[CTRL_FILTERATTACK])); 
                break;
            case CTRL_FILTERDECAY:
                ValuePanel[CTRL_FILTERDECAY] = ControlPanel[CTRL_FILTERDECAY] / 32.f;
                filt_env_.SetTime(ADENV_SEG_DECAY, (ValuePanel[CTRL_FILTERDECAY])); 
                break;
            case CTRL_FILTERSUSTAIN:
                ValuePanel[CTRL_FILTERSUSTAIN] = ControlPanel[CTRL_FILTERSUSTAIN] / 127.f;
                filt_env_.SetSustainLevel(ValuePanel[CTRL_FILTERSUSTAIN]); 
                break;
            case CTRL_FILTERRELEASE: 
                ValuePanel[CTRL_FILTERRELEASE] = ControlPanel[CTRL_FILTERRELEASE] / 64.f;
                filt_env_.SetTime(ADSR_SEG_RELEASE, ValuePanel[CTRL_FILTERRELEASE]); 
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
            case CTRL_AMPLFOMOD:
                ValuePanel[CTRL_AMPLFOMOD] = ControlPanel[CTRL_AMPLFOMOD] / 127.f;
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
    float              velocity_, freq_;
    bool               env_gate_, split_high_, split_low_;
};

template <size_t max_voices>
class VoiceManager
{
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float sample_rate)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].Init(sample_rate);
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
        float lfo_out[BLOCK_SIZE], pw1_out[BLOCK_SIZE], pw2_out[BLOCK_SIZE], pwlfo_out[BLOCK_SIZE],
                fm1_out[BLOCK_SIZE], fm2_out[BLOCK_SIZE], fmlfo_out[BLOCK_SIZE], reset_vector[BLOCK_SIZE],
                filt_lfo[BLOCK_SIZE], amp_lfo[BLOCK_SIZE], one_array[BLOCK_SIZE];
        float pw1_diff, pw2_diff;

        float fm_rad1 = ValuePanel[CTRL_OSC1FREQUENCYMOD] * TWOPI_F;
        float fm_rad2 = ValuePanel[CTRL_OSC2FREQUENCYMOD] * TWOPI_F;

        //Set fixed values for LFO modulation buffers
        arm_fill_f32(0.5, pwlfo_out, BLOCK_SIZE);
        arm_fill_f32(0, fmlfo_out, BLOCK_SIZE);

        //Array filled with 1s
        arm_fill_f32(1, one_array, BLOCK_SIZE);

        //Process LFO - Might be a good idea to give LFO its own process function
        lfo.ProcessBlock(lfo_out, pwlfo_out, fmlfo_out, reset_vector, false, BLOCK_SIZE);

        //Set modulated values for osc1
        pw1_diff = 0.5-ValuePanel[CTRL_OSC1PULSEWIDTH];
        arm_scale_f32(lfo_out, ValuePanel[CTRL_OSC1PWMOD], pw1_out, BLOCK_SIZE);
        arm_scale_f32(pw1_out, pw1_diff, pw1_out, BLOCK_SIZE);
        arm_offset_f32(pw1_out, ValuePanel[CTRL_OSC1PULSEWIDTH], pw1_out, BLOCK_SIZE);
        arm_scale_f32(lfo_out, fm_rad1, fm1_out, BLOCK_SIZE);

        //Set modulated values for osc2
        pw2_diff = 0.5-ValuePanel[CTRL_OSC2PULSEWIDTH];
        arm_scale_f32(lfo_out, ValuePanel[CTRL_OSC2PWMOD], pw2_out, BLOCK_SIZE);
        arm_scale_f32(pw2_out, pw2_diff, pw2_out, BLOCK_SIZE);
        arm_offset_f32(pw2_out, ValuePanel[CTRL_OSC2PULSEWIDTH], pw2_out, BLOCK_SIZE);
        arm_scale_f32(lfo_out, fm_rad2, fm2_out, BLOCK_SIZE);

        //Set modulated values for filter
        //System wide filter is modulated from LFO
        arm_scale_f32(lfo_out, ValuePanel[CTRL_FILTERLFOMOD], filt_lfo, BLOCK_SIZE);
        //LFO mod becomes subtrahend with 1 as minuend, difference is multiplied to cutoff frequency
        arm_sub_f32(one_array, filt_lfo, filt_lfo, BLOCK_SIZE);

        arm_scale_f32(lfo_out, ValuePanel[CTRL_AMPLFOMOD], amp_lfo, BLOCK_SIZE);
        arm_sub_f32(one_array, amp_lfo, amp_lfo, BLOCK_SIZE);

        for(size_t i = 0; i < max_voices; i++)
        {
            //if(voices[i].IsActive())
            //{
                float temp[BLOCK_SIZE];
                voices[i].ProcessBlock(temp, pw1_out, pw2_out, 
                                        fm1_out, fm2_out, 
                                        filt_lfo, amp_lfo, BLOCK_SIZE);
                arm_add_f32(buf, temp, buf, BLOCK_SIZE);
            //}
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

static VoiceManager<12> mgr;
static DaisySeed        hw;
static Encoder          enc;
MidiUartHandler         midi;
CpuLoadMeter            loadMeter;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    loadMeter.OnBlockStart();
    for(size_t i = 0; i < BLOCK_SIZE; i++)
    {
        out[0][i] = out[1][i] = mgr.Process() * 0.1;
    }
    loadMeter.OnBlockEnd();
}

void AudioCallbackBlock(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    loadMeter.OnBlockStart();
    float buf[BLOCK_SIZE];
    mgr.ProcessBlock(buf, BLOCK_SIZE);

    arm_scale_f32(buf, 0.5, buf, BLOCK_SIZE);

    out[0] = out[1] = buf;

    loadMeter.OnBlockEnd();
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

    if (param < 28)
    {
        mgr.SetParam(param);
    }
    else
    {
        switch(param)
        {
            case CTRL_LFOWAVEFORM:
                //No polybleps but have ramp
                ValuePanel[CTRL_LFOWAVEFORM] = ControlPanel[CTRL_LFOWAVEFORM] / 26;
                lfo.SetWaveform(ValuePanel[CTRL_LFOWAVEFORM]); 
                break;
            case CTRL_LFOFREQUENCY:
                //TODO implement tempo sync
                ValuePanel[CTRL_LFOFREQUENCY] = ControlPanel[CTRL_LFOFREQUENCY] / 6.4f;
                lfo.SetFreq(ValuePanel[CTRL_LFOFREQUENCY]);
                break;
            case CTRL_LFOTEMPOSYNC:
                ValuePanel[CTRL_LFOTEMPOSYNC] = ControlPanel[CTRL_LFOTEMPOSYNC] ? 1 : 0;
                break;
            case CTRL_FXTYPE:
                ValuePanel[CTRL_FXTYPE] = ControlPanel[CTRL_FXTYPE];
                break;
            case CTRL_FXPARAM1:
                ValuePanel[CTRL_FXPARAM1] = ControlPanel[CTRL_FXPARAM1];
                break;
            case CTRL_FXPARAM2:
                ValuePanel[CTRL_FXPARAM2] = ControlPanel[CTRL_FXPARAM2];
                break;
            case CTRL_FXMIX:
                ValuePanel[CTRL_FXMIX] = ControlPanel[CTRL_FXMIX];
                break;

            default: break;
        }
    }
}

int main(void)
{
    hw.Init(true);
    hw.SetAudioBlockSize(BLOCK_SIZE);
    hw.StartLog();

    float sample_rate = hw.AudioSampleRate();

    // Initialize Midi 
    MidiUartHandler::Config midi_cfg;

    midi.Init(midi_cfg);
    enc.Init(hw.GetPin(0), hw.GetPin(2), hw.GetPin(1));
    mgr.Init(sample_rate);
    lfo.Init(sample_rate);
    loadMeter.Init(sample_rate, BLOCK_SIZE);

    //uint8_t param = 0;

    lfo.SetAmp(1);
    lfo.SetWaveform(ValuePanel[CTRL_LFOWAVEFORM]);
    lfo.SetFreq(ValuePanel[CTRL_LFOFREQUENCY]); 

    // start the audio callback
    hw.StartAudio(AudioCallbackBlock);
    midi.StartReceive();

    for(;;)
    {
        /*enc.Debounce(); //adds so much noise
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
        */

        // Listen to MIDI
        midi.Listen();

        // When message waiting
        while(midi.HasEvents())
        {
            //Calculate CPU average load
            const float avgLoad = loadMeter.GetAvgCpuLoad();
            hw.PrintLine("Avg: " FLT_FMT3, FLT_VAR3(avgLoad * 100.0f));

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
