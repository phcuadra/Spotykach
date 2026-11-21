#pragma once

#include <cstddef>
#include <cstdint>
#include "nocopy.h"
#include "../buffer.h"

namespace spotykach {

class Disintegrate {
public:
    Disintegrate();
    ~Disintegrate() = default;

    void init(const float sample_rate);

    // Output safety stage when Grit is in Disintegrate mode (decay itself is permanently written into Buffer)
    void process(float& inout0, float& inout1);

    // Destructive per-frame tape wear applied directly to the loop buffer as the playhead leaves frames behind
    void erode_frame(Buffer::Frame* buf, const size_t rec_size, const int32_t idx, const float weight);

    // Advances physical tape defect seed for a newly recorded or loaded loop
    void reset_tape();

    // Smooth SoftSwitch gate [0..1] from Fx::_grit_switch (0 when inactive or in another Grit mode)
    void set_gate(const float gate) { _gate = gate; }
    float gate() const { return _gate; }
    bool is_active() const { return _gate > 0.0001f && _intensity >= 0.005f; }

    // Recent oxide flaking activity [0..1] for subtle LED feedback
    float activity() const { return _activity; }

    // Pitch knob (while holding Grit): Erosion rate [0..1]
    float intensity() const { return _intensity; }
    void set_intensity(const float norm);

    // Pos knob (while holding Grit): Dropout character / patch size [0..1]
    float character() const { return _character; }
    void set_character(const float norm);

    // SOS knob (while holding Grit): HF tape wear, saturation warmth & micro-crackle [0..1]
    float mix() const { return _mix; }
    void set_mix(const float norm);

private:
    NOCOPY(Disintegrate)

    void _apply();
    uint32_t _next_u32();
    float _next_bipolar();

    float _sample_rate;
    float _intensity;
    float _character;
    float _mix;
    float _gate;

    // Precomputed DSP parameters
    float _rate;
    float _spatial_freq;
    float _drift_inc;
    float _stereo_link;

    // Slowly evolving tape-defect drift & band-limited oxide crackle state
    float _drift_phase;
    float _detail_phase;
    float _dust_lp_l;
    float _dust_lp_r;
    float _activity;

    uint32_t _rng_state;
    uint32_t _tape_seed_l;
    uint32_t _tape_seed_r;
};

};
