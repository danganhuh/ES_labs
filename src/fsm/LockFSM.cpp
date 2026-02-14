/**
 * @file LockFSM.cpp
 * @brief SRV Layer - Lock Finite State Machine Implementation
 */

#include "LockFSM.h"
#include <string.h>
#include <stdio.h>

static LockState currentState = STATE_MENU;
static LockState lockStatus = STATE_LOCKED;
static char password[10] = "1234";
static char tempOldPass[10];
static char msg[32];
static bool lastOpWasError = false;

void LockFSM_Init() {
    currentState = STATE_MENU;
    lockStatus = STATE_LOCKED;
    strcpy(password, "1234");
    strcpy(msg, "Enter command");
}

LockState LockFSM_GetState() {
    return lockStatus;
}

LockState LockFSM_GetCurrentState() {
    return currentState;
}

const char* LockFSM_GetMessage() {
    return msg;
}

bool LockFSM_WasLastOpError() {
    return lastOpWasError;
}

void LockFSM_SelectOperation(char op) {
    lastOpWasError = false;
    switch (op) {
        case '0':
            lockStatus = STATE_LOCKED;
            currentState = STATE_LOCKED;
            strcpy(msg, "System Locked");
            break;
        case '1':
            currentState = STATE_INPUT_UNLOCK;
            strcpy(msg, "Enter pass:");
            break;
        case '2':
            currentState = STATE_INPUT_CHANGE_OLD;
            strcpy(msg, "Old pass:");
            break;
        case '3':
            snprintf(msg, sizeof(msg), "Status: %s", lockStatus == STATE_LOCKED ? "LOCKED" : "OPEN");
            currentState = STATE_MENU;
            break;
        default:
            currentState = STATE_ERROR;
            strcpy(msg, "Invalid Cmd!");
            lastOpWasError = true;
            break;
    }
}

void LockFSM_ProcessInput(const char* input) {
    if (currentState == STATE_INPUT_UNLOCK) {
        if (strcmp(input, password) == 0) {
            lockStatus = STATE_UNLOCKED;
            currentState = STATE_MENU;
            strcpy(msg, "Access Granted!");
            lastOpWasError = false;
        } else {
            currentState = STATE_MENU;
            strcpy(msg, "Wrong Code!");
            lastOpWasError = true;
        }
    } else if (currentState == STATE_INPUT_CHANGE_OLD) {
        if (strcmp(input, password) == 0) {
            currentState = STATE_INPUT_CHANGE_NEW;
            strcpy(tempOldPass, input);
            strcpy(msg, "New pass:");
            lastOpWasError = false;
        } else {
            currentState = STATE_MENU;
            strcpy(msg, "Wrong Old Pass!");
            lastOpWasError = true;
        }
    } else if (currentState == STATE_INPUT_CHANGE_NEW) {
        strcpy(password, input);
        currentState = STATE_MENU;
        strcpy(msg, "Pass Changed!");
        strcpy(msg, "Pass Changed!");
    }
}
