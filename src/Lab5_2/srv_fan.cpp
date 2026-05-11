#include "srv_fan.h"

#include <Arduino.h>

#include "Lab5_2_Shared.h"

/* ============================================================================
 * SRV — Relay actuator with time-proportional control.
 *
 * The fan's "speed" is faked across the configurable LAB5_2_DEFAULT_RELAY_
 * WINDOW_MS window: we open and close the relay so that, on average, the
 * fan spends the right fraction of every window powered on. This is the
 * standard trick for driving slow binary actuators (heaters, valves,
 * mechanical relays) from a continuous control law — same approach the
 * reference Indrumar Lab 5.2 uses for its heater. We just flip the
 * polarity to cooling.
 *
 * ON-OFF mode lands here too. The controller posts 0 % or 100 %, the
 * slicer sees full-OFF / full-ON, and the relay holds steady — exactly
 * what we want.
 * ==========================================================================*/

static int      s_demandPct      = 0;     /* what the controller asked for */
static bool     s_relayOn        = false; /* current contact state         */
static bool     s_activeLow      = (LAB5_2_RELAY_ACTIVE_LOW != 0);
static uint16_t s_windowMs       = LAB5_2_DEFAULT_RELAY_WINDOW_MS;
static unsigned long s_windowStartMs = 0UL;

/* ============================================================================
 * Internal helpers
 * ==========================================================================*/

static int clampInt(int v, int lo, int hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/** Translate a logical "relay closed" intent into the right GPIO level for
 *  the configured polarity, and write it out. Updates s_relayOn. */
static void writeRelay(bool wantClosed)
{
    s_relayOn = wantClosed;
    const int level = s_activeLow ? (wantClosed ? LOW : HIGH)
                                  : (wantClosed ? HIGH : LOW);
    digitalWrite(LAB5_2_RELAY_PIN, level);
}

/* ============================================================================
 * Public API
 * ==========================================================================*/

void Fan52_Init(void)
{
    pinMode(LAB5_2_RELAY_PIN, OUTPUT);

    s_demandPct      = 0;
    s_activeLow      = (LAB5_2_RELAY_ACTIVE_LOW != 0);
    s_windowMs       = LAB5_2_DEFAULT_RELAY_WINDOW_MS;
    s_windowStartMs  = millis();

    /* Open the contact explicitly — never leave the actuator in an
     * undefined state at boot. */
    writeRelay(false);
}

void Fan52_SetDemandPct(int demandPct)
{
    s_demandPct = clampInt(demandPct, 0, 100);
}

int Fan52_Loop(void)
{
    const unsigned long now = millis();

    /* Roll the TPC window when the current one elapses. millis() can wrap
     * (every ~49.7 days on an ATmega2560 with a 1 ms tick) — using unsigned
     * subtraction makes that wrap-safe. */
    if ((now - s_windowStartMs) >= (unsigned long)s_windowMs)
    {
        s_windowStartMs = now;
    }

    /* Pure ON/OFF shortcuts: skip the math and just hold the contact. This
     * matches the ON-OFF controller's binary output and avoids any timing
     * glitch at the window boundary when demand=0 or demand=100. */
    if (s_demandPct <= 0)
    {
        writeRelay(false);
        return 0;
    }
    if (s_demandPct >= 100)
    {
        writeRelay(true);
        return 100;
    }

    /* Time-proportional slicing inside the window:
     *
     *   |---- ON ----|---------- OFF -----------|
     *   0          onMs                       window
     *
     * Resolution is one task period (LAB5_2_FAN_TASK_MS = 100 ms). At the
     * default 2000 ms window that gives 20 distinguishable duties, which
     * is plenty for a fan whose mechanical time-constant is several
     * hundred ms.
     */
    const unsigned long onMs    = ((unsigned long)s_demandPct * (unsigned long)s_windowMs) / 100UL;
    const unsigned long elapsed = now - s_windowStartMs;
    writeRelay(elapsed < onMs);

    return s_demandPct;
}

int Fan52_GetActualPct(void)
{
    return s_demandPct;
}

void Fan52_HardOff(void)
{
    s_demandPct      = 0;
    s_windowStartMs  = millis();
    writeRelay(false);
}

void Fan52_SetWindowMs(uint16_t windowMs)
{
    if (windowMs < LAB5_2_RELAY_WINDOW_MIN_MS) { windowMs = LAB5_2_RELAY_WINDOW_MIN_MS; }
    if (windowMs > LAB5_2_RELAY_WINDOW_MAX_MS) { windowMs = LAB5_2_RELAY_WINDOW_MAX_MS; }
    s_windowMs = windowMs;
    /* Don't reset s_windowStartMs — let the current slice finish naturally.
     * The new length kicks in at the next window roll-over. */
}

uint16_t Fan52_GetWindowMs(void)
{
    return s_windowMs;
}

void Fan52_SetPolarity(bool activeLow)
{
    if (activeLow == s_activeLow) { return; }
    s_activeLow = activeLow;
    /* Re-issue the current relay state so the polarity change is observed
     * immediately, not on the next Loop() call. */
    writeRelay(s_relayOn);
}

bool Fan52_GetPolarity(void)
{
    return s_activeLow;
}

bool Fan52_IsRelayOn(void)
{
    return s_relayOn;
}
