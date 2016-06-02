
#include "ymodem_boot.h"

/* start of 128B packet */
#define SOH 0x01
/* start of 1KiB packet */
#define STX 0x02
/* end of transfer */
#define EOT 0x04
#define ACK 0x06
#define BSP 0x08
#define NAK 0x15
#define CAN 0x18
#define EOF 0x1A
#define CRC 'C'

#define HEADER_SIZE 3
#define TRAILER_SIZE 2
#define PACKET_OVERHEAD (HEADER_SIZE + TRAILER_SIZE)
static const uint32_t BUFFER_SIZE = 1024 + PACKET_OVERHEAD;

static const uint32_t SEQNO = 1;
static const uint32_t SEQNO_COMPLEMENT = 2;

#define STATUS_OK 0
#define STATUS_ERROR 1
#define STATUS_ABORT 2
#define STATUS_TIMEOUT 3

static uint16_t crc16(const uint8_t *start, const uint8_t *end) {
	uint16_t crc = 0;
	const uint8_t *b;
	unsigned int i;
	for (b = start; b != end; b++) {
		crc = crc ^ (((uint16_t)*b) << 8);
		for (i = 0; i < 8; i++) {
			if ((crc & 0x8000) != 0) {
				crc = (crc << 1) ^ 0x1021;
			} else {
				crc = crc << 1;
			}
		}
	}
	return crc;
}

int receive_packet(uint32_t *packet_size,
                   uint8_t *buffer,
                   uint32_t max_can_count) {
	unsigned int i;
	uint8_t b;
	int success = uart_read_byte(&b);
	if (success == 0) {
		return STATUS_TIMEOUT;
	}
	switch (b) {
	case SOH:
		*packet_size = 128;
		break;
	case STX:
		*packet_size = 1024;
		break;
	case EOT:
		*packet_size = 0;
		return STATUS_OK;
	case CAN:
		if (max_can_count > 0) {
			return receive_packet(packet_size, buffer, max_can_count - 1);
		} else {
			return STATUS_ABORT;
		}
	default:
		return STATUS_ABORT;
	}
	buffer[0] = b;

	for (i = 1; i < *packet_size + PACKET_OVERHEAD; i++) {
		success = uart_read_byte(&b);
		if (!success) {
			return STATUS_TIMEOUT;
		}
		buffer[i] = b;
	}
	/* if you are wondering why the following like prints a warning, read
	 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=38341 */
	if (buffer[SEQNO] != (buffer[SEQNO_COMPLEMENT] ^ 0xffu)) {
		return STATUS_ERROR;
	}
	if (crc16(buffer + 3, buffer + *packet_size + PACKET_OVERHEAD) != 0) {
		return STATUS_ERROR;
	}
	return STATUS_OK;
}

static void abort_file(void) {
	uart_write_byte(CAN);
	uart_write_byte(CAN);
	uart_write_byte(CAN);
	uart_write_byte(CAN);
	uart_write_byte(BSP);
	uart_write_byte(BSP);
	uart_write_byte(BSP);
	uart_write_byte(BSP);
	uart_discard_input();
}

int ymodem_receive(uint8_t *dest, uint32_t *size) {
	unsigned int i;
	int status;
	/* read the first header */
	uint8_t buffer[BUFFER_SIZE];
	uint32_t total_length = 0;
	uint32_t size_start = 0;
	uint32_t packet_length;
	for (;;) {
		uart_discard_input();
		uart_write_byte(CRC);
		status = receive_packet(&packet_length, buffer, 3);
		if (status == STATUS_OK) {
			if (buffer[SEQNO] != 0) {
				uart_write_byte(NAK);
				continue;
			}
			uart_write_byte(ACK);
			uart_write_byte(CRC);
			size_start = 3;
			/* skip the filename */
			while (buffer[size_start] != 0) {
				size_start++;
			}
			size_start++;
			/* read the file size */
			while (buffer[size_start] != ' ') {
				total_length = total_length * 10 + (buffer[size_start] - '0');
				size_start = size_start + 1;
			}
			break;
		} else if (status == STATUS_ERROR) {
			abort_file();
			return 0;
		} else if (status == STATUS_ABORT) {
			uart_write_byte(ACK);
			return 0;
		} else if (status == STATUS_TIMEOUT) {
			continue;
		}
	}
	// Read the data.
	uint8_t expected_packet = 1;
	uint32_t bytes_received = 0;
	for (;;) {
		status = receive_packet(&packet_length, buffer, 0);
		if (status == STATUS_OK) {
			if (packet_length == 0) {
				uart_write_byte(ACK);
				uart_write_byte(CRC);
				/* check received packet length */
				if (total_length > bytes_received) {
					abort_file();
					print_str("error: Received ");
					print_hex(bytes_received);
					print_str(" bytes (expected: ");
					print_hex(total_length);
					print_str(" bytes)\n");;
					return 0;
				}
				break;
			}
			if (buffer[SEQNO] != expected_packet) {
				uart_write_byte(NAK);
				uart_discard_input();
				continue;
			}
			if (bytes_received >= total_length) {
				abort_file();
				print_str("error: Received too much data, expected ");
				print_hex(total_length);
				print_str(" bytes\n");
				return 0;
			}
			expected_packet = expected_packet + 1;
			uart_write_byte(ACK);
			for (i = 0; i < packet_length; i++) {
				dest[bytes_received] = buffer[i + HEADER_SIZE];
				bytes_received = bytes_received + 1;
			}
		}else if (status == STATUS_ERROR) {
			abort_file();
			return 0;
		}else if (status == STATUS_ABORT) {
			return 0;
		}else if (status == STATUS_TIMEOUT) {
			abort_file();
			return 0;
		}
	}
	/* read the last header (should be empty, signals that there are no more
	 * files following) */
	status = receive_packet(&packet_length, buffer, 0);
	if (status == STATUS_OK) {
		if (packet_length == 0) {
			print_str("error: expected final packet, received EOF?");
			return 0;
		}
		uart_write_byte(ACK);
		/* TODO: check final CRC checksum? */
	}else if (status == STATUS_ERROR) {
		abort_file();
		return 0;
	}else if (status == STATUS_ABORT) {
		return 0;
	}else if (status == STATUS_TIMEOUT) {
		abort_file();
		return 0;
	}
	*size = total_length;
	return 1;
}
