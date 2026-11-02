/*
 * The GPU programs (CMakeLists.txt assembles them): the microkernels, the clear clip
 * workaround and its VDM stream in programs.S, and the PDS programs sgxinit.c keeps as words,
 * from pds/. Original table names unknown.
 */
extern const uint32_t program0[16030];
extern const uint32_t program1[2826];
static const uint32_t program2[] = {
#include "program2.inc"
};
static const uint32_t program3[] = {
#include "program3.inc"
};
static const uint32_t program4[] = {
#include "program4.inc"
};
static const uint32_t program5[] = {
#include "program5.inc"
};
static const uint32_t program6[] = {
#include "program6.inc"
};
static const uint32_t program7[] = {
#include "program7.inc"
};
extern const uint32_t program8[10];
extern const uint32_t program9[60];
extern const uint32_t program10[80];
