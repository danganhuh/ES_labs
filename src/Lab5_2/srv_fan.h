#ifndef LAB5_2_SRV_FAN_H
#define LAB5_2_SRV_FAN_H

#include <stdint.h>

/**
 * SRV layer — relay-driven cooling actuator (DC motor / fan via 5 V relay).
 *
 * Functionally this is the "fan" service the rest of the application talks
 * to, but the underlying device is a BINARY relay. PWM would just rattle
 * the coil, so we implement TIME-PROPORTIONAL CONTROL (TPC):
 *
 *   demand = 0%      → relay held OPEN for the whole window
 *   demand = 100%    → relay held CLOSED for the whole window
 *   demand = X%      → relay CLOSED for X% of the window, OPEN for (100−X)%
 *
 * The window length is configurable (`window <ms>` runtime command); the
 * default is LAB5_2_DEFAULT_RELAY_WINDOW_MS. Mechanical inertia of the fan
 * blade smooths the resulting on/off train into an average air-flow that
 * the PID loop perceives as continuous.
 *
 * ON-OFF mode is the degenerate case where the controller already commits
 * to 0 % or 100 % each cycle, so TPC collapses into pure latched behaviour
 * with no extra logic needed here.
 *
 * Polarity is settable at boot (LAB5_2_RELAY_ACTIVE_LOW) and at runtime
 * (`polarity low|high`). Most cheap optocoupled relay modules (including
 * the Wokwi part) are active-LOW; raw transistor-driven boards are
 * active-HIGH. Get this wrong and the motor stays on when the firmware
 * thinks it asked it to stop — which is exactly the failure mode this
 * service is here to prevent.
 */

void Fan52_Init(void);

/**
 * @brief Update the demanded duty (0..100%). Stored internally; the TPC
 *        slicer reads this every Fan52_Loop().
 *
 * Negative values are clamped to 0 (the actuator is unidirectional).
 */
void Fan52_SetDemandPct(int demandPct);

/**
 * @brief Step the TPC slicer one tick.
 *
 * Recomputes the window phase from millis(), decides whether the relay
 * should be closed right now, and drives the GPIO with the appropriate
 * level for the configured polarity.
 *
 * Returns the duty (%) currently being commanded — for telemetry / LCD.
 * The instantaneous relay state can be queried via Fan52_IsRelayOn().
 */
int  Fan52_Loop(void);

/** Latest applied demand (%, 0..100). Same value Fan52_Loop() returned. */
int  Fan52_GetActualPct(void);

/** Force the actuator output to zero immediately (used on sensor failure or
 *  on `force off`). Opens the relay and resets the TPC window so the next
 *  non-zero demand starts a fresh slice. */
void Fan52_HardOff(void);

/**
 * @brief Set the TPC window length, in milliseconds. Clamped to the
 *        LAB5_2_RELAY_WINDOW_MIN_MS..LAB5_2_RELAY_WINDOW_MAX_MS range.
 *        Takes effect on the NEXT window boundary, not mid-window — so
 *        the current slice still plays out cleanly.
 */
void Fan52_SetWindowMs(uint16_t windowMs);

/** Current TPC window length, ms. */
uint16_t Fan52_GetWindowMs(void);

/**
 * @brief Set the relay-board polarity.
 *
 * @param activeLow true ⇒ IN=LOW closes the contact (typical optocoupler
 *                  modules, Wokwi part). false ⇒ IN=HIGH closes the
 *                  contact (raw NPN driver boards).
 *
 * Reapplies the current relay level immediately so the change takes
 * effect on the next slice without waiting for a controller cycle.
 */
void Fan52_SetPolarity(bool activeLow);

/** Current polarity. true = active-LOW, false = active-HIGH. */
bool Fan52_GetPolarity(void);

/** True while the relay contact is currently closed (motor running). */
bool Fan52_IsRelayOn(void);

#endif
