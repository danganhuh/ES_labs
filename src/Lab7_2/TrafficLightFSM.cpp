/**
 * @file TrafficLightFSM.cpp
 * @brief Lab7 Part 2 - Traffic Light FSM Implementation
 * 
 * Implements two independent Moore FSMs for traffic light control.
 * Each direction cycles: GREEN (3s) → YELLOW (1s) → RED (4s)
 * 
 * State Tables:
 * ┌───────────┬────────┬─────────────────────────────────┐
 * │ Num │ Name    │ Output │ Duration │ Next State      │
 * ├─────┼─────────┼────────┼──────────┼─────────────────┤
 * │ 0   │ RED     │ 0      │ 4000ms   │ GREEN           │
 * │ 1   │ GREEN   │ 1      │ 3000ms   │ YELLOW          │
 * │ 2   │ YELLOW  │ 2      │ 1000ms   │ RED             │
 * └─────┴─────────┴────────┴──────────┴─────────────────┘
 * 
 * Priority Logic:
 * - Default: EW active (GREEN), NS inactive (RED)
 * - Request: NS sends request → EW transitions to RED, NS to GREEN
 * - Release: After NS completes cycle → EW resumes GREEN
 */

#include "TrafficLightFSM.h"
#include <Arduino.h>
#include <stdio.h>

// ============================================================================
// FSM State Definition
// ============================================================================

typedef struct {
    TrafficLightState current_state;
    uint8_t output;
    uint32_t state_duration_ms;
    uint32_t state_entered_ms;
} FSMDirectionState;

typedef struct {
    uint8_t output;                           // Output in this state
    TrafficLightState next_state;             // Next state
    uint32_t duration_ms;                     // Time to stay in this state
} TrafficLightFSMStateTransition;

// ============================================================================
// State Transition Tables
// ============================================================================

static const TrafficLightFSMStateTransition FSM_LIGHT_TABLE[3] = {
    // State 0: RED
    {
        .output = 0,
        .next_state = LIGHT_GREEN,
        .duration_ms = 4000  // 4 seconds
    },
    // State 1: GREEN
    {
        .output = 1,
        .next_state = LIGHT_YELLOW,
        .duration_ms = 3000  // 3 seconds
    },
    // State 2: YELLOW
    {
        .output = 2,
        .next_state = LIGHT_RED,
        .duration_ms = 1000  // 1 second
    }
};

// ============================================================================
// FSM Runtime State
// ============================================================================

static FSMDirectionState g_fsm_ew = {
    .current_state = LIGHT_GREEN,
    .output = 1,
    .state_duration_ms = 3000,
    .state_entered_ms = 0
};

static FSMDirectionState g_fsm_ns = {
    .current_state = LIGHT_RED,
    .output = 0,
    .state_duration_ms = 4000,
    .state_entered_ms = 0
};

// Priority management
static bool g_ns_request = false;
static bool g_ew_active = true;

// ============================================================================
// Helper Functions
// ============================================================================

static FSMDirectionState* get_direction_state(TrafficDirection direction) {
    return (direction == DIRECTION_EW) ? &g_fsm_ew : &g_fsm_ns;
}

// ============================================================================
// FSM Interface Implementation
// ============================================================================

void TrafficLightFSM_Init(void) {
    // Initialize EW: Green (active)
    g_fsm_ew.current_state = LIGHT_GREEN;
    g_fsm_ew.output = FSM_LIGHT_TABLE[LIGHT_GREEN].output;
    g_fsm_ew.state_duration_ms = FSM_LIGHT_TABLE[LIGHT_GREEN].duration_ms;
    g_fsm_ew.state_entered_ms = millis();
    
    // Initialize NS: Red (inactive)
    g_fsm_ns.current_state = LIGHT_RED;
    g_fsm_ns.output = FSM_LIGHT_TABLE[LIGHT_RED].output;
    g_fsm_ns.state_duration_ms = FSM_LIGHT_TABLE[LIGHT_RED].duration_ms;
    g_fsm_ns.state_entered_ms = millis();
    
    // Initialize priority
    g_ns_request = false;
    g_ew_active = true;
}

void TrafficLightFSM_Reset(TrafficDirection direction) {
    FSMDirectionState* state = get_direction_state(direction);
    
    if (direction == DIRECTION_EW) {
        state->current_state = LIGHT_GREEN;
    } else {
        state->current_state = LIGHT_RED;
    }
    
    state->output = FSM_LIGHT_TABLE[state->current_state].output;
    state->state_duration_ms = FSM_LIGHT_TABLE[state->current_state].duration_ms;
    state->state_entered_ms = millis();
}

TrafficLightState TrafficLightFSM_GetState(TrafficDirection direction) {
    return get_direction_state(direction)->current_state;
}

uint8_t TrafficLightFSM_GetOutput(TrafficDirection direction) {
    return get_direction_state(direction)->output;
}

uint32_t TrafficLightFSM_GetStateTime(TrafficDirection direction) {
    FSMDirectionState* state = get_direction_state(direction);
    return (millis() - state->state_entered_ms);
}

const char* TrafficLightFSM_GetStateName(TrafficDirection direction) {
    TrafficLightState state = get_direction_state(direction)->current_state;
    
    switch (state) {
        case LIGHT_RED:    return "RED";
        case LIGHT_GREEN:  return "GREEN";
        case LIGHT_YELLOW: return "YELLOW";
        default:           return "UNKNOWN";
    }
}

void TrafficLightFSM_Update(TrafficDirection direction) {
    FSMDirectionState* state = get_direction_state(direction);
    uint32_t time_in_state = millis() - state->state_entered_ms;
    
    // Check if state duration has expired
    if (time_in_state >= state->state_duration_ms) {
        // Transition to next state
        TrafficLightState next_state = FSM_LIGHT_TABLE[state->current_state].next_state;
        
        state->current_state = next_state;
        state->output = FSM_LIGHT_TABLE[next_state].output;
        state->state_duration_ms = FSM_LIGHT_TABLE[next_state].duration_ms;
        state->state_entered_ms = millis();
    }
}

void TrafficLightFSM_SetNSRequest(bool ns_request) {
    g_ns_request = ns_request;
    
    if (ns_request) {
        // NS is requesting priority
        g_ew_active = false;
        
        // Force EW to RED sequence if not already
        if (g_fsm_ew.current_state != LIGHT_RED) {
            g_fsm_ew.current_state = LIGHT_RED;
            g_fsm_ew.output = 0;
            g_fsm_ew.state_duration_ms = FSM_LIGHT_TABLE[LIGHT_RED].duration_ms;
            g_fsm_ew.state_entered_ms = millis();
        }
        
        // Allow NS to proceed if not already green
        if (g_fsm_ns.current_state == LIGHT_RED) {
            g_fsm_ns.current_state = LIGHT_GREEN;
            g_fsm_ns.output = 1;
            g_fsm_ns.state_duration_ms = FSM_LIGHT_TABLE[LIGHT_GREEN].duration_ms;
            g_fsm_ns.state_entered_ms = millis();
        }
    } else {
        // NS request released
        // Check if NS has finished its cycle
        if (g_fsm_ns.current_state == LIGHT_RED && 
            (millis() - g_fsm_ns.state_entered_ms) > FSM_LIGHT_TABLE[LIGHT_RED].duration_ms) {
            // NS has completed its cycle, restore EW priority
            g_ew_active = true;
            
            if (g_fsm_ew.current_state == LIGHT_RED) {
                g_fsm_ew.current_state = LIGHT_GREEN;
                g_fsm_ew.output = 1;
                g_fsm_ew.state_duration_ms = FSM_LIGHT_TABLE[LIGHT_GREEN].duration_ms;
                g_fsm_ew.state_entered_ms = millis();
            }
        }
    }
}

bool TrafficLightFSM_GetNSRequest(void) {
    return g_ns_request;
}

bool TrafficLightFSM_IsEWActive(void) {
    return g_ew_active;
}

const char* TrafficLightFSM_GetSystemState(void) {
    if (g_ns_request) {
        return "NS_PRIORITY";
    } else if (g_ew_active) {
        return "EW_PRIORITY";
    } else {
        return "TRANSITION";
    }
}
