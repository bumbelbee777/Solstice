#include "SolsticeAPI/V1/Scripting.h"
#include <algorithm>
#include "Scripting/Compiler.hxx"
#include "Scripting/BytecodeVM.hxx"
#include <cstring>
#include <sstream>
#include <string>

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingCompile(
    const char* Source,
    char* ErrorBuffer,
    size_t ErrorBufferSize) {
    if (!Source) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Scripting::Compiler compiler;
        auto program = compiler.Compile(std::string(Source));
        (void)program;
        return SolsticeV1_ResultSuccess;
    } catch (const std::exception& e) {
        if (ErrorBuffer && ErrorBufferSize > 0) {
            size_t len = std::min(ErrorBufferSize - 1, strlen(e.what()));
#ifdef _WIN32
            strncpy_s(ErrorBuffer, ErrorBufferSize, e.what(), len);
#else
            strncpy(ErrorBuffer, e.what(), len);
#endif
            ErrorBuffer[len] = '\0';
        }
        return SolsticeV1_ResultFailure;
    } catch (...) {
        if (ErrorBuffer && ErrorBufferSize > 0) {
            const char* msg = "Unknown compilation error";
            size_t len = std::min(ErrorBufferSize - 1, strlen(msg));
#ifdef _WIN32
            strncpy_s(ErrorBuffer, ErrorBufferSize, msg, len);
#else
            strncpy(ErrorBuffer, msg, len);
#endif
            ErrorBuffer[len] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingExecute(
    const char* Source,
    char* OutputBuffer,
    size_t OutputBufferSize,
    char* ErrorBuffer,
    size_t ErrorBufferSize) {
    if (!Source) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Scripting::Compiler compiler;
        auto program = compiler.Compile(std::string(Source));

        Solstice::Scripting::BytecodeVM vm;

        std::ostringstream outputStream;
        vm.RegisterNative("print", [&outputStream](const std::vector<Solstice::Scripting::Value>& args) -> Solstice::Scripting::Value {
            bool first = true;
            for (const auto& arg : args) {
                if (!first) {
                    outputStream << " ";
                }
                first = false;

                if (std::holds_alternative<int64_t>(arg)) {
                    outputStream << std::get<int64_t>(arg);
                } else if (std::holds_alternative<double>(arg)) {
                    outputStream << std::get<double>(arg);
                } else if (std::holds_alternative<std::string>(arg)) {
                    outputStream << std::get<std::string>(arg);
                } else {
                    outputStream << "[value]";
                }
            }
            outputStream << "\n";
            return (int64_t)0;
        });

        vm.LoadProgram(program);
        vm.Run();

        std::string output = outputStream.str();
        if (OutputBuffer && OutputBufferSize > 0) {
            size_t len = std::min(OutputBufferSize - 1, output.length());
#ifdef _WIN32
            strncpy_s(OutputBuffer, OutputBufferSize, output.c_str(), len);
#else
            strncpy(OutputBuffer, output.c_str(), len);
#endif
            OutputBuffer[len] = '\0';
        }

        return SolsticeV1_ResultSuccess;
    } catch (const std::exception& e) {
        if (ErrorBuffer && ErrorBufferSize > 0) {
            size_t len = std::min(ErrorBufferSize - 1, strlen(e.what()));
#ifdef _WIN32
            strncpy_s(ErrorBuffer, ErrorBufferSize, e.what(), len);
#else
            strncpy(ErrorBuffer, e.what(), len);
#endif
            ErrorBuffer[len] = '\0';
        }
        return SolsticeV1_ResultFailure;
    } catch (...) {
        if (ErrorBuffer && ErrorBufferSize > 0) {
            const char* msg = "Unknown execution error";
            size_t len = std::min(ErrorBufferSize - 1, strlen(msg));
#ifdef _WIN32
            strncpy_s(ErrorBuffer, ErrorBufferSize, msg, len);
#else
            strncpy(ErrorBuffer, msg, len);
#endif
            ErrorBuffer[len] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
}

} // extern "C"
