SECTIONS
{
	/* Start of program at beginning of flash */
	.vectors :
	{
		. = ALIGN(4);
		_start_vectors = .;
		KEEP(*(.vectors))
		KEEP(*(.pendsv_vector))
		KEEP(*(.systick_vector))
		KEEP(*(.interrupt_vectors))
		_end_vectors = .;
		. = ALIGN(4);
	} >FLASH

	.text :
	{
		. = ALIGN(4);
		*(.text) *(.text.*)
		*(.glue_7t) *(.glue_7)
		. = ALIGN(4);
	} >FLASH

	.rodata :
	{
		. = ALIGN(4);
		*(.rodata) *(.rodata*)
		. = ALIGN(4);
	} >FLASH

	_sidata = LOADADDR(.data);

	.data :
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data) *(.data*)
		. = ALIGN(4);
		_edata = .;
	} >RAM AT> FLASH

	.bss :
	{
		. = ALIGN(4);
		_sbss = .;
		*(.bss) *(.bss.*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} >RAM

	.mainstack :
	{
		. = ALIGN(4);
		_stack_bottom = .;
		. += 4096;
		_stack_top = .;
		. += 4;
	} >RAM

	.heap :
	{
		. = ALIGN(4);
		_sheap = .;
		. = ORIGIN(RAM) + LENGTH(RAM);
		_eheap = .;
	} >RAM
}
