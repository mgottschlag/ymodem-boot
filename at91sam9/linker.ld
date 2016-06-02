
ENTRY(start_asm)
SECTIONS
{
	. = 0x21100000;

	.setup : {
		*(.setup)
	}

	.text : {
		*(.text)
	}

	.rodata : {
		*(.rodata*)
	}
	.data : {
		*(.data*)
	}
	.note : {
		*(.note*)
	}
	.init_array : {
		init_array_start = .;
		*(.init_array*)
		init_array_end = .;
	}
	.bss : {
		bss_start = .;
		*(.bss*)
		bss_end = .;
	}
}


