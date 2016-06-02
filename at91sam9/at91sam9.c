
#include "../ymodem_boot.h"

#define PIT_MR (*(volatile uint32_t*)0xFFFFFE30)

#define PIT_MR_PITEN (1 << 24)
#define PIT_MR_PIV_MASK 0xfffff

#define PIT_PIIR (*(volatile uint32_t*)0xFFFFFE3C)

#define PMC_PCR (*(volatile uint32_t*)0xFFFFFD0C)

#define PMC_PCR_EN (1 << 28)
#define PMC_PCR_CMD_WRITE (1 << 12)
#define PERIPH_ID_SYS 1


void hw_init(void) {
	/* enable the periodic interval timer */ 
	PIT_MR = PIT_MR_PIV_MASK | PIT_MR_PITEN;
	/* enable the SYS clock */
	PMC_PCR = PMC_PCR_EN | PMC_PCR_CMD_WRITE | PERIPH_ID_SYS;
}

uint32_t usecs_to_pit_steps(uint32_t usecs) {
	/* MCLK is around 133MHz, and the clock is further divided by 16 before
	 * passed to the PIT. Therefore, one microsecond equals around 133/16
	 * counter steps. */
	return usecs * 133 / 16;
}

#define DBGU_SR (*(volatile uint32_t*)0xfffff214)
#define DBGU_RHR (*(volatile uint32_t*)0xfffff218)
#define DBGU_THR (*(volatile uint32_t*)0xfffff21c)

#define DBGU_SR_RXRDY 0x1
#define DBGU_SR_TXRDY 0x2

static uint8_t input_buffer[4096];
static uint32_t input_size = 0;
static uint32_t input_write_cursor = 0;
static uint32_t input_read_cursor = 0;

static void uart_receive(void) {
	if (input_size == sizeof(input_buffer)) {
		return;
	}
	if ((DBGU_SR & DBGU_SR_RXRDY) == 0) {
		return;
	}
	input_buffer[input_write_cursor] = DBGU_RHR;
	input_write_cursor =
			(input_write_cursor + 1) % sizeof(input_buffer);
	input_size++;
}

int uart_read_byte(uint8_t *b) {
	uint32_t timeout = PIT_PIIR + usecs_to_pit_steps(500000);
	while (input_size == 0 && ((int32_t)(timeout - PIT_PIIR) > 0)) {
		uart_receive();
	}
	if (input_size != 0) {
		*b = input_buffer[input_read_cursor];
		input_read_cursor =
				(input_read_cursor + 1) % sizeof(input_buffer);
		input_size--;
		return 1;
	} else {
		return 0;
	}
}

void uart_write_byte(uint8_t b) {
	/* read incoming data to prevent FIFO overflows */
	uart_receive();
	while ((DBGU_SR & DBGU_SR_TXRDY) == 0) {
		uart_receive();
	}
	DBGU_THR = b;
}

void uart_discard_input(void) {
	/* drain the hardware FIFO */
	while ((DBGU_SR & DBGU_SR_RXRDY) != 0) {
		DBGU_RHR;
	}
	/* discard buffer contents */
	input_size = 0;
}

#define RSTC_CR (*((volatile uint32_t*)0xfffffe00))
#define RSTC_CR_PROCRST 0x1
#define RSTC_CR_PERRST 0x4
#define RSTC_CR_PASSWD 0xa5000000

void reset(void) {
	RSTC_CR = RSTC_CR_PROCRST | RSTC_CR_PERRST | RSTC_CR_PASSWD;
	for (;;);
}

