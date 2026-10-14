/* Leave one page for the SCE records appended after the linked RX bytes. */
SECTIONS
{
	. = ALIGN(0x1000) + 0x1000;
}
INSERT BEFORE .data;
