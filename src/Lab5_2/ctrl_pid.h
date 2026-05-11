#ifndef LAB5_2_CTRL_PID_H
#define LAB5_2_CTRL_PID_H

/**
 * Discrete PID controller for a unidirectional cooling actuator.
 *
 * Implements the canonical formula given in the methodological guide:
 *
 *     R(t) = Kp·e(t) + Ki·∑e(n·Δt)·Δt + Kd·(e(t) - e(t-Δt))/Δt
 *
 * with two hardening tweaks borrowed from the reference solution
 * (`app_lab_6_2_i_ctrl_dc_motor`) and from textbook practice:
 *
 *   1. Anti-windup on the integral. The accumulator is clamped to
 *      ±LAB5_2_PID_INTEGRAL_LIMIT (default 100) so the I term cannot
 *      saturate and "wind up" while the actuator is already at full power.
 *      The reference does the simpler "clamp at +100" because its plant
 *      is unidirectional too — we replicate that for behavioural match.
 *
 *   2. Output clamp to ±limitAbs (default 100, in % fan power). The PID
 *      output is the abstract demand; the fan service interprets it as a
 *      duty cycle in [0..100] (negative ⇒ fan off, see Step()).
 *
 * Sign convention used in this lab (cooling):
 *   error = PV - SP   (positive when room is too hot ⇒ controller pushes
 *                      fan power up)
 *
 * Reset()/Configure() let the command parser swap gains at runtime without
 * tearing the task down.
 */
class PidController52
{
public:
    void  Init(float kp, float ki, float kd, float limitAbs, float initialOutput);
    void  Reset(float initialOutput);
    void  Configure(float kp, float ki, float kd, float limitAbs);

    /**
     * @brief Step the controller one tick.
     * @param error        PV - SP, in °C
     * @param dtSeconds    elapsed time since last Step (clamped to ≥ 1 ms)
     * @return             saturated controller output (e.g. % fan power)
     */
    float Step(float error, float dtSeconds);

private:
    float clamp(float value, float limitAbs) const;

    float kp;
    float ki;
    float kd;
    float limitAbs;

    float integral;
    float prevError;
    bool  initialized;
};

#endif
