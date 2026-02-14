# Test Scenarios and Validation

## Test Environment
- Hardware: Arduino Uno + 4x4 Keypad + 16x2 LCD + Red/Green LEDs
- Simulator: Wokwi (optional)
- Default Password: `1234`

## Test Scenario 1: System Initialization

### Test 1.1 - Power On
1. Apply power to system
2. **Expected:** LCD shows "Smart Lock" and "Press * for menu"
3. **Expected:** Both LEDs are OFF
4. **Expected:** System is in LOCKED state
5. **Status:** ✓ Pass/Fail ___

### Test 1.2 - Menu Access
1. Press `*` key
2. **Expected:** LCD shows menu options "*0:Lock *1:Unlock" and "*2:ChgPass *3:Stat"
3. **Expected:** No LED activity
4. **Status:** ✓ Pass/Fail ___

## Test Scenario 2: Lock/Unlock Operations

### Test 2.1 - Lock System (Operation 0)
1. Press `*`, then press `0`, then press `#`
2. **Expected:** LCD shows "System Locked"
3. **Expected:** Red LED blinks for ~1 second
4. **Expected:** System returns to menu prompt
5. **Status:** ✓ Pass/Fail ___

### Test 2.2 - Unlock with Correct Password
1. Press `*`, then press `1`, then press `#`
2. **Expected:** LCD shows "Enter pass:"
3. **Expected:** Can type password (shown as "Cmd:1234")
4. Press `1234` then `#`
5. **Expected:** LCD shows "Access Granted!"
6. **Expected:** Green LED blinks for ~1 second
7. **Expected:** Lock state changes to UNLOCKED
8. **Expected:** System returns to menu
9. **Status:** ✓ Pass/Fail ___

### Test 2.3 - Unlock with Wrong Password
1. Press `*`, then press `1`, then press `#`
2. **Expected:** LCD shows "Enter pass:"
3. Press `9999` then `#`
4. **Expected:** LCD shows "Wrong Code!"
5. **Expected:** Red LED blinks for ~1 second (error feedback)
6. **Expected:** Lock state remains LOCKED
7. **Expected:** System returns to menu
8. **Status:** ✓ Pass/Fail ___

### Test 2.4 - Empty Password Input
1. Press `*`, then press `1`, then press `#`
2. Press `#` immediately (without entering password)
3. **Expected:** LCD shows error or returns to menu
4. **Status:** ✓ Pass/Fail ___

## Test Scenario 3: Password Change

### Test 3.1 - Change Password with Correct Old Password
1. Press `*`, then press `2`, then press `#`
2. **Expected:** LCD shows "Old pass:"
3. Press `1234` (correct old password) then `#`
4. **Expected:** LCD shows "New pass:"
5. Press `5678` (new password) then `#`
6. **Expected:** LCD shows "Pass Changed!"
7. **Expected:** Green LED blinks for ~1 second
8. **Expected:** New password is now `5678`
9. **Expected:** System returns to menu
10. **Status:** ✓ Pass/Fail ___

### Test 3.2 - Change Password with Wrong Old Password
1. Press `*`, then press `2`, then press `#`
2. **Expected:** LCD shows "Old pass:"
3. Press `9999` (wrong password) then `#`
4. **Expected:** LCD shows "Wrong Old Pass!"
5. **Expected:** Red LED blinks for ~1 second (error feedback)
6. **Expected:** Password remains unchanged
7. **Expected:** System returns to menu
8. **Status:** ✓ Pass/Fail ___

### Test 3.3 - Verify New Password Works
1. After Test 3.1, press `*`, then press `1`, then press `#`
2. Press `5678` (new password) then `#`
3. **Expected:** LCD shows "Access Granted!"
4. **Expected:** Green LED blinks
5. **Status:** ✓ Pass/Fail ___

## Test Scenario 4: Status Display

### Test 4.1 - Display Status (LOCKED)
1. System in LOCKED state
2. Press `*`, then press `3`, then press `#`
3. **Expected:** LCD shows "Status: LOCKED"
4. **Expected:** No LED activity
5. **Expected:** System returns to menu
6. **Status:** ✓ Pass/Fail ___

### Test 4.2 - Display Status (UNLOCKED)
1. Unlock system (Test 2.2)
2. Press `*`, then press `3`, then press `#`
3. **Expected:** LCD shows "Status: OPEN" or "Status: UNLOCKED"
4. **Expected:** No LED activity
5. **Status:** ✓ Pass/Fail ___

## Test Scenario 5: Key Debouncing

### Test 5.1 - Fast Key Press
1. Rapidly press same key multiple times
2. **Expected:** Only ONE key press is registered per physical press
3. **Expected:** No duplicate key entries
4. **Notes:** 20ms software debounce delay prevents false triggers
5. **Status:** ✓ Pass/Fail ___

### Test 5.2 - Key Bounce Detection
1. Press and hold key for ~100ms
2. **Expected:** Key is registered only ONCE
3. **Expected:** No repeating key activity
4. **Status:** ✓ Pass/Fail ___

## Test Scenario 6: User Feedback

### Test 6.1 - LCD Message Display
1. Perform various operations
2. **Expected:** LCD always shows relevant current state/message
3. **Expected:** Menu options clear and readable
4. **Expected:** Error messages clear
5. **Status:** ✓ Pass/Fail ___

### Test 6.2 - LED Feedback Timing
1. Perform unlock operation with correct password
2. **Expected:** Green LED turns on
3. **Expected:** Green LED stays on for approximately 1 second
4. **Expected:** Green LED automatically turns off
5. **Expected:** No blocking occurs (responsive to new input)
6. **Status:** ✓ Pass/Fail ___

### Test 6.3 - Error LED Timing
1. Perform unlock operation with wrong password
2. **Expected:** Red LED turns on
3. **Expected:** Red LED stays on for approximately 1 second
4. **Expected:** Red LED automatically turns off
5. **Status:** ✓ Pass/Fail ___

## Test Scenario 7: Latency and Responsiveness

### Test 7.1 - Keypad Response Time
1. Press a key
2. **Expected:** LCD updates within 50ms
3. **Notes:** Debouncing adds ~20ms, FSM processing <1ms
4. **Total latency:** < 100ms (requirement met)
5. **Status:** ✓ Pass/Fail ___

### Test 7.2 - LED Response Time
1. Press key that triggers LED feedback (#)
2. **Expected:** LED turns on within 5ms of command execution
3. **Status:** ✓ Pass/Fail ___

### Test 7.3 - Menu Navigation Responsiveness
1. Press `*` to open menu
2. Rapidly press operation keys
3. **Expected:** System responds to each press
4. **Expected:** No commands are missed
5. **Status:** ✓ Pass/Fail ___

## Test Scenario 8: Edge Cases

### Test 8.1 - Multiple Menu Opens
1. Press `*` multiple times rapidly
2. **Expected:** Menu displays each time, no corruption
3. **Status:** ✓ Pass/Fail ___

### Test 8.2 - Invalid Operation Selection
1. Press `*`, then press invalid key (e.g., 5), then press `#`
2. **Expected:** LCD shows "Invalid Cmd!" or returns to menu
3. **Expected:** Red LED may indicate error
4. **Status:** ✓ Pass/Fail ___

### Test 8.3 - Partial Command Entry
1. Press `*`, then press `1`
2. Don't press `#`, instead press `*` again
3. **Expected:** Menu resets/displays properly
4. **Status:** ✓ Pass/Fail ___

## Test Scenario 9: Long Duration Test

### Test 9.1 - 5 Minute Operation
1. Perform normal operations for 5 minutes
2. Lock/unlock multiple times
3. Change password
4. Check status several times
5. **Expected:** System operates reliably without hangs or crashes
6. **Expected:** All LEDs function consistently
7. **Expected:** LCD displays correctly
8. **Status:** ✓ Pass/Fail ___

## Test Summary

| Scenario | Total Tests | Pass | Fail |
|----------|-------------|------|------|
| Initialization | 2 | ___ | ___ |
| Lock/Unlock | 4 | ___ | ___ |
| Password Change | 3 | ___ | ___ |
| Status | 2 | ___ | ___ |
| Debouncing | 2 | ___ | ___ |
| User Feedback | 3 | ___ | ___ |
| Latency | 3 | ___ | ___ |
| Edge Cases | 3 | ___ | ___ |
| Duration | 1 | ___ | ___ |
| **TOTAL** | **23** | ___ | ___ |

### Overall Result: ___________

**Date:** ____________ **Tester:** ____________

---

## Notes and Observations

(Use this section to document any issues, unexpected behaviors, or observations during testing)

