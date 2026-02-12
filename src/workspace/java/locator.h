#pragma once
#include <memory>
#include <string>
#include <vector>
#include "workspace/scanner.h"


struct ClassLocation
{
    bool found = false;
    std::string abs_path;
    std::string rel_path;
    std::string reason;
};


class JavaLocator
{
public:
    virtual ~JavaLocator() = default;
    virtual ClassLocation locate_class(const std::string &fqcn) = 0;
};


std::unique_ptr<JavaLocator> make_text_java_locator(const std::string &repo_root,
                                                    const std::vector<FileEntry> &files);


std::unique_ptr<JavaLocator> make_text_java_locator(const std::vector<FileEntry> &files);

