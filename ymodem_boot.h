#ifndef YMODEM_BOOT_H_INCLUDED
#define YMODEM_BOOT_H_INCLUDED

#include "stdint.h"

void hw_init(void);

int uart_read_byte(uint8_t *b);
void uart_write_byte(uint8_t b);
void uart_discard_input(void);

void reset(void) __attribute__((noreturn));

void print_hex(uint32_t value);
void print_str(const char *s);

int ymodem_receive(uint8_t *dest, uint32_t *size);

void memmove(uint8_t *dest, const uint8_t *src, uint32_t size);

#endif
