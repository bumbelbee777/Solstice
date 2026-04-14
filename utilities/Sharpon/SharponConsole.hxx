#pragma once

#include <cstddef>
#include <string>
#include <vector>

void Sharpon_DrawConsolePanel(std::vector<std::string>& scrollback, char* inputBuf, size_t inputBufSize,
                                void (*onLine)(void* user, const char* line), void* userData, bool* pOpen);
