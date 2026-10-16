cmake_minimum_required(VERSION 3.20)
project(SkillIssue VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
option(SKILLISSUE_SANITIZERS "Enable compiler/test sanitizers" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SKILLISSUE_MSVC_FRONTEND OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(SKILLISSUE_MSVC_FRONTEND ON)
endif()

add_library(skillissue
    src/compiler.cpp src/lexer.cpp src/parser.cpp src/semantic.cpp
    src/constant_fold.cpp src/codegen.cpp)
target_include_directories(skillissue PUBLIC include)
add_executable(skillc src/main.cpp)
target_link_libraries(skillc PRIVATE skillissue)
add_library(skill_runtime STATIC runtime/skill_runtime.cpp)

function(skillissue_warnings target)
    if(SKILLISSUE_MSVC_FRONTEND)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
skillissue_warnings(skillissue)
skillissue_warnings(skillc)
skillissue_warnings(skill_runtime)

if(SKILLISSUE_SANITIZERS)
    if(SKILLISSUE_MSVC_FRONTEND)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(FATAL_ERROR "Use RelWithDebInfo for MSVC AddressSanitizer (no /RTC)")
        endif()
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
        target_compile_options(skillissue PRIVATE /fsanitize=address)
        target_compile_options(skillc PRIVATE /fsanitize=address)
        target_link_options(skillc PRIVATE /INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(skillissue PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_compile_options(skillc PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(skillc PRIVATE -fsanitize=address,undefined)
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "Sanitizers require MSVC, Clang, or GCC")
    endif()
endif()

# The LLVM library SDK is unnecessary: only the external clang++ driver is used.
find_program(SKILLISSUE_CLANG NAMES clang++ clang++.exe
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/.tools/llvm/bin")
find_program(SKILLISSUE_LLVM_AS NAMES llvm-as llvm-as.exe
    HINTS "${CMAKE_CURRENT_SOURCE_DIR}/.tools/llvm/bin")
if(SKILLISSUE_CLANG)
    message(STATUS "Native SkillIssue backend: ${SKILLISSUE_CLANG}")
    set(NATIVE_RUNTIME "${CMAKE_CURRENT_BINARY_DIR}/skill_runtime_native.obj")
    add_custom_command(OUTPUT "${NATIVE_RUNTIME}"
        COMMAND "${SKILLISSUE_CLANG}" -std=c++20 -c
            "${CMAKE_CURRENT_SOURCE_DIR}/runtime/skill_runtime.cpp" -o "${NATIVE_RUNTIME}"
        DEPENDS runtime/skill_runtime.cpp VERBATIM)
    add_custom_target(native_runtime ALL DEPENDS "${NATIVE_RUNTIME}")
    set(DEMO_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/sum${CMAKE_EXECUTABLE_SUFFIX}")
    add_custom_target(demo
        COMMAND $<TARGET_FILE:skillc> "${CMAKE_CURRENT_SOURCE_DIR}/examples/sum.skill"
            -o "${CMAKE_CURRENT_BINARY_DIR}/sum.ll"
        COMMAND "${SKILLISSUE_CLANG}" -Wno-override-module "${CMAKE_CURRENT_BINARY_DIR}/sum.ll"
            "${NATIVE_RUNTIME}" -o "${DEMO_EXECUTABLE}"
        COMMAND "${DEMO_EXECUTABLE}"
        DEPENDS skillc native_runtime VERBATIM)
else()
    message(STATUS "clang++ not found: compiler/unit tests build; native tests and demo require SKILLISSUE_CLANG")
endif()

if(BUILD_TESTING)
    # A small dependency-free runner keeps setup offline and tests the public API.
    add_executable(skillissue_tests tests/compiler_tests.cpp)
    target_link_libraries(skillissue_tests PRIVATE skillissue)
    skillissue_warnings(skillissue_tests)
    add_test(NAME compiler_unit COMMAND skillissue_tests)
    add_test(NAME compiler_cli COMMAND "${CMAKE_COMMAND}"
        "-DSKILLC=$<TARGET_FILE:skillc>"
        "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli-tests"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.cmake")
    if(SKILLISSUE_CLANG)
        foreach(folding IN ITEMS on off)
            foreach(level IN ITEMS 0 2)
                add_test(NAME native_fold_${folding}_O${level} COMMAND "${CMAKE_COMMAND}"
                    "-DSKILLC=$<TARGET_FILE:skillc>" "-DCLANG=${SKILLISSUE_CLANG}"
                    "-DLLVM_AS=${SKILLISSUE_LLVM_AS}"
                    "-DRUNTIME=${NATIVE_RUNTIME}" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/native-tests/${folding}-O${level}"
                    "-DFOLDING=${folding}" "-DLEVEL=${level}"
                    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_tests.cmake")
                set_tests_properties(native_fold_${folding}_O${level} PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
    endif()
endif()
