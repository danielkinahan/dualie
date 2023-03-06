#include <Utility/dsp.h>
#include "oscillator.h"
#include "arm_math.h"

using namespace custom;
static inline float Polyblep(float phase_inc, float t);

constexpr float TWO_PI_RECIP = 1.0f / TWOPI_F;

void Oscillator::ProcessBlock(float *buf, float *pw_buf, float *fm_buf, float *reset_vector, bool reset, size_t size)
{
    //TODO: Add phase incr vector modulated by fm_bufs
    float double_pi_recip;
    float phase_vector[size], t_vector[size], pw_vector[size], pw_rad_vector[size];//, phase_inc_vector[size];

    //Block processing PhaseAdd
    arm_scale_f32(fm_buf, TWOPI_F, fm_buf, size);

    //switching to phase_vector saves 5% avg load
    for (size_t i = 0; i < size; i++)
    {
        if(reset && reset_vector[i])
        {
            phase_vector[i] = 0;
            continue;
        }
        phase_ += phase_inc_;
        if(phase_ > TWOPI_F)
        {
            phase_ -= TWOPI_F;
            reset_vector[i] = true; //end of cycle
        }
        else
        {
            reset_vector[i] = false;
        }
        //eor_ = (phase_ - phase_inc_ < PI_F && phase_ >= PI_F); //end of rise - not using right now
        phase_vector[i] = phase_;
    }

    //Add phase - this doesn't sound right
    arm_add_f32(phase_vector, fm_buf, phase_vector, size);

    double_pi_recip = 2.0f * TWO_PI_RECIP;

    switch(waveform_)
    {
        case WAVE_SIN:
            for (size_t i = 0; i < size; i++)
            {
                buf[i] = arm_sin_f32(phase_vector[i]);
            }
            break;
        case WAVE_TRI:
            arm_scale_f32(phase_vector, double_pi_recip, t_vector, size);
            arm_offset_f32(t_vector, -1.0f, t_vector, size);
            arm_abs_f32(t_vector, t_vector, size);
            arm_offset_f32(t_vector, -0.5f, t_vector, size);
            arm_scale_f32(t_vector, 2.0f, buf, size);
            break;
        case WAVE_SAW:
            arm_scale_f32(phase_vector, double_pi_recip, buf, size);
            arm_offset_f32(buf, -1.0f, buf, size);
            arm_negate_f32(buf, buf, size);
            break;
        case WAVE_RAMP:
            arm_scale_f32(phase_vector, double_pi_recip, buf, size);
            arm_offset_f32(buf, -1.0f, buf, size);
            break;
        case WAVE_SQUARE:
            arm_clip_f32(pw_buf, pw_vector, 0.f, 1.f, 16);
            arm_scale_f32(pw_vector, TWOPI_F, pw_rad_vector, size);
            //I bet there is probably a better way of using ternary ops on a vector
            for (size_t i = 0; i < size; i++)
            {
                buf[i] = phase_vector[i] < pw_rad_vector[i] ? (1.0f) : -1.0f; 
            }
            break;

        //Implementing polyblep block processing will be harder
        //I'll leave these as simple loops for now
        
        //Currently polyblep_tri and polyblep_square take double the processing power
        case WAVE_POLYBLEP_TRI:
            arm_scale_f32(phase_vector, TWO_PI_RECIP, t_vector, size);
            for (size_t i = 0; i < size; i++)
            {
                buf[i] = phase_vector[i] < PI_F ? 1.0f : -1.0f; 
                buf[i] += Polyblep(phase_inc_, t_vector[i]);
                buf[i] -= Polyblep(phase_inc_, fmodf(t_vector[i] + 0.5f, 1.0f));
                // Leaky Integrator:
                // y[n] = A + x[n] + (1 - A) * y[n-1]
                buf[i]       = phase_inc_ * buf[i] + (1.0f - phase_inc_) * last_out_;
                last_out_ = buf[i];
            }
            break;
        case WAVE_POLYBLEP_SAW:
            arm_scale_f32(phase_vector, TWO_PI_RECIP, t_vector, size);
            for (size_t i = 0; i < size; i++)
            {
                buf[i] = (2.0f * t_vector[i]) - 1.0f;
                buf[i] -= Polyblep(phase_inc_, t_vector[i]);
                buf[i] *= -1.0f;
            }
            break;
        case WAVE_POLYBLEP_SQUARE:
            arm_clip_f32(pw_buf, pw_vector, 0.f, 1.f, 16);
            arm_scale_f32(pw_vector, TWOPI_F, pw_rad_vector, size);

            arm_scale_f32(phase_vector, TWO_PI_RECIP, t_vector, size);
            for (size_t i = 0; i < size; i++)
            {
                buf[i] = phase_vector[i] < pw_rad_vector[i] ? 1.0f : -1.0f;
                buf[i] += Polyblep(phase_inc_, t_vector[i]);
                buf[i] -= Polyblep(phase_inc_, fmodf(t_vector[i] + (1.0f - pw_vector[i]), 1.0f));
                buf[i] *= 0.707f; // ?
            }
            break;
        default: arm_fill_f32(0, buf, size); break;
    }
}

float Oscillator::Process()
{
    float out, t;
    switch(waveform_)
    {
        case WAVE_SIN: out = sinf(phase_); break;
        case WAVE_TRI:
            t   = -1.0f + (2.0f * phase_ * TWO_PI_RECIP);
            out = 2.0f * (fabsf(t) - 0.5f);
            break;
        case WAVE_SAW:
            out = -1.0f * (((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f);
            break;
        case WAVE_RAMP: out = ((phase_ * TWO_PI_RECIP * 2.0f)) - 1.0f; break;
        case WAVE_SQUARE: out = phase_ < pw_rad_ ? (1.0f) : -1.0f; break;
        case WAVE_POLYBLEP_TRI:
            t   = phase_ * TWO_PI_RECIP;
            out = phase_ < PI_F ? 1.0f : -1.0f;
            out += Polyblep(phase_inc_, t);
            out -= Polyblep(phase_inc_, fmodf(t + 0.5f, 1.0f));
            // Leaky Integrator:
            // y[n] = A + x[n] + (1 - A) * y[n-1]
            out       = phase_inc_ * out + (1.0f - phase_inc_) * last_out_;
            last_out_ = out;
            break;
        case WAVE_POLYBLEP_SAW:
            t   = phase_ * TWO_PI_RECIP;
            out = (2.0f * t) - 1.0f;
            out -= Polyblep(phase_inc_, t);
            out *= -1.0f;
            break;
        case WAVE_POLYBLEP_SQUARE:
            t   = phase_ * TWO_PI_RECIP;
            out = phase_ < pw_rad_ ? 1.0f : -1.0f;
            out += Polyblep(phase_inc_, t);
            out -= Polyblep(phase_inc_, fmodf(t + (1.0f - pw_), 1.0f));
            out *= 0.707f; // ?
            break;
        default: out = 0.0f; break;
    }
    phase_ += phase_inc_;
    if(phase_ > TWOPI_F)
    {
        phase_ -= TWOPI_F;
        eoc_ = true;
    }
    else
    {
        eoc_ = false;
    }
    eor_ = (phase_ - phase_inc_ < PI_F && phase_ >= PI_F);

    return out * amp_;
}

float Oscillator::CalcPhaseInc(float f)
{
    return (TWOPI_F * f) * sr_recip_;
}

static float Polyblep(float phase_inc, float t)
{
    float dt = phase_inc * TWO_PI_RECIP;
    if(t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    else if(t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    else
    {
        return 0.0f;
    }
}
