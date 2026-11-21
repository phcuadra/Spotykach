#pragma once

#include "nocopy.h"
#include "fx.drive.h"
#include "fx.reduce.h"
#include "filter.h"
#include "fx.disintegrate.h"

namespace spotykach {

class Grit {
public:
    enum class Mode: uint8_t {
        Drive,
        Reduce,
        Filter,
        Disintegrate
    };

    Grit() = default;
    ~Grit() = default;

    void init(const float sample_rate);
    void process(float& inout0, float& inout1);

    Mode mode() const { return _mode; }
    void switch_mode();

    void set_gate(const float gate);
    Disintegrate& disintegrator() { return _disintegrate; }
    const Disintegrate& disintegrator() const { return _disintegrate; }

    float intensity();
    void set_intensity(const float norm);

    float character();
    void set_character(const float norm);

    float mix();
    void set_mix(const float norm, const bool all = false);

private:
    NOCOPY(Grit)

    Drive _drive;
    Reduce _reduce;
    Filter _filter;
    Disintegrate _disintegrate;

    Mode _mode { Mode::Drive };
};

};
