#include "fx.disintegrate.h"
#include <algorithm>
#include <cmath>

using namespace spotykach;

namespace {

static constexpr float kMinActiveIntensity = 0.005f;
static constexpr float kSilenceFloor       = 1.0e-5f;
static constexpr float kCrackleGateFloor   = 0.0025f;

// Patch width spans ~16 ms (750 samples) at character=0 to ~1.33 s (64000 samples) at character=1
static constexpr float kMaxSpatialFreq = 1.0f / 750.0f;
static constexpr float kMinSpatialFreq = 1.0f / 64000.0f;

inline float fast_clamp(const float v, const float lo, const float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float smoothstep01(const float t)
{
    const float c = fast_clamp(t, 0.0f, 1.0f);
    return c * c * (3.0f - 2.0f * c);
}

inline float hash01(const int32_t n, const uint32_t seed)
{
    uint32_t h = static_cast<uint32_t>(n) * 0x9E3779B1u ^ seed;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

// C1-continuous 1D value noise in [0, 1]
inline float smooth_noise_1d(const float x, const uint32_t seed)
{
    const float fl = std::floor(x);
    const int32_t i = static_cast<int32_t>(fl);
    const float f = x - fl;
    const float u = f * f * (3.0f - 2.0f * f);
    const float a = hash01(i, seed);
    const float b = hash01(i + 1, seed);
    return a + u * (b - a);
}

// Evaluates localized oxide flaking strength [0, 1] at a spatial coordinate
inline float eval_oxide_flake(const float pos, const float detail_phase, const uint32_t seed)
{
    const float coarse = smooth_noise_1d(pos, seed);
    const float fine   = smooth_noise_1d(pos * 2.73f + detail_phase, seed ^ 0xA53C9E47u);
    const float field  = 0.72f * coarse + 0.28f * fine;

    // ~52% of the tape starts as an intact island (flake == 0), while defect peaks reach 0.8..1.0
    const float raw = fast_clamp((field - 0.51f) * 3.2f, 0.0f, 1.0f);
    return smoothstep01(raw);
}

inline float soft_clip_guard(const float x)
{
    // Mild safety bound matching DaisySP SoftLimit behavior without altering normal levels
    if (x > 1.2f) return 1.2f;
    if (x < -1.2f) return -1.2f;
    return x;
}

} // namespace

Disintegrate::Disintegrate():
_sample_rate    { 48000.0f },
_intensity      { 0.25f },
_character      { 0.45f },
_mix            { 0.30f },
_gate           { 0.0f },
_rate           { 0.0f },
_spatial_freq   { 1.0f / 6900.0f },
_drift_inc      { 1.5e-6f },
_stereo_link    { 0.64f },
_drift_phase    { 0.0f },
_detail_phase   { 17.3f },
_dust_lp_l      { 0.0f },
_dust_lp_r      { 0.0f },
_activity       { 0.0f },
_rng_state      { 0x6D2B79F5u },
_tape_seed_l    { 0x1F83D9ABu },
_tape_seed_r    { 0x5BE0CD19u }
{
    _apply();
}

void Disintegrate::init(const float sample_rate)
{
    _sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    reset_tape();
    _apply();
}

void Disintegrate::reset_tape()
{
    _tape_seed_l = _next_u32() | 1u;
    _tape_seed_r = _next_u32() | 1u;
    _drift_phase = 0.0f;
    _detail_phase = 17.3f;
    _dust_lp_l = 0.0f;
    _dust_lp_r = 0.0f;
    _activity = 0.0f;
}

void Disintegrate::set_intensity(const float norm)
{
    _intensity = fast_clamp(norm, 0.0f, 1.0f);
    _apply();
}

void Disintegrate::set_character(const float norm)
{
    _character = fast_clamp(norm, 0.0f, 1.0f);
    _apply();
}

void Disintegrate::set_mix(const float norm)
{
    _mix = fast_clamp(norm, 0.0f, 1.0f);
    _apply();
}

void Disintegrate::_apply()
{
    if (_intensity < kMinActiveIntensity) {
        _rate = 0.0f;
    }
    else {
        // Musical curve: gentle multi-minute decay at low settings, dramatic shedding near 1.0
        _rate = 0.14f * _intensity + 0.86f * (_intensity * _intensity);
    }

    // Exponential interpolation between kMaxSpatialFreq (fine pitting) and kMinSpatialFreq (large sheets)
    const float log_ratio = std::log(kMinSpatialFreq / kMaxSpatialFreq);
    _spatial_freq = kMaxSpatialFreq * std::exp(_character * log_ratio);

    // Slow boundary creep (~0.008 to ~0.045 patch-widths per second) keeps weak spots physically
    // anchored on the loop over many passes while gradually nibbling into surviving islands over time
    _drift_inc = (0.008f + 0.037f * _rate) / _sample_rate;

    // Fine pitting (character=0) allows subtle L/R track independence; large sheets (character=1) peel both tracks together
    _stereo_link = 0.35f + 0.65f * _character;
}

uint32_t Disintegrate::_next_u32()
{
    uint32_t x = _rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _rng_state = x ? x : 0x6D2B79F5u;
    return _rng_state;
}

float Disintegrate::_next_bipolar()
{
    return static_cast<float>(static_cast<int32_t>(_next_u32() >> 8)) * (1.0f / 8388608.0f) - 1.0f;
}

void Disintegrate::process(float& inout0, float& inout1)
{
    // All tone darkening, dropouts, warmth, and crackle are destructively baked into Buffer
    // so releasing Grit leaves no abrupt timbre jump.
    inout0 = soft_clip_guard(inout0);
    inout1 = soft_clip_guard(inout1);
}

void Disintegrate::erode_frame(Buffer::Frame* buf, const size_t rec_size, const int32_t idx, const float weight)
{
    if (buf == nullptr || rec_size < 8 || _rate <= 0.0f) return;

    const float eff = fast_clamp(weight * _gate, 0.0f, 1.0f);
    if (eff <= 0.0001f) return;

    const int32_t size = static_cast<int32_t>(rec_size);
    int32_t i0 = idx % size;
    if (i0 < 0) i0 += size;

    // 1. Spatially persistent, periodic C1-smooth oxide flaking field
    const float pos = static_cast<float>(i0) * _spatial_freq + _drift_phase;
    float flake_l = eval_oxide_flake(pos, _detail_phase, _tape_seed_l);
    float flake_r = eval_oxide_flake(pos, _detail_phase + 5.17f, _tape_seed_r);

    // Blend near loop wraparound so the oxide wear profile is C1-periodic across the splice seam
    const int32_t seam_len = std::min<int32_t>(2048, size >> 2);
    const int32_t seam_start = size - seam_len;
    if (seam_len > 16 && i0 >= seam_start) {
        const float seam_t = smoothstep01(static_cast<float>(i0 - seam_start) / static_cast<float>(seam_len));
        const float wrapped_pos = static_cast<float>(i0 - size) * _spatial_freq + _drift_phase;
        const float wrap_l = eval_oxide_flake(wrapped_pos, _detail_phase, _tape_seed_l);
        const float wrap_r = eval_oxide_flake(wrapped_pos, _detail_phase + 5.17f, _tape_seed_r);
        flake_l += seam_t * (wrap_l - flake_l);
        flake_r += seam_t * (wrap_r - flake_r);
    }

    flake_r = flake_r + _stereo_link * (flake_l - flake_r);

    // Advance slow defect drift (wrapped to preserve float precision indefinitely)
    _drift_phase += _drift_inc * eff;
    if (_drift_phase >= 4096.0f) _drift_phase -= 4096.0f;
    _detail_phase += _drift_inc * 1.618f * eff;
    if (_detail_phase >= 4096.0f) _detail_phase -= 4096.0f;

    _activity += 0.002f * (0.5f * (flake_l + flake_r) - _activity);

    float xl = buf[i0].l;
    float xr = buf[i0].r;

    // 2. Progressive High-Frequency Dulling (zero-phase convex stride-1 + stride-3 waveform diffusion)
    const int32_t im1 = (i0 == 0) ? (size - 1) : (i0 - 1);
    const int32_t ip1 = (i0 + 1 == size) ? 0 : (i0 + 1);
    int32_t im3 = i0 - 3;
    if (im3 < 0) im3 += size;
    int32_t ip3 = i0 + 3;
    if (ip3 >= size) ip3 -= size;

    const float wear_base = eff * (0.06f + 0.94f * _rate) * (0.08f + 0.92f * _mix);
    const float kappa_l = fast_clamp(wear_base * (0.75f + 1.45f * flake_l), 0.0f, 0.48f);
    const float kappa_r = fast_clamp(wear_base * (0.75f + 1.45f * flake_r), 0.0f, 0.48f);

    const float lap1_l = 0.5f * (buf[im1].l + buf[ip1].l) - xl;
    const float lap3_l = 0.5f * (buf[im3].l + buf[ip3].l) - xl;
    xl += kappa_l * (0.65f * lap1_l + 0.35f * lap3_l * _mix);

    const float lap1_r = 0.5f * (buf[im1].r + buf[ip1].r) - xr;
    const float lap3_r = 0.5f * (buf[im3].r + buf[ip3].r) - xr;
    xr += kappa_r * (0.65f * lap1_r + 0.35f * lap3_r * _mix);

    // 3. Oxide Dropouts / Flaking (damaged/thinned regions shed oxide slightly faster)
    const float env_l = std::fabs(xl);
    const float env_r = std::fabs(xr);
    const float thin_l = 1.0f + 0.65f * (1.0f - fast_clamp(env_l * 6.67f, 0.0f, 1.0f));
    const float thin_r = 1.0f + 0.65f * (1.0f - fast_clamp(env_r * 6.67f, 0.0f, 1.0f));

    const float loss_l = fast_clamp(eff * _rate * (0.016f + 0.58f * flake_l * thin_l), 0.0f, 1.0f);
    const float loss_r = fast_clamp(eff * _rate * (0.016f + 0.58f * flake_r * thin_r), 0.0f, 1.0f);
    xl *= (1.0f - loss_l);
    xr *= (1.0f - loss_r);

    // 4. Subtle Odd-Symmetric Tape Compression / Saturation Warmth (zero DC offset, strictly contractive)
    const float sat = eff * _rate * _mix * 0.24f;
    if (sat > 0.0f) {
        xl = xl / (1.0f + sat * xl * xl);
        xr = xr / (1.0f + sat * xr * xr);
    }

    // 5. Oxide Asperity Micro-Crackle (band-limited warm dust modulated by signal envelope)
    if (_mix > 0.01f && (env_l > kCrackleGateFloor || env_r > kCrackleGateFloor)) {
        // ~0.5% sparse impulse probability per sample, filtered into ~0.3ms warm analog oxide ticks
        float imp_l = 0.0f;
        float imp_r = 0.0f;
        if ((_next_u32() & 0xFFu) == 0u) {
            imp_l = _next_bipolar();
            imp_r = _stereo_link * imp_l + (1.0f - _stereo_link) * _next_bipolar();
        }
        _dust_lp_l += 0.32f * (imp_l - _dust_lp_l);
        _dust_lp_r += 0.32f * (imp_r - _dust_lp_r);

        const float crackle_scale = eff * _mix * (0.30f + 0.70f * _intensity);
        if (env_l > kCrackleGateFloor) {
            xl += _dust_lp_l * crackle_scale * (0.012f + 0.045f * flake_l) * env_l;
        }
        if (env_r > kCrackleGateFloor) {
            xr += _dust_lp_r * crackle_scale * (0.012f + 0.045f * flake_r) * env_r;
        }
    }

    // Flush sub-audible tails cleanly to pure silence (prevents Cortex-M7 denormals & residual hiss)
    buf[i0].l = (std::fabs(xl) < kSilenceFloor) ? 0.0f : soft_clip_guard(xl);
    buf[i0].r = (std::fabs(xr) < kSilenceFloor) ? 0.0f : soft_clip_guard(xr);
}
