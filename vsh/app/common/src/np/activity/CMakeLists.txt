enable_language(CXX)
# SceNpActivityNet (vs0:sys/external/np_activity.suprx), built by Sony with SNC 3.57 in C++: the
# activity feed's network requests. Its files are Sony's where the library's checks name them
# (W:\Build\J01509657\vsh\app\common\src\np\activity\...).
set(MODULE_IMPORT_NAMES vsh/app/common/src/np/np_manager/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	np_activity.cpp
	abstract_context_manager.cpp
	context_manager.cpp
	common/activity_object.cpp
	common/activity_lock_manager.cpp
	common/activity_memory.cpp
	common/activity_mutex.cpp
	common/activity_simple_lock.cpp
	component/abstract_component.cpp
	component/auth/component_auth.cpp
	component/context/component_context.cpp
	component/http/component_http.cpp
	component/command_executor/component_command_executor.cpp
	component/utility/component_utility.cpp
	component/xml_generator/component_xml_generator.cpp
	component/xml_parser/component_xml_parser.cpp
	../np_common/xml.c
	component/xml_parser/observer/abstract_xml_parser_subject.cpp
	component/xml_parser/handler/xml_parser_account_information_handler.cpp
	component/xml_parser/handler/xml_parser_account_status_list_handler.cpp
	component/xml_parser/handler/xml_parser_add_status_list_handler.cpp
	component/xml_parser/handler/xml_parser_check_status_handler.cpp
	component/xml_parser/handler/xml_parser_result_handler.cpp
	component/xml_parser/handler/xml_parser_static_resource_handler.cpp
	component/xml_parser/handler/xml_parser_status_detail_handler.cpp
	component/xml_parser/handler/xml_parser_status_id_handler.cpp
	component/xml_parser/handler/xml_parser_status_id_comment_handler.cpp
	component/xml_parser/handler/xml_parser_status_info_handler.cpp
	component/xml_parser/handler/xml_parser_status_timeline_handler.cpp
	common/activity_xml_writer.cpp
	common/activity_hmac.cpp
	command/command_add_comment.cpp
	command/command_add_status_list.cpp
	command/command_add_vote_good_job.cpp
	command/command_check_status_update.cpp
	command/command_get_account_information.cpp
	command/command_get_friend_status_timeline.cpp
	command/command_get_friend_status_timeline_by_title.cpp
	command/command_get_static_resource.cpp
	command/command_get_status_detail.cpp
	command/command_get_status_info.cpp
	command/command_get_status_info_with_callback.cpp
	command/command_get_status_timeline_by_account.cpp
	command/command_get_status_timeline_by_account_and_title.cpp
	command/command_remove_comment.cpp
	command/command_remove_status.cpp
	command/command_remove_vote_good_job.cpp
	command/command_set_account_information.cpp
	common/activity_debug.cpp
	component/xml_parser/handler/abstract_xml_parser_handler.cpp
	command/abstract_command.cpp
	../../../../../../sys/sdk/target/src/sha1/libsha1.c
	../../../../../../sys/sdk/target/src/libsnc/weak_calls.c
	../../../../../../sys/internal/kernel/src/sysmem/sha1_transform.S
)
# the XML parser SceNpCommon exports, built here from its source with a 1025-byte value buffer
set_source_files_properties(../np_common/xml.c PROPERTIES COMPILE_DEFINITIONS NP_XML_PARSER_ONLY)
target_compile_definitions(${PSP2_TARGET} PRIVATE NP_XML_VALUE_SIZE=1025)
# the SDK's libSceSha1, which Sony built with GCC and without stack guards
set_source_files_properties(../../../../../../sys/sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(../../../../../../sys/internal/kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
