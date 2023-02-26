# desktop-synthesizer

A polyphonic, multi-oscillator synth with many bells and many whistles. Based around the Daisy Seed platform.

##Controls
| Control              | Description                 | Unit               |
| --------------------| --------------------------- | ------------------ |
| OSC1WAVEFORM         | Waveform of oscillator 1     | Wavetype                |
| OSC1PULSEWIDTH       | Pulse width of oscillator 1  | %                  |
| OSC1FREQUENCYMOD     | Frequency modulation of oscillator 1 from LFO | %  |
| OSC1PWMOD            | Pulse width modulation of oscillator 1 from LFO| % |
| OSC2WAVEFORM         | Waveform of oscillator 2     | Wavetype                |
| OSC2PULSEWIDTH       | Pulse width of oscillator 2  | %                  |
| OSC2FREQUENCYMOD     | Frequency modulation of oscillator 2 from LFO | % |
| OSC2PWMOD            | Pulse width modulation of oscillator 2 from LFO| % |
| OSC2TUNEFINE         | Fine tune of oscillator 2    | Cents              |
| OSC2TUNECOARSE       | Coarse tune of oscillator 2  | Semitones          |
| OSC2SYNC             | Oscillator 1 resets oscillator 2s cycle | True/False         |
| NOISE                | Level of white noise         | dB                 |
| OSCMIX               | Mix between oscillator 1 and 2 | %               |
| OSCSPLIT             | Split keyboard at C4, left side is Osc1 and right is Osc2 | True/False            |
| FILTERCUTOFF         | Cutoff frequency of filter   | Hz                 |
| FILTERRESONANCE      | Resonance of filter           | %                 |
| FILTERLFOMOD         | Frequency modulation of filter by LFO | Hz      |
| FILTERVELOCITYMOD    | Modulation of filter by velocity | dB            |
| FILTERKEYBEDTRACK    | Tracking of filter by keybed position | dB/octave |
| FILTERATTACK         | Envelope attack of filter     | s                  |
| FILTERDECAY          | Envelope decay of filter      | s                  |
| FILTERSUSTAIN        | Envelope sustain of filter    | %                  |
| FILTERRELEASE        | Envelope release of filter    | s                  |
| AMPATTACK            | Envelope attack of amplifier | s                  |
| AMPDECAY             | Envelope decay of amplifier   | s                  |
| AMPSUSTAIN           | Envelope sustain of amplifier | %                  |
| AMPRELEASE           | Envelope release of amplifier | s                  |
| AMPLFOMOD            | Frequency modulation of amplifier by LFO | Hz |
| LFOWAVEFORM          | Waveform of LFO               | Wavetype                |
| LFOFREQUENCY         | Frequency of LFO              | Hz                 |
| LFOTEMPOSYNC         | Synchronize LFO to tempo      | True/False                |
| FXTYPE               | Type of effect                | N/A                |
| FXPARAM1             | Parameter 1 of effect         | N/A                |
| FXPARAM2             | Parameter 2 of effect         | N/A                |
| FXMIX                | Mix level of effect           | %                  |

##Control-Flow diagram
```mermaid
flowchart TB

    LFO-->PW-Modulation-1
    LFO-->PW-Modulation-2
    LFO-->Frequency-Modulation-1
    LFO-->Frequency-Modulation-2
    LFO-->LFO-F
    LFO-->LFO-A
    
    Osc1-->Mix
    Osc2-->Mix
    
    Moise-->Filter
    Mix-->Filter
    
    Filter-->Amp
    
    Amp-->FX
    
    FX-->Volume
    subgraph Voice
      subgraph Osc1
        Wave-1
        PW-1
        Frequency-Modulation-1
        PW-Modulation-1
      end
      subgraph Osc2
        Wave-2
        PW-2
        Frequency-Modulation-2
        PW-Modulation-2
        Pitch
        pitchsw(Fine/Octave)-->Pitch
        Sync
      end
      subgraph OscControl
        Mix
        split(Split)
      end
      subgraph Filter
        Cutoff
        Resonance
        subgraph Filter-Modulation
          Velocity
          LFO-F
          Keybed-Tracking
          subgraph Envelope-F
            Attack-F
            Decay-F
            Sustain-F
            Release-F
          end
        end
      end
      subgraph Amp
        LFO-A
        subgraph Envelope-A
          Attack-A
          Decay-A
          Sustain-A
          Release-A
        end
      end
    end
    subgraph LFO
      Frequency
      Waveform
      sync(Tempo-Sync)
    end
    subgraph FX
      Type
      Param1
      Param2
      Mix
    end
    
    
```
