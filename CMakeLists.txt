
cmake_minimum_required(VERSION 3.16)
project(codegencli LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


############################## dep #############################

add_library(ts_runtime STATIC
    external/tree-sitter/lib/src/lib.c
)

target_include_directories(ts_runtime PUBLIC
    external/tree-sitter/lib/include
    external/tree-sitter/lib/src
)

add_library(ts_java STATIC
    external/tree-sitter-java/src/parser.c
)

target_include_directories(ts_java PUBLIC
    external/tree-sitter/lib/include
    external/tree-sitter-java/src
)

target_link_libraries(ts_java PUBLIC ts_runtime)

############################## dep end #############################

# sys
add_library(sysproc STATIC
  src/sys/fd.cpp
  src/sys/io.cpp
  src/sys/process_posix.cpp
  src/sys/error.cpp
)
target_include_directories(sysproc PUBLIC src)

# app 
add_library(app STATIC
  src/app/codegen_runner.cpp
)
target_include_directories(app PUBLIC src)
target_link_libraries(app PUBLIC sysproc)

# workspace
add_library(workspace STATIC
  src/workspace/scanner.cpp
  src/workspace/java/locator_text.cpp
  src/workspace/java/extractor_treesitter.cpp
  src/workspace/java/snippet_from_hit_ts.cpp
  src/workspace/java/dep_harvest_ts.cpp
  src/workspace/search_rg.cpp
  src/workspace/prompt_spec.cpp
  src/workspace/context_builder.cpp
)
target_include_directories(workspace PUBLIC src)
target_link_libraries(workspace PUBLIC ts_java)

# cli
add_library(cli STATIC
  src/cli/commands.cpp
  src/cli/cmd_scan.cpp
  src/cli/cmd_locate.cpp
  src/cli/cmd_extract.cpp
  src/cli/cmd_search.cpp
  src/cli/cmd_snippets.cpp
  src/cli/cmd_prompt.cpp
  src/cli/cmd_context.cpp
  src/cli/cmd_ask.cpp
  src/cli/cmd_raw.cpp
)
target_include_directories(cli PUBLIC src)
target_link_libraries(cli PUBLIC workspace)

# main
add_executable(codegencli
  src/main.cpp
)
target_link_libraries(codegencli PRIVATE app cli)


if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(sysproc PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(codegencli PRIVATE -Wall -Wextra -Wpedantic)
endif()
