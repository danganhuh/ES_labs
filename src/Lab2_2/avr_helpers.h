/**
 * @file avr_helpers.h
 * @brief C-callable wrappers for Arduino functions
 *
 * Since Arduino.h is C++-only, .c files cannot include it directly.
 * These wrapper functions (implemented in main.cpp with extern "C" linkage)
 * provide access to Arduino hardware APIs from plain C code.
 */

#ifndef AVR_HELPERS_H
#define AVR_HELPERS_H

#include <stdint.h>

/* Arduino pin-mode constants (must match Arduino.h values) */
#define PIN_MODE_OUTPUT       1
#define PIN_MODE_INPUT_PULLUP 2

/* Logic levels */
#define PIN_HIGH 1
#define PIN_LOW  0

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO */
void     hw_pin_mode(uint8_t pin, uint8_t mode);
void     hw_digital_write(uint8_t pin, uint8_t val);
uint8_t  hw_digital_read(uint8_t pin);

/* Timing */
unsigned long hw_millis(void);
void          hw_delay_ms(unsigned long ms);

/* Serial debug output */
void dbg_print(const char *s);
void dbg_print_num(long n);
void dbg_println(const char *s);
void dbg_println_num(long n);
void dbg_println_empty(void);
void dbg_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* AVR_HELPERS_H */
