#include "srv_temp_sensor.h"

#include "Lab5_2_Shared.h"

#include <OneWire.h>
#include <DallasTemperature.h>

/* ============================================================================
 * Module statics
 * ==========================================================================*/

static OneWire           s_oneWire(LAB5_2_TEMP_PIN);
static DallasTemperature s_dallas(&s_oneWire);

/* Conditioning pipeline state.
 * The reference uses int FIFOs (lib_cond_*); we keep that for behavioural
 * parity — the °C resolution we care about is 1 °C anyway since the LCD
 * shows whole degrees and the controller's setpoint is integer. The float
 * conversion happens at the boundary of this module. */
static int s_medianFifo [LAB5_2_MEDIAN_WINDOW];   /* most-recent first */
static int s_medianSorted[LAB5_2_MEDIAN_WINDOW];  /* sort scratch space */
static int s_wavgFifo   [LAB5_2_WAVG_WINDOW];
static int s_wavgWeights[LAB5_2_WAVG_WINDOW] = {5, 4, 3, 2, 1};

/* Filled-up tracking — until we have N samples we just pass through, so the
 * filter doesn't latch on the initial 0 °C placeholders. */
static uint8_t s_medianFill = 0;
static uint8_t s_wavgFill   = 0;

static float s_lastRawC      = 0.0f;
static float s_lastFilteredC = 0.0f;
static bool  s_isValid       = false;

/* ============================================================================
 * Conditioning primitives — small in-file copies of the reference lib_cond
 * routines. We could pull lib_cond as a separate file, but inlining here
 * keeps this lab self-contained and avoids littering the project root.
 * ==========================================================================*/

static int saturateInt(int value, int minVal, int maxVal)
{
    if (value < minVal) { return minVal; }
    if (value > maxVal) { return maxVal; }
    return value;
}

/** Push a new value at index 0, shifting older values to the right.
 *  Mirrors `lib_cond_fifo_push`. */
static void fifoPushInt(int *fifo, int size, int value)
{
    for (int i = size - 1; i > 0; --i)
    {
        fifo[i] = fifo[i - 1];
    }
    fifo[0] = value;
}

/** Median of an int FIFO. Mirrors `lib_cond_median_filter`: we work on a
 *  sorted COPY so the FIFO retains chronological order for the next push. */
static int medianFromFifo(const int *fifo, int *scratch, int size)
{
    for (int i = 0; i < size; ++i) { scratch[i] = fifo[i]; }

    /* Bubble-sort — N is small (5), so the O(N^2) is fine and matches the
     * `lib_cond_sort_buffer` implementation. */
    for (int i = 0; i < size - 1; ++i)
    {
        for (int j = 0; j < size - i - 1; ++j)
        {
            if (scratch[j] > scratch[j + 1])
            {
                const int tmp = scratch[j];
                scratch[j]    = scratch[j + 1];
                scratch[j + 1]= tmp;
            }
        }
    }
    return scratch[size / 2];
}

/** Weighted-average. Mirrors `lib_cond_weighted_average_filter`.
 *  The newest sample (index 0) has the largest weight. */
static int weightedAverage(const int *fifo, const int *weights, int size)
{
    long sumWeights = 0;
    long acc        = 0;
    for (int i = 0; i < size; ++i)
    {
        sumWeights += weights[i];
        acc        += (long)fifo[i] * weights[i];
    }
    if (sumWeights <= 0) { return fifo[0]; }
    return (int)(acc / sumWeights);
}

/* ============================================================================
 * Public API
 * ==========================================================================*/

void TempSensor52_Init(void)
{
    for (int i = 0; i < LAB5_2_MEDIAN_WINDOW; ++i)
    {
        s_medianFifo[i] = 0;
        s_medianSorted[i] = 0;
    }
    for (int i = 0; i < LAB5_2_WAVG_WINDOW; ++i)
    {
        s_wavgFifo[i] = 0;
    }
    s_medianFill    = 0;
    s_wavgFill      = 0;
    s_lastRawC      = 0.0f;
    s_lastFilteredC = 0.0f;
    s_isValid       = false;

    s_dallas.begin();
    /* 12-bit resolution (default) — gives 0.0625 °C native step at the cost
     * of ~750 ms conversion time. We sample every 1000 ms anyway. */
    s_dallas.setResolution(12);
    /* `requestTemperatures()` blocks for the conversion time when wait-for-
     * conversion is true. We instead do a non-blocking request in Loop(). */
    s_dallas.setWaitForConversion(true);
}

bool TempSensor52_Loop(void)
{
    s_dallas.requestTemperatures();
    const float rawC = s_dallas.getTempCByIndex(0);

    /* DallasTemperature returns DEVICE_DISCONNECTED_C (-127.0) on error. */
    if (rawC == DEVICE_DISCONNECTED_C || isnan(rawC))
    {
        s_isValid = false;
        return false;
    }

    s_lastRawC = rawC;

    /* --- Stage 1: saturate to plausible band --------------------------- */
    const int rawInt   = (int)(rawC + (rawC >= 0.0f ? 0.5f : -0.5f));
    const int satInt   = saturateInt(rawInt, LAB5_2_TEMP_MIN_C, LAB5_2_TEMP_MAX_C);

    /* --- Stage 2: median filter ---------------------------------------- */
    fifoPushInt(s_medianFifo, LAB5_2_MEDIAN_WINDOW, satInt);
    if (s_medianFill < LAB5_2_MEDIAN_WINDOW) { ++s_medianFill; }

    int medianInt;
    if (s_medianFill < LAB5_2_MEDIAN_WINDOW)
    {
        /* Not enough samples yet — pass the saturated value straight through
         * so the controller sees something sane on the first few cycles. */
        medianInt = satInt;
    }
    else
    {
        medianInt = medianFromFifo(s_medianFifo, s_medianSorted, LAB5_2_MEDIAN_WINDOW);
    }

    /* --- Stage 3: weighted average ------------------------------------- */
    fifoPushInt(s_wavgFifo, LAB5_2_WAVG_WINDOW, medianInt);
    if (s_wavgFill < LAB5_2_WAVG_WINDOW) { ++s_wavgFill; }

    int filteredInt;
    if (s_wavgFill < LAB5_2_WAVG_WINDOW)
    {
        filteredInt = medianInt;
    }
    else
    {
        filteredInt = weightedAverage(s_wavgFifo, s_wavgWeights, LAB5_2_WAVG_WINDOW);
    }

    s_lastFilteredC = (float)filteredInt;
    s_isValid       = true;
    return true;
}

float TempSensor52_GetTempC(void)
{
    return s_lastFilteredC;
}

float TempSensor52_GetRawTempC(void)
{
    return s_lastRawC;
}

bool TempSensor52_IsValid(void)
{
    return s_isValid;
}
