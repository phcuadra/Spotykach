#include "fx.grit.h"
#include "common.h"

using namespace spotykach;
using namespace daisysp;

void Grit::init(const float sample_rate)
{
    _drive.init(sample_rate);
    _reduce.init(sample_rate);
    _filter.init(sample_rate);
    _disintegrate.init(sample_rate);
    set_intensity(_drive.intensity());
    set_mix(.5f, true);
}

void Grit::switch_mode()
{
    static constexpr auto kModeCount = 4;
    static constexpr Mode kModes[kModeCount] = {
        Mode::Drive,
        Mode::Reduce,
        Mode::Filter,
        Mode::Disintegrate
    };
    for (auto i = 0; i < kModeCount; i++) {
        if (_mode == kModes[i]) {
            _mode = kModes[(i + 1) % kModeCount];
            if (_mode != Mode::Disintegrate) {
                _disintegrate.set_gate(0.0f);
            }
            return;
        }
    }
}

void Grit::set_gate(const float gate)
{
    _disintegrate.set_gate(_mode == Mode::Disintegrate ? gate : 0.0f);
}

float Grit::intensity()
{
    switch (_mode) {
        case Mode::Drive: return _drive.intensity();
        case Mode::Reduce: return _reduce.intensity();
        case Mode::Filter: return _filter.cutoff();
        case Mode::Disintegrate: return _disintegrate.intensity();
        default: return 0.0f;
    }
}
void Grit::set_intensity(const float norm)
{
    auto clamp = fclamp(norm, 0.0f, 1.0f);
    switch (_mode) {
        case Mode::Drive: _drive.set_intensity(clamp); break;
        case Mode::Reduce: _reduce.set_intensity(clamp); break;
        case Mode::Filter: _filter.set_cutoff(clamp); break;
        case Mode::Disintegrate: _disintegrate.set_intensity(clamp); break;
    }
}
float Grit::character()
{
    switch (_mode) {
        case Mode::Filter: return _filter.q();
        case Mode::Disintegrate: return _disintegrate.character();
        default: return 0.0f;
    }
}
void Grit::set_character(const float norm)
{
    auto clamp = fclamp(norm, 0.0f, 1.0f);
    switch (_mode) {
        case Mode::Filter: _filter.set_q(clamp); break;
        case Mode::Disintegrate: _disintegrate.set_character(clamp); break;
        default: break;
    }
}
float Grit::mix()
{
    switch (_mode) {
        case Mode::Drive: return _drive.mix();
        case Mode::Reduce: return _reduce.mix();
        case Mode::Filter: return _filter.mix();
        case Mode::Disintegrate: return _disintegrate.mix();
        default: return 0.f;
    }
}
void Grit::set_mix(const float norm, const bool all)
{
    auto clamp = fclamp(norm, 0.0f, 1.0f);
    if (all) {
        _drive.set_mix(clamp);
        _reduce.set_mix(clamp);
        _filter.set_mix(clamp);
        _disintegrate.set_mix(clamp);
        return;
    }

    switch (_mode) {
        case Mode::Drive: _drive.set_mix(clamp); break;
        case Mode::Reduce: _reduce.set_mix(clamp); break;
        case Mode::Filter: _filter.set_mix(clamp); break;
        case Mode::Disintegrate: _disintegrate.set_mix(clamp); break;
    }
}

void Grit::process(float& inout0, float& inout1)
{
    switch (_mode) {
        case Mode::Drive: _drive.process(inout0, inout1); break;
        case Mode::Reduce: _reduce.process(inout0, inout1); break;
        case Mode::Filter: _filter.process(inout0, inout1); break;
        case Mode::Disintegrate: _disintegrate.process(inout0, inout1); break;
    }
}
