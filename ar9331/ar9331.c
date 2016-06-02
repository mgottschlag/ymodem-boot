
#include "../ymodem_boot.h"

uint32_t read_c0_count(void) {
	uint32_t value;
	asm volatile("mfc0 %0, $9" : "=r"(value));
	return value;
}

void hw_init(void) {
}

uint32_t usecs_to_counter_steps(uint32_t usecs) {
	/* the count register is incremented every other clock cycle, so the
	 * counter frequency is 200MHz in a 400MHz system */
	return usecs * 200;
}

#define UART_DATA (*(volatile uint32_t*)0xb8020000)
#define UART_CS (*(volatile uint32_t*)0x18020004)

#define UART_DATA_RX_CSR (1 << 8)
#define UART_DATA_TX_CSR (1 << 9)

#define UART_CS_TX_BUSY (1 << 14)

static uint8_t input_buffer[4096];
static uint32_t input_size = 0;
static uint32_t input_write_cursor = 0;
static uint32_t input_read_cursor = 0;

static void uart_receive(void) {
	uint32_t data = UART_DATA;
	if (input_size != sizeof(input_buffer) &&
			(data & UART_DATA_RX_CSR) != 0) {
		input_buffer[input_write_cursor] = data & 0xff;
		UART_DATA = UART_DATA_RX_CSR;
		input_write_cursor =
				(input_write_cursor + 1) % sizeof(input_buffer);
		input_size++;
	}
}

int uart_read_byte(uint8_t *b) {
	uint32_t timeout = read_c0_count() + usecs_to_counter_steps(500000);
	while (input_size == 0 && ((int32_t)(timeout - read_c0_count()) > 0)) {
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
	while ((UART_DATA & UART_DATA_TX_CSR) == 0) {
		uart_receive();
	}
	UART_DATA = UART_DATA_TX_CSR | b;
}

void uart_discard_input(void) {
	/* drain the hardware FIFO */
	while ((UART_DATA & UART_DATA_RX_CSR) != 0) {
	}
	/* discard buffer contents */
	input_size = 0;
}

#define RST_RESET (*((volatile uint32_t*)0xb806001c))

#define RST_RESET_FULL_CHIP_RESET (1 << 24)

void reset(void) {
	/* drain the uart TX fifo to prevent loss of data */
	while (UART_CS & UART_CS_TX_BUSY);

	RST_RESET = RST_RESET_FULL_CHIP_RESET;
	for (;;);
}
