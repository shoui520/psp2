/* core 0's program 0: parser0, from 0x805000 */
MEMORY
{
	image (rwx) : ORIGIN = 0x805000, LENGTH = 0x8000
}

ENTRY(parser0Entry)
INCLUDE fw2/parser/parser.ld

__parser0_code_load = __program_code_load;
__parser0_code_size = __program_code_size;
__parser0_local_load = __program_local_load;
__parser0_local_size = __program_local_size;
__parser0_globals_load = __program_globals_load;
__parser0_globals_size = __program_globals_size;
__parser0_zero_size = __program_zero_size;
