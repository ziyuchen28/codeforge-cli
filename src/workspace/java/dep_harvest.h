
#pragma once

#include <cstddef>
#include <string>
#include <vector>


// Extract method callee names inside a method/constructor node byte range.
std::vector<std::string> harvest_callees_in_range(const std::string &abs_path,
                                                  size_t node_start,
                                                  size_t node_end);

