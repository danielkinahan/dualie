#include "daisy_seed.h"
#include "daisysp.h"

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

    void SetParam(int ctrlValue, int param, bool midiCC) 
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
                if (midiCC){
                    ctrlAttack = ctrlValue;
                }
                else
                {
                    ctrlAttack += ctrlValue;
                }
                env_.SetTime(ADENV_SEG_ATTACK, (ctrlAttack / 32.f));
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
                env_.SetTime(ADENV_SEG_DECAY, (ctrlDecay / 32.f));
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
                env_.SetSustainLevel(ctrlSustain / 127.f);
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
                env_.SetTime(ADSR_SEG_RELEASE, (ctrlRelease / 64.f));
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

    void SetParam(int ctrlValue, int param, bool midiCC)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetParam(ctrlValue, param, midiCC);
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
//static Oscillator       osc;
static Oscillator       lfo;
static Encoder          enc;
static MidiUsbHandler   midi;
//static Adsr             env;
static MoogLadder       flt;
//bool                    gate;

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
    if (param < 5)
    {
        mgr.SetParam(ctrlValue, param, midiCC);
    }
    else
    {
        switch(param)
        {
            case 5:
            {
                if (midiCC){
                    ctrlFilterFreq = ctrlValue;
                }
                else
                {
                    ctrlFilterFreq += ctrlValue;
                }
                //flt.SetFreq(ctrlFilterFreq * 174);
            }
            break;
            case 6:
            {
                if (midiCC){
                    ctrlFilterRes = ctrlValue;
                }
                else
                {
                    ctrlFilterRes += ctrlValue;
                }
                flt.SetRes(ctrlFilterRes / 134.0f);
            }
            break;
            case 7:
            {
                if (midiCC){
                    ctrlLFOFreq = ctrlValue;
                }
                else
                {
                    ctrlLFOFreq += ctrlValue;
                }
                lfo.SetFreq(ctrlLFOFreq / 6.4f);
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
