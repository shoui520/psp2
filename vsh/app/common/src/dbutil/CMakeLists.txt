enable_language(CXX)
# SceDbutil (vs0:sys/external/dbutil.suprx), built by Sony with SNC 3.57 in C++ (paf's
# allocator, string and vector headers in its records): the system's database utility and its
# collators. Its paf imports take libpaf's link names.
# its SQLite imports take the vendored SQLite's names, its IPMI imports the C++ names SceShellSvc
# exports them under
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/sqlite/exports.yml
	vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	module.c
	collator.cpp
	collator_danish.cpp
	collator_dutch.cpp
	collator_english.cpp
	collator_finnish.cpp
	collator_french.cpp
	collator_german.cpp
	collator_halfwidth.cpp
	collator_italian.cpp
	collator_japanese.cpp
	collator_katakana.cpp
	collator_korean.cpp
	collator_latin.cpp
	collator_norwegian.cpp
	collator_polish.cpp
	collator_portuguese.cpp
	collator_russian.cpp
	collator_simplified_chinese.cpp
	collator_spanish.cpp
	collator_swedish.cpp
	collator_traditional_chinese.cpp
	collator_turkish.cpp
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3")
