/*
 * paf::common::_error_callback: three words of the file's data, set by the static
 * initializer and read by nothing in this module (their meaning is open; the name is
 * Sony's export).
 */
#include <stdint.h>

namespace paf {
namespace common {

uint32_t _error_callback[3];

}
}
