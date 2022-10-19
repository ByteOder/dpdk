SECTIONS
{
	. = ALIGN(16);
	__module_start__ = .;
	.module_section = { *(.module_section) }
	__module_end__ = .;
}