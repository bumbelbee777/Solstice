#include "SolsticeAPI/V1/Scripting.h"
#include <algorithm>
#include "Scripting/Compiler/Compiler.hxx"
#include "Scripting/VM/BytecodeVM.hxx"
#include <cstring>
#include <sstream>
#include <string>
#include <optional>

extern "C" {
namespace {
SolsticeV1_ScriptingPrintHook g_PrintHook = nullptr;
void* g_PrintHookUserData = nullptr;

void WriteError(const std::string& message, char* errorBuffer, size_t errorBufferSize) {
    if (!errorBuffer || errorBufferSize == 0) {
        return;
    }
    const size_t len = std::min(errorBufferSize - 1, message.size());
#ifdef _WIN32
    strncpy_s(errorBuffer, errorBufferSize, message.c_str(), len);
#else
    strncpy(errorBuffer, message.c_str(), len);
#endif
    errorBuffer[len] = '\0';
}

SolsticeV1_ResultCode ExecuteProgram(
    const Solstice::Scripting::Program& program,
    std::optional<std::string> exportName,
    char* outputBuffer,
    size_t outputBufferSize,
    char* errorBuffer,
    size_t errorBufferSize) {
    try {
        Solstice::Scripting::BytecodeVM vm;
        std::ostringstream outputStream;
        vm.RegisterNative("print", [&outputStream](const std::vector<Solstice::Scripting::Value>& args) -> Solstice::Scripting::Value {
            std::ostringstream lineStream;
            bool first = true;
            for (const auto& arg : args) {
                if (!first) {
                    outputStream << " ";
                    lineStream << " ";
                }
                first = false;

                if (std::holds_alternative<int64_t>(arg)) {
                    outputStream << std::get<int64_t>(arg);
                    lineStream << std::get<int64_t>(arg);
                } else if (std::holds_alternative<double>(arg)) {
                    outputStream << std::get<double>(arg);
                    lineStream << std::get<double>(arg);
                } else if (std::holds_alternative<std::string>(arg)) {
                    outputStream << std::get<std::string>(arg);
                    lineStream << std::get<std::string>(arg);
                } else {
                    outputStream << "[value]";
                    lineStream << "[value]";
                }
            }
            outputStream << "\n";
            if (g_PrintHook) {
                const std::string line = lineStream.str();
                g_PrintHook(line.c_str(), g_PrintHookUserData);
            }
            return (int64_t)0;
        });

        vm.LoadProgram(program);
        if (exportName.has_value()) {
            const auto expIt = program.Exports.find(*exportName);
            if (expIt == program.Exports.end()) {
                WriteError("Export not found", errorBuffer, errorBufferSize);
                return SolsticeV1_ResultFailure;
            }
            (void)vm.RunFunctionSlice(expIt->second, {});
        } else {
            vm.Run();
        }

        const std::string output = outputStream.str();
        if (outputBuffer && outputBufferSize > 0) {
            size_t len = std::min(outputBufferSize - 1, output.length());
#ifdef _WIN32
            strncpy_s(outputBuffer, outputBufferSize, output.c_str(), len);
#else
            strncpy(outputBuffer, output.c_str(), len);
#endif
            outputBuffer[len] = '\0';
        }
        return SolsticeV1_ResultSuccess;
    } catch (const std::exception& e) {
        WriteError(e.what(), errorBuffer, errorBufferSize);
        return SolsticeV1_ResultFailure;
    } catch (...) {
        WriteError("Unknown execution error", errorBuffer, errorBufferSize);
        return SolsticeV1_ResultFailure;
    }
}
} // namespace

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
        return ExecuteProgram(program, std::nullopt, OutputBuffer, OutputBufferSize, ErrorBuffer, ErrorBufferSize);
    } catch (const std::exception& e) {
        WriteError(e.what(), ErrorBuffer, ErrorBufferSize);
        return SolsticeV1_ResultFailure;
    } catch (...) {
        WriteError("Unknown execution error", ErrorBuffer, ErrorBufferSize);
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ScriptingExecuteExport(
    const char* Source,
    const char* ExportName,
    char* OutputBuffer,
    size_t OutputBufferSize,
    char* ErrorBuffer,
    size_t ErrorBufferSize) {
    if (!Source || !ExportName || ExportName[0] == '\0') {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Scripting::Compiler compiler;
        auto program = compiler.Compile(std::string(Source));
        return ExecuteProgram(program, std::string(ExportName), OutputBuffer, OutputBufferSize, ErrorBuffer, ErrorBufferSize);
    } catch (const std::exception& e) {
        WriteError(e.what(), ErrorBuffer, ErrorBufferSize);
        return SolsticeV1_ResultFailure;
    } catch (...) {
        WriteError("Unknown execution error", ErrorBuffer, ErrorBufferSize);
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API void SolsticeV1_ScriptingSetPrintHook(
    SolsticeV1_ScriptingPrintHook Hook,
    void* UserData) {
    g_PrintHook = Hook;
    g_PrintHookUserData = UserData;
}

} // extern "C"
