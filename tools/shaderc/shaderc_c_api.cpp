/*
 * Copyright 2011-2025 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "shaderc_c_api.h"

#include "shaderc.h"

#include <bx/bx.h>
#include <bx/error.h>
#include <bx/file.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bgfx
{
        bool compileShader(const char* _varying, const char* _comment, char* _shader, uint32_t _shaderLen, const Options& _options, bx::WriterI* _shaderWriter, bx::WriterI* _messageWriter);
}

namespace
{
        class VectorWriter : public bx::WriterI
        {
        public:
                explicit VectorWriter(std::vector<uint8_t>& _buffer)
                        : m_buffer(_buffer)
                {
                }

                virtual int32_t write(const void* _data, int32_t _size, bx::Error*) override
                {
                        if (0 == _size)
                        {
                                return 0;
                        }

                        const uint8_t* begin = static_cast<const uint8_t*>(_data);
                        m_buffer.insert(m_buffer.end(), begin, begin + _size);
                        return _size;
                }

        private:
                std::vector<uint8_t>& m_buffer;
        };

        class StringWriter : public bx::WriterI
        {
        public:
                explicit StringWriter(std::string& _string)
                        : m_string(_string)
                {
                }

                virtual int32_t write(const void* _data, int32_t _size, bx::Error*) override
                {
                        if (0 == _size)
                        {
                                return 0;
                        }

                        const char* begin = static_cast<const char*>(_data);
                        m_string.append(begin, _size);
                        return _size;
                }

        private:
                std::string& m_string;
        };

        void applyStringList(const bgfx_shaderc_string_list& _input, std::vector<std::string>& _output)
        {
                _output.clear();

                if (NULL == _input.data
                ||  0 == _input.count)
                {
                        return;
                }

                _output.reserve(_input.count);
                for (uint32_t ii = 0; ii < _input.count; ++ii)
                {
                        const char* value = _input.data[ii];
                        if (NULL != value)
                        {
                                _output.push_back(value);
                        }
                }
        }

        void populateOptions(const bgfx_shaderc_compile_options& _input, bgfx::Options& _output)
        {
                _output = bgfx::Options();

                _output.shaderType = _input.shader_type;

                if (NULL != _input.platform)
                {
                        _output.platform = _input.platform;
                }

                if (NULL != _input.profile)
                {
                        _output.profile = _input.profile;
                }

                _output.inputFilePath  = (NULL != _input.input_path)  ? _input.input_path  : "<memory>";
                _output.outputFilePath = (NULL != _input.output_path) ? _input.output_path : "";

                applyStringList(_input.include_dirs, _output.includeDirs);
                applyStringList(_input.defines,      _output.defines);
                applyStringList(_input.dependencies, _output.dependencies);

                _output.disasm                = _input.disasm;
                _output.raw                   = _input.raw;
                _output.preprocessOnly        = _input.preprocess_only;
                _output.depends               = _input.depends;
                _output.debugInformation      = _input.debug_information;
                _output.avoidFlowControl      = _input.avoid_flow_control;
                _output.noPreshader           = _input.no_preshader;
                _output.partialPrecision      = _input.partial_precision;
                _output.preferFlowControl     = _input.prefer_flow_control;
                _output.backwardsCompatibility = _input.backwards_compatibility;
                _output.warningsAreErrors     = _input.warnings_are_errors;
                _output.keepIntermediate      = _input.keep_intermediate;
                _output.optimize              = _input.optimize;

                uint32_t level = _input.optimization_level;
                level = std::min(level, uint32_t(3));
                _output.optimizationLevel = level;
        }

        void assignBuffer(bgfx_shaderc_buffer* _buffer, const void* _data, size_t _size, bool _nullTerminate)
        {
                if (NULL == _buffer)
                {
                        return;
                }

                if (0 == _size
                ||  NULL == _data)
                {
                        if (_nullTerminate)
                        {
                                uint8_t* memory = static_cast<uint8_t*>(std::malloc(1));
                                if (NULL == memory)
                                {
                                        _buffer->data = NULL;
                                        _buffer->size = 0;
                                        return;
                                }

                                memory[0] = '\0';
                                _buffer->data = memory;
                                _buffer->size = 0;
                        }
                        else
                        {
                                _buffer->data = NULL;
                                _buffer->size = 0;
                        }

                        return;
                }

                const size_t total = _size + (_nullTerminate ? 1 : 0);

                uint8_t* memory = static_cast<uint8_t*>(std::malloc(total));
                if (NULL == memory)
                {
                        _buffer->data = NULL;
                        _buffer->size = 0;
                        return;
                }

                std::memcpy(memory, _data, _size);
                if (_nullTerminate)
                {
                        memory[_size] = '\0';
                }
                _buffer->data = memory;
                _buffer->size = static_cast<uint32_t>(_size);
        }
}

int bgfx_shaderc_compile(
          const bgfx_shaderc_compile_options* _options
        , const char* _shader_source
        , uint32_t _shader_size
        , const char* _varying_source
        , uint32_t _varying_size
        , bgfx_shaderc_buffer* _output
        , bgfx_shaderc_buffer* _messages
        )
{
        if (NULL != _output)
        {
                _output->data = NULL;
                _output->size = 0;
        }

        if (NULL != _messages)
        {
                _messages->data = NULL;
                _messages->size = 0;
        }

        if (NULL == _options
        ||  NULL == _shader_source)
        {
                if (NULL != _messages)
                {
                        static const char kError[] = "Invalid arguments passed to bgfx_shaderc_compile.\n";
                        assignBuffer(_messages, kError, BX_COUNTOF(kError) - 1, true);
                }

                return BGFX_SHADERC_RESULT_INVALID_ARGUMENT;
        }

        uint32_t shaderSize = _shader_size;
        if (0 == shaderSize)
        {
                shaderSize = static_cast<uint32_t>(std::strlen(_shader_source));
        }

        if (0 == shaderSize)
        {
                if (NULL != _messages)
                {
                        static const char kEmpty[] = "Shader source is empty.\n";
                        assignBuffer(_messages, kEmpty, BX_COUNTOF(kEmpty) - 1, true);
                }

                return BGFX_SHADERC_RESULT_INVALID_ARGUMENT;
        }

        char* shaderData = new char[shaderSize + 2];
        std::memcpy(shaderData, _shader_source, shaderSize);
        shaderData[shaderSize]     = '\n';
        shaderData[shaderSize + 1] = '\0';

        std::string varying;
        const char* varyingPtr = NULL;
        if (NULL != _varying_source)
        {
                uint32_t varyingSize = _varying_size;
                if (0 == varyingSize)
                {
                        varyingSize = static_cast<uint32_t>(std::strlen(_varying_source));
                }

                varying.assign(_varying_source, varyingSize);
                varying.push_back('\0');
                varyingPtr = varying.c_str();
        }

        bgfx::Options options;
        populateOptions(*_options, options);

        std::vector<uint8_t> outputData;
        std::string messageData;

        VectorWriter shaderWriter(outputData);
        StringWriter messageWriter(messageData);

        bool compiled = bgfx::compileShader(
                  varyingPtr
                , ""
                , shaderData
                , shaderSize
                , options
                , &shaderWriter
                , &messageWriter);

        if (NULL != _output)
        {
                assignBuffer(_output,
                             outputData.empty() ? NULL : outputData.data(),
                             outputData.size(),
                             false);
        }

        if (NULL != _messages)
        {
                const size_t messageSize = messageData.size();
                if (messageSize > 0)
                {
                        assignBuffer(_messages, messageData.data(), messageSize, true);
                }
                else
                {
                        assignBuffer(_messages, "", 0, true);
                }
        }

        return compiled ? BGFX_SHADERC_RESULT_SUCCESS : BGFX_SHADERC_RESULT_ERROR;
}

void bgfx_shaderc_free_buffer(bgfx_shaderc_buffer* _buffer)
{
        if (NULL == _buffer
        ||  NULL == _buffer->data)
        {
                return;
        }

        std::free(_buffer->data);
        _buffer->data = NULL;
        _buffer->size = 0;
}
