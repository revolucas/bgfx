/*
 * Copyright 2011-2025 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#ifndef BGFX_SHADERC_C_API_H_HEADER_GUARD
#define BGFX_SHADERC_C_API_H_HEADER_GUARD

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#   if defined(BGFX_SHADERC_SHARED_LIBRARY)
#       define BGFX_SHADERC_API __declspec(dllexport)
#   elif defined(BGFX_SHADERC_SHARED_LIBRARY_IMPORT)
#       define BGFX_SHADERC_API __declspec(dllimport)
#   else
#       define BGFX_SHADERC_API
#   endif
#else
#   define BGFX_SHADERC_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
        BGFX_SHADERC_RESULT_INVALID_ARGUMENT = -1,
        BGFX_SHADERC_RESULT_ERROR            = 0,
        BGFX_SHADERC_RESULT_SUCCESS          = 1,
};

typedef struct bgfx_shaderc_buffer
{
        uint8_t* data;
        uint32_t size;
} bgfx_shaderc_buffer;

typedef struct bgfx_shaderc_string_list
{
        const char* const* data;
        uint32_t count;
} bgfx_shaderc_string_list;

typedef struct bgfx_shaderc_compile_options
{
        char shader_type;
        const char* platform;
        const char* profile;
        const char* input_path;
        const char* output_path;

        bgfx_shaderc_string_list include_dirs;
        bgfx_shaderc_string_list defines;
        bgfx_shaderc_string_list dependencies;

        bool disasm;
        bool raw;
        bool preprocess_only;
        bool depends;

        bool debug_information;

        bool avoid_flow_control;
        bool no_preshader;
        bool partial_precision;
        bool prefer_flow_control;
        bool backwards_compatibility;
        bool warnings_are_errors;
        bool keep_intermediate;

        bool optimize;
        uint32_t optimization_level;
} bgfx_shaderc_compile_options;

BGFX_SHADERC_API int bgfx_shaderc_compile(
          const bgfx_shaderc_compile_options* _options
        , const char* _shader_source
        , uint32_t _shader_size
        , const char* _varying_source
        , uint32_t _varying_size
        , bgfx_shaderc_buffer* _output
        , bgfx_shaderc_buffer* _messages
        );

BGFX_SHADERC_API void bgfx_shaderc_free_buffer(bgfx_shaderc_buffer* _buffer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // BGFX_SHADERC_C_API_H_HEADER_GUARD
