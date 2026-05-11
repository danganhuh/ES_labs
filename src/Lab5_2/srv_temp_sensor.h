#ifndef LAB5_2_SRV_TEMP_SENSOR_H
#define LAB5_2_SRV_TEMP_SENSOR_H

#include <Arduino.h>

/**
 * SRV layer — DS18B20 temperature sensor service.
 *
 * Wraps the OneWire / DallasTemperature stack, applies the same three-stage
 * signal-conditioning pipeline used by the reference solution
 * (`dd_sns_temperature_loop` → `lib_cond_*`):
 *
 *      raw °C  ─►  saturate(min..max)  ─►  median(window)  ─►  weighted-avg
 *
 *   - saturate clamps wild outliers (CRC-valid but garbage decodes happen).
 *   - median rejects single-bit decode glitches without smearing the signal.
 *   - weighted-average smooths what remains; newer samples weigh higher so
 *     the controller sees a gently-low-passed view of the true PV.
 *
 * Public interface: a single Init/Loop pair + getters. The acquisition task
 * in Lab5_2_main calls Init() once and Loop() at the periodic recurrence;
 * downstream tasks read the filtered PV via TempSensor52_GetTempC().
 */

void  TempSensor52_Init(void);

/**
 * Perform one full read cycle: trigger conversion, fetch raw °C, push
 * through the conditioning pipeline, update internal state.
 * @return  true on a successful read, false on a sensor / bus error
 */
bool  TempSensor52_Loop(void);

/** Latest filtered (median + weighted average) temperature in °C. */
float TempSensor52_GetTempC(void);

/** Latest UNFILTERED sample in °C (after saturation only). Useful for the
 *  Serial Plotter to compare raw vs. filtered convergence. */
float TempSensor52_GetRawTempC(void);

/** True if the last Loop() succeeded. False ⇒ no DS18B20 detected, or the
 *  bus glitched. The caller must treat the temperature getters as stale. */
bool  TempSensor52_IsValid(void);

#endif
