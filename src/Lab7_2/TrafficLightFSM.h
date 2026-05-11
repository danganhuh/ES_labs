/**
 * @file TrafficLightFSM.h
 * @brief Lab7 Part 2 - Traffic Light FSM Control Interface
 * 
 * Implements two independent Moore FSMs for traffic light control:
 * - One FSM for East-West (EW) direction
 * - One FSM for North-South (NS) direction
 * 
 * Each FSM has 3 states:
 *   - GREEN (output=1): Traffic allowed
 *   - YELLOW (output=2): Prepare to stop
 *   - RED (output=0): Traffic stopped
 * 
 * Priority Logic:
 *   - Default: EW stays GREEN, NS stays RED (until request)
 *   - On NS Request: Switch to NS GREEN, EW RED
 *   - After NS cycle: Return to EW GREEN
 * 
 * Sequences:
 *   GREEN (3s) → YELLOW (1s) → RED (4s) → cycle
 * 
 * Architecture: SRV Layer
 *   - Independent FSM instances for each direction
 *   - Shared priority manager for coordination
 */

#ifndef TRAFFIC_LIGHT_FSM_H
#define TRAFFIC_LIGHT_FSM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum TrafficLightState
 * @brief Three-state FSM for traffic light control
 */
typedef enum {
    LIGHT_RED = 0,      // Red: Stop (output = 0)
    LIGHT_GREEN = 1,    // Green: Go (output = 1)
    LIGHT_YELLOW = 2    // Yellow: Caution (output = 2)
} TrafficLightState;

/**
 * @enum TrafficDirection
 * @brief Traffic direction identifier
 */
typedef enum {
    DIRECTION_EW = 0,   // East-West
    DIRECTION_NS = 1    // North-South
} TrafficDirection;

// ============================================================================
// FSM Initialization and Control
// ============================================================================

/**
 * @brief Initialize both traffic light FSMs
 * 
 * Sets up:
 * - EW direction: Initially GREEN
 * - NS direction: Initially RED
 * - Priority: EW has priority
 * 
 * Called once during system setup.
 */
void TrafficLightFSM_Init(void);

/**
 * @brief Reset FSM state for a specific direction
 * 
 * @param direction Target direction (DIRECTION_EW or DIRECTION_NS)
 */
void TrafficLightFSM_Reset(TrafficDirection direction);

// ============================================================================
// State Query Functions
// ============================================================================

/**
 * @brief Get current state of traffic light for a direction
 * 
 * @param direction Target direction
 * @return Current light state (RED, GREEN, or YELLOW)
 */
TrafficLightState TrafficLightFSM_GetState(TrafficDirection direction);

/**
 * @brief Get output value for current state
 * 
 * Moore output depends only on current state:
 * - RED: output = 0
 * - GREEN: output = 1
 * - YELLOW: output = 2
 * 
 * @param direction Target direction
 * @return Output value (0, 1, or 2)
 */
uint8_t TrafficLightFSM_GetOutput(TrafficDirection direction);

/**
 * @brief Get time in current state (milliseconds)
 * 
 * @param direction Target direction
 * @return Time in current state (ms)
 */
uint32_t TrafficLightFSM_GetStateTime(TrafficDirection direction);

/**
 * @brief Get state name for display
 * 
 * @param direction Target direction
 * @return State name string ("RED", "GREEN", or "YELLOW")
 */
const char* TrafficLightFSM_GetStateName(TrafficDirection direction);

// ============================================================================
// FSM State Transition Functions
// ============================================================================

/**
 * @brief Update state based on time elapsed
 * 
 * Implements automatic state transitions:
 * - GREEN (3000ms) → YELLOW
 * - YELLOW (1000ms) → RED
 * - RED (4000ms) → GREEN (back to start)
 * 
 * Must be called periodically (~100ms) from main loop or FreeRTOS task.
 * 
 * @param direction Target direction
 */
void TrafficLightFSM_Update(TrafficDirection direction);

/**
 * @brief Request priority switch (NS requests active)
 * 
 * When North-South traffic requests priority:
 * - Forces EW to begin RED sequence
 * - Allows NS to proceed with GREEN
 * 
 * @param ns_request True if NS requests priority, False to release
 */
void TrafficLightFSM_SetNSRequest(bool ns_request);

/**
 * @brief Check if NS has active request
 * 
 * @return true if North-South is requesting priority
 */
bool TrafficLightFSM_GetNSRequest(void);

/**
 * @brief Check if EW currently has priority
 * 
 * @return true if East-West has priority
 */
bool TrafficLightFSM_IsEWActive(void);

/**
 * @brief Get system state name for debugging
 * 
 * @return Description of current priority state
 */
const char* TrafficLightFSM_GetSystemState(void);

#ifdef __cplusplus
}
#endif

#endif // TRAFFIC_LIGHT_FSM_H
