/*
 * The empty strings every paf::string and paf::wstring points at before it has text, and
 * after it is destroyed (their address is how the destructor knows to free nothing).
 */
#include <std/string>

namespace paf {

template<>
char basic_string<char, char_traits<char>, allocator<char> >::empty_string = 0;

template<>
wchar_t basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >::empty_string = 0;

}
