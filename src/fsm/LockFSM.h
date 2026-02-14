/**
 * @file LockFSM.h
 * @brief SRV Layer - Lock Finite State Machine Interface
 *
 * This module defines the interface for the lock system FSM.
 */

#ifndef LOCKFSM_H
#define LOCKFSM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_MENU,
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_INPUT_UNLOCK,
    STATE_INPUT_CHANGE_OLD,
    STATE_INPUT_CHANGE_NEW,
    STATE_ERROR
} LockState;

void LockFSM_Init();
void LockFSM_SelectOperation(char op);
void LockFSM_ProcessInput(const char* input);
LockState LockFSM_GetState();
LockState LockFSM_GetCurrentState();
const char* LockFSM_GetMessage();
bool LockFSM_WasLastOpError();

#ifdef __cplusplus
}
#endif

#endif // LOCKFSM_H
