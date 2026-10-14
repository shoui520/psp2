/*
 * Room for the SCE tables vita-elf-create appends to the text segment: the
 * data segment starts 64 KiB further out than the default script puts it.
 */
SECTIONS
{
	. = . + 0x10000;
}
INSERT BEFORE .data;
