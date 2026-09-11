#pragma once
#include <boost/sml.hpp>
#include <string>
#include <vector>

namespace app {
namespace sml = boost::sml;
using string_type = std::string;
template <typename T> using vector_type = std::vector<T>;

} // namespace app
