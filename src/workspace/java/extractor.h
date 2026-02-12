
#pragma once

#include <cstddef>
#include <string>


struct Method
{
    bool found = false;

    std::string abs_path;
    std::string rel_path;

    // Byte offsets 
    size_t start = 0;
    size_t end = 0;

    std::string reason;
    std::string text;
};


Method extract_method_from_file(const std::string &abs_path,
                                const std::string &rel_path,
                                const std::string &method_name);

