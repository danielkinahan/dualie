#include "daisysp.h"
#include "daisy_seed.h"
#include "desktop-synthesizer.h"

#include "oscillator.h"
#include "adsr.h"
#include "moogladder.h"
#include "whitenoise.h"
#include "arm_math.h"

using namespace daisysp;
using namespace daisy;
using namespace daisy::seed;

class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.1f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.01f);
    }

    void ProcessBlock(float *buf, size_t size)
    {
        float amp_out[size], osc_out[size];
        env_.ProcessBlock(amp_out, size, env_gate_);
        
        osc_.ProcessBlock(osc_out, size);

        arm_mult_f32(osc_out, amp_out, buf, size);
    }

    float Process()
    {
        float amp = env_.Process(env_gate_);
        if(env_.IsRunning())
        {
            float sig;
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
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    inline bool  IsActive() const { return env_.IsRunning(); }
    inline float GetNote() const { return note_; }

  private:
    custom::Oscillator osc_;
    custom::Adsr       env_;
    float      note_, velocity_;
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

    void ProcessBlock(float *buf, size_t size)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            float temp[size];
            voices[i].ProcessBlock(temp, size);
            arm_add_f32(buf, temp, buf, size);
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

static DaisySeed        hw;
MidiUsbHandler          midi;
static VoiceManager<24> mgr;

void AudioCallback(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = out[1][i] = mgr.Process() * 0.2;
    }
}

void AudioCallbackBlock(AudioHandle::InputBuffer  in, AudioHandle::OutputBuffer out, size_t size)
{
    float buf[size];
    mgr.ProcessBlock(buf, size);

    arm_scale_f32(buf, 0.2, buf, size);

    out[0] = out[1] = buf;
}

void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            if(p.velocity == 0.f)
            {
                mgr.OnNoteOff(p.note, p.velocity);
            }
            else
            {
                mgr.OnNoteOn(p.note, p.velocity);
            }
        }
        break;
        case NoteOff:
        {
            NoteOnEvent p = m.AsNoteOn();
            mgr.OnNoteOff(p.note, p.velocity);
        }
        break;
        default: break;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(16);
    
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    mgr.Init(hw.AudioSampleRate());

    hw.StartAudio(AudioCallbackBlock);
    midi.StartReceive();

    for(;;) 
    {
        midi.Listen();
        while(midi.HasEvents())
        {
            HandleMidiMessage(midi.PopEvent());
        }
    }
}