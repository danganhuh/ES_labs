# Smart Lock System - Software Architecture Documentation

## 1. Overview

This document describes the modular architecture of the advanced menu-driven smart lock system (Lab 1.2) using an Arduino Uno with a 4x4 keypad, 16x2 LCD display, and status LEDs.

## 2. Architecture Layers (MCAL/ECAL/SRV Model)

### 2.1 MCAL (Microcontroller Abstraction Layer)
Direct Arduino hardware functions (digitalWrite, pinMode, digitalRead, millis, Serial functions).

### 2.2 ECAL (Electronic Control Abstraction Layer)
Hardware drivers abstracted into reusable modules:

- **LedDriver** (`src/led/LedDriver.h|cpp`)
  - Controls GPIO-connected LEDs
  - Methods: Init(), On(), Off(), Toggle()
  - Used by: FSM layer for visual feedback

- **KeypadDriver** (`src/keypad/KeypadDriver.h|cpp`)
  - Reads 4x4 matrix keypad with software debouncing (20ms)
  - Methods: Init(), GetKey()
  - Debouncing prevents false key transitions

- **LcdDriver** (`src/lcd/LcdDriver.h|cpp`)
  - Controls 16x2 LC display via I2C/parallel
  - Methods: Init(), Print(), Clear(), SetCursor()
  - Displays system state and user messages

### 2.3 SRV (Service/Business Logic Layer)
- **LockFSM** (`src/fsm/LockFSM.h|cpp`)
  - Implements finite state machine for lock control
  - States: MENU, LOCKED, UNLOCKED, INPUT_UNLOCK, INPUT_CHANGE_OLD, INPUT_CHANGE_NEW, ERROR
  - Functions: Init(), SelectOperation(), ProcessInput(), GetState(), WasLastOpError(), GetMessage()
  - Tracks: Lock status, password state, operation success/failure

- **Lab1_2_main** (`src/Lab1_2/Lab1_2_main.cpp`)
  - Application layer: integrates all drivers and FSM
  - Implements user interaction flow
  - Manages LED feedback timing (non-blocking)
  - Handles menu navigation and state transitions

## 3. Hardware Configuration

| Component | Pin(s) | Type | Driver |
|-----------|--------|------|--------|
| LED Green | 10 | GPIO | LedDriver |
| LED Red | 13 | GPIO | LedDriver |
| Keypad Rows | A0-A3 | GPIO | KeypadDriver |
| Keypad Cols | 6-9 | GPIO | KeypadDriver |
| LCD RS | 12 | GPIO | LcdDriver |
| LCD EN | 11 | GPIO | LcdDriver |
| LCD D4-D7 | 5,4,3,2 | GPIO | LcdDriver |

## 4. Finite State Machine Diagram

```
                    ┌─────────────────┐
                    │    STATE_MENU   │
                    └────────┬────────┘
                             │
                ┌────────────┼────────────┐
                │            │            │
        Press 0 │    Press 1 │    Press 2 │    Press 3
                ▼            ▼            ▼            ▼
          [LOCKED]   [INPUT_UNLOCK]  [INPUT_CHANGE_OLD]  [SHOW_STATUS]
                │            │              │               │
                │      [correct password]   │[correct pass]  │
                │            │              ▼               │
                │            ├─────────► [INPUT_CHANGE_NEW] │
                │            │              │               │
                │      [wrong password]     │               │
                │            ▼              ▼               │
                │         [Error]        [Error]            │
                │            │              │               │
                └────────────┴──────┬───────┴───────────────┘
                                    │
                                 [MENU]
                                    ▲
                                    │
                         Return after operation
```

## 5. State Transitions and Events

| Current State | Event | Action | Next State | LED Feedback |
|---------------|-------|--------|-----------|--------------|
| MENU | Press * | Show menu | MENU | None |
| MENU | Press 0# | Lock system | LOCKED | Red (1s) |
| MENU | Press 1# | Request password | INPUT_UNLOCK | None |
| INPUT_UNLOCK | Enter password# | Check password | MENU | Green (correct) or Red (wrong) |
| MENU | Press 2# | Request old pass | INPUT_CHANGE_OLD | None |
| INPUT_CHANGE_OLD | Enter password# | Check password | INPUT_CHANGE_NEW or MENU (error) | Red (error) |
| INPUT_CHANGE_NEW | Enter new pass# | Update password | MENU | Green |
| MENU | Press 3# | Show status | MENU | None |

## 6. Software Modules and Interfaces

### 6.1 LedDriver Interface
```cpp
class LedDriver {
  public:
    LedDriver(uint8_t pin);
    void Init();           // Configure GPIO, set to OFF
    void On();             // Set HIGH
    void Off();            // Set LOW
    void Toggle();         // Invert state
};
```

### 6.2 KeypadDriver Interface
```cpp
class KeypadDriver {
  public:
    KeypadDriver(char (*keymap)[4], const byte* rows, 
                 const byte* cols, byte nRows, byte nCols);
    void Init();           // Initialize keypad hardware
    char GetKey();         // Get debounced key press
};
```
**Debouncing Logic:**
- 20ms software debounce delay
- Prevents false key transitions from bouncing contacts
- Ensures reliable key detection

### 6.3 LcdDriver Interface
```cpp
class LcdDriver {
  public:
    LcdDriver(uint8_t rs, uint8_t en, uint8_t d4, 
              uint8_t d5, uint8_t d6, uint8_t d7);
    void Init();                    // Initialize LCD
    void Print(const char* l1, const char* l2);  // 2-line display
    void Clear();                   // Clear display
    void SetCursor(uint8_t col, uint8_t row);
};
```

### 6.4 LockFSM Interface
```cpp
void LockFSM_Init();
void LockFSM_SelectOperation(char op);  // 0=Lock, 1=Unlock, 2=ChgPass, 3=Status
void LockFSM_ProcessInput(const char* input);
LockState LockFSM_GetState();           // Returns lock status (LOCKED/UNLOCKED)
LockState LockFSM_GetCurrentState();    // Returns current FSM state
bool LockFSM_WasLastOpError();          // Check if last operation failed
const char* LockFSM_GetMessage();       // Get feedback message for display
```

## 7. Key Design Features

### 7.1 Modularity
- Each hardware component in its own driver module
- FSM logic completely separate from I/O handling
- Main application layer orchestrates all modules
- Easy to extend or replace any component

### 7.2 Non-Blocking Feedback
- LED feedback timing based on `millis()` not `delay()`
- Main loop continues running during feedback display
- Allows responsive real-time interaction

### 7.3 Software Debouncing
- KeypadDriver implements 20ms debounce delay
- Prevents false key releases from being detected
- Improves reliability for noisy mechanical switches

### 7.4 Clear State Management
- FSM separates lock status from operational state
- Error tracking for UI feedback
- Message buffer for LCD display

## 8. Response Latency Analysis

**System Response Time:**
- Keypad key press detected: < 1ms (hardware polling)
- Debouncing delay: 20ms (configurable)
- FSM processing: < 1ms (simple state machine)
- LCD update: < 10ms (standard display timing)
- LED update: immediate (GPIO write)

**Total typical latency: ~30-40ms** (well under 100ms requirement)

## 9. Testing Scenarios

See `TEST_SCENARIOS.md` for detailed test cases and validation procedures.

## 10. Component Reusability

These drivers can be reused in other projects:
- **LedDriver**: Any GPIO-controlled LED application
- **KeypadDriver**: Any 4x4 or NxM keypad application (configurable)
- **LcdDriver**: Any character LCD with parallel/I2C interface
- **LockFSM**: Any door lock or access control system

Simply adjust pin definitions and keymap as needed for different hardware configurations.
