
#include "ymodem_boot.h"

void memmove(uint8_t *dest, const uint8_t *src, uint32_t size) {
	if (dest < src) {
		while (size > 0) {
			*dest = *src;
			dest++;
			src++;
			size--;
		}
	} else {
		dest += size;
		src += size;
		while (size > 0) {
			dest--;
			src--;
			*dest = *src;
			size--;
		}
	}
}

void print_hex(uint32_t x) {
	unsigned int i;
	const char *digits = "0123456789abcdef";
	uart_write_byte('0');
	uart_write_byte('x');
	for (i = 0; i < 8; i++) {
		uart_write_byte(digits[x >> 28]);
		x <<= 4;
	}
}

void print_str(const char *s) {
	while (*s) {
		if (*s == '\n') {
			uart_write_byte('\r');
		}
		uart_write_byte(*s);
		s++;
	}
}

#define from_be __builtin_bswap32

struct image_header {
	uint32_t magic;
	uint32_t header_crc;
	uint32_t time;
	uint32_t size;
	uint32_t load_address;
	uint32_t entry_point;
	uint32_t data_crc;
	uint8_t os;
	uint8_t arch;
	uint8_t image_type;
	uint8_t compression_type;
	uint8_t name[32];
} __attribute__((packed));

static const uint32_t UIMAGE_MAGIC = 0x27051956;

void main(void) __attribute__((noreturn));
void main(void) {
	uint32_t size;
	uint8_t *download_addr = (uint8_t*)ADDRESS + 0x100000;

	hw_init();

	print_str(" ymodem bootloader\n");
	print_str("===================\n");
	print_str("downloading uImage to ");
	print_hex((uint32_t)download_addr);
	print_str("...\n");
	for (;;) {
		int success = ymodem_receive(download_addr, &size);
		if (success) {
			break;
		}
		print_str("download failed, retrying...\n");
	}
	print_str("download complete, ");
	print_hex(size);
	print_str(" bytes received.\n");
	const struct image_header *header = (struct image_header*)download_addr;
	if (from_be(header->magic) != UIMAGE_MAGIC) {
		print_str("error: invalid uimage magic ");
		print_hex(from_be(header->magic));
		print_str(" (expected: ");
		print_hex(UIMAGE_MAGIC);
		print_str("\n");
		reset();
	}
	if (size < sizeof(struct image_header) + from_be(header->size)) {
		print_str("error: invalid uimage size ");
		print_hex(from_be(header->size));
		print_str(" (received only ");
		print_hex(size);
		print_str(" bytes)\n");
		reset();
	}
	uint8_t *dest = (uint8_t*)from_be(header->load_address);
	memmove(dest, download_addr + 0x40, from_be(header->size));
	print_str("relocated file from ");
	print_hex((uint32_t)download_addr + 0x40);
	print_str(" to ");
	print_hex((uint32_t)dest);
	print_str("\njumping to ");
	print_hex(from_be(header->entry_point));
	print_str("...\n");
	void (*entry)(void) = (void(*)(void))from_be(header->entry_point);
	entry();
	reset();
}

