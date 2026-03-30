#include "lib_sig_cond.h"

static float clamp_pct(float value)
{
    if (value < 0.0f) { return 0.0f; }
    if (value > 100.0f) { return 100.0f; }
    return value;
}

static float median3(float a, float b, float c)
{
    if (a > b)
    {
        const float t = a;
        a = b;
        b = t;
    }
    if (b > c)
    {
        const float t = b;
        b = c;
        c = t;
    }
    if (a > b)
    {
        const float t = a;
        a = b;
        b = t;
    }
    return b;
}

static float compute_median(const SigCond42State* state, float fallback)
{
    if (state->sampleCount == 0u) { return fallback; }
    if (state->sampleCount == 1u) { return state->samples[0]; }
    if (state->sampleCount == 2u)
    {
        return 0.5f * (state->samples[0] + state->samples[1]);
    }
    return median3(state->samples[0], state->samples[1], state->samples[2]);
}

void sig42_init(SigCond42State* state, float initialPct, unsigned long nowMs)
{
    const float initial = clamp_pct(initialPct);

    state->samples[0] = initial;
    state->samples[1] = initial;
    state->samples[2] = initial;
    state->sampleIndex = 0u;
    state->sampleCount = 3u;
    state->weighted = initial;
    state->ramped = initial;
    state->lastMs = nowMs;
    state->initialized = true;
}

void sig42_step(
    SigCond42State* state,
    float rawPct,
    unsigned long nowMs,
    float alpha,
    float rampPctPerSec,
    float* outClampedPct,
    float* outMedianPct,
    float* outWeightedPct,
    float* outRampedPct,
    bool* outClampAlert,
    bool* outLimitAlert)
{
    if (!state->initialized)
    {
        sig42_init(state, rawPct, nowMs);
    }

    const float clamped = clamp_pct(rawPct);
    const bool clampAlert = (clamped != rawPct);

    state->samples[state->sampleIndex] = clamped;
    state->sampleIndex = (uint8_t)((state->sampleIndex + 1u) % 3u);
    if (state->sampleCount < 3u)
    {
        state->sampleCount++;
    }

    const float median = compute_median(state, clamped);

    if (alpha < 0.0f) { alpha = 0.0f; }
    if (alpha > 1.0f) { alpha = 1.0f; }
    state->weighted = (alpha * median) + ((1.0f - alpha) * state->weighted);

    const unsigned long dtMs = (nowMs >= state->lastMs) ? (nowMs - state->lastMs) : 0u;
    const float dtSec = ((float)dtMs) / 1000.0f;
    const float maxStep = rampPctPerSec * dtSec;

    if (state->ramped < state->weighted)
    {
        const float step = state->weighted - state->ramped;
        state->ramped += (step > maxStep) ? maxStep : step;
    }
    else
    {
        const float step = state->ramped - state->weighted;
        state->ramped -= (step > maxStep) ? maxStep : step;
    }

    state->ramped = clamp_pct(state->ramped);
    state->lastMs = nowMs;

    const bool limitAlert = (state->ramped <= 0.1f) || (state->ramped >= 99.9f);

    if (outClampedPct) { *outClampedPct = clamped; }
    if (outMedianPct) { *outMedianPct = median; }
    if (outWeightedPct) { *outWeightedPct = state->weighted; }
    if (outRampedPct) { *outRampedPct = state->ramped; }
    if (outClampAlert) { *outClampAlert = clampAlert; }
    if (outLimitAlert) { *outLimitAlert = limitAlert; }
}
