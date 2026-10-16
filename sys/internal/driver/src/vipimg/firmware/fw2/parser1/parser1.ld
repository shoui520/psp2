/* core 0's program 1: parser1, from 0x80d000 */
MEMORY
{
	image (rwx) : ORIGIN = 0x80d000, LENGTH = 0x9000
}

ENTRY(parser1Entry)
INCLUDE fw2/parser/parser.ld

__parser1_code_load = __program_code_load;
__parser1_code_size = __program_code_size;
__parser1_local_load = __program_local_load;
__parser1_local_size = __program_local_size;
__parser1_globals_load = __program_globals_load;
__parser1_globals_size = __program_globals_size;
__parser1_zero_size = __program_zero_size;
