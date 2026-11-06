enable_language(CXX)
# SceDbutil (vs0:sys/external/dbutil.suprx), built by Sony with SNC 3.57 in C++ (paf's
# allocator, string and vector headers in its records): the system's database utility and its
# collators. Its paf imports take libpaf's link names.
# libSceCes's Shift_JIS conversions (the Japanese collator's) and parts of the SNC runtime are
# linked in from their libraries' folders; its SQLite imports take the vendored SQLite's names, its IPMI imports the C++ names SceShellSvc
# exports them under
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/app/common/src/sqlite/exports.yml
	vsh/lowlayer/src/shell_svc/exports.yml sys/internal/usermode/src/driver_us/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
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
	connection.cpp
	dcn_client.cpp
	dcn_server.cpp
	dcn_server_stub.cpp
	function.cpp
	record_result.cpp
	record.cpp
	value.cpp
	row_cache.cpp
	result.cpp
	rowid_result.cpp
	statement.cpp
	collation.cpp
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/JisProfile_SJis1997_CP932.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/SJis_FromUCS.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/SJis_ToUCS.c
	../../../../../sys/internal/usermode/src/ces/libces/src/MBCS/Mbcs_FromUTF32.c
	../../../../../sys/internal/usermode/src/ces/libces/src/SBCS/Sbc_UCS.c
	../../../../../sys/internal/usermode/src/ces/libces/src/SBCS/ISO_646/ASCII.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis1990_Ext_FromUcs.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis1990_Ext_row85_94.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_row01_08.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_row13.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_row16_84.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_UcsCJKExB.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/Jis_FromUCS.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/JIS_X_0201_Katakana.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/JisMenKuTen.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/ShiftJIS.c
	../../../../../sys/internal/usermode/src/ces/libces/src/MBCS/Mbcs_FromUCS4.c
	../../../../../sys/internal/usermode/src/ces/libces/src/SBCS/ces_sbcs_profile.c
	../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_FromUcs.c
	../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/lowlayer/src")
# Sony's connection code calls paf::string's assign out of line and inlines AddListener and the
# table map's find: the budget that gives both
set_source_files_properties(connection.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(dcn_client.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(dcn_server.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(dcn_server_stub.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(function.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(record_result.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(record.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(value.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(row_cache.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(result.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(rowid_result.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(statement.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(collation.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
set_source_files_properties(../../../../../sys/internal/usermode/src/ces/libces/src/JIS/jis2004_row16_84.c PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=400)
