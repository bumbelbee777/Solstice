#include "LibUI/Core/Core.hxx"
#include "LibUI/Widgets/Widgets.hxx"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Solstice engine DLL loading
#ifdef _WIN32
typedef bool (*InitializeFunc)();
typedef void (*ShutdownFunc)();
typedef int (*CompileFunc)(const char* source, char* errorBuffer, size_t errorBufferSize);
typedef int (*ExecuteFunc)(const char* source, char* outputBuffer, size_t outputBufferSize, char* errorBuffer, size_t errorBufferSize);
HMODULE g_SolsticeDLL = nullptr;
#else
typedef bool (*InitializeFunc)();
typedef void (*ShutdownFunc)();
typedef int (*CompileFunc)(const char* source, char* errorBuffer, size_t errorBufferSize);
typedef int (*ExecuteFunc)(const char* source, char* outputBuffer, size_t outputBufferSize, char* errorBuffer, size_t errorBufferSize);
void* g_SolsticeDLL = nullptr;
#endif

CompileFunc g_CompileFunc = nullptr;
ExecuteFunc g_ExecuteFunc = nullptr;

bool LoadSolsticeEngine() {
#ifdef _WIN32
    g_SolsticeDLL = LoadLibraryA("SolsticeEngine.dll");
    if (!g_SolsticeDLL) {
        std::cerr << "Failed to load SolsticeEngine.dll" << std::endl;
        return false;
    }

    InitializeFunc initFunc = (InitializeFunc)GetProcAddress(g_SolsticeDLL, "Initialize");
    if (!initFunc) {
        std::cerr << "Failed to find Initialize function in SolsticeEngine.dll" << std::endl;
        FreeLibrary(g_SolsticeDLL);
        g_SolsticeDLL = nullptr;
        return false;
    }

    // Get Compile function
    g_CompileFunc = (CompileFunc)GetProcAddress(g_SolsticeDLL, "Compile");
    if (!g_CompileFunc) {
        std::cerr << "Warning: Compile function not found in SolsticeEngine.dll" << std::endl;
    }

    // Get Execute function
    g_ExecuteFunc = (ExecuteFunc)GetProcAddress(g_SolsticeDLL, "Execute");
    if (!g_ExecuteFunc) {
        std::cerr << "Warning: Execute function not found in SolsticeEngine.dll" << std::endl;
    }

    return initFunc();
#else
    g_SolsticeDLL = dlopen("libSolsticeEngine.so", RTLD_LAZY);
    if (!g_SolsticeDLL) {
        std::cerr << "Failed to load libSolsticeEngine.so: " << dlerror() << std::endl;
        return false;
    }

    InitializeFunc initFunc = (InitializeFunc)dlsym(g_SolsticeDLL, "Initialize");
    if (!initFunc) {
        std::cerr << "Failed to find Initialize function: " << dlerror() << std::endl;
        dlclose(g_SolsticeDLL);
        g_SolsticeDLL = nullptr;
        return false;
    }

    // Get Compile function
    g_CompileFunc = (CompileFunc)dlsym(g_SolsticeDLL, "Compile");
    if (!g_CompileFunc) {
        std::cerr << "Warning: Compile function not found: " << dlerror() << std::endl;
    }

    // Get Execute function
    g_ExecuteFunc = (ExecuteFunc)dlsym(g_SolsticeDLL, "Execute");
    if (!g_ExecuteFunc) {
        std::cerr << "Warning: Execute function not found: " << dlerror() << std::endl;
    }

    return initFunc();
#endif
}

void UnloadSolsticeEngine() {
    if (g_SolsticeDLL) {
#ifdef _WIN32
        ShutdownFunc shutdownFunc = (ShutdownFunc)GetProcAddress(g_SolsticeDLL, "Shutdown");
        if (shutdownFunc) {
            shutdownFunc();
        }
        FreeLibrary(g_SolsticeDLL);
#else
        ShutdownFunc shutdownFunc = (ShutdownFunc)dlsym(g_SolsticeDLL, "Shutdown");
        if (shutdownFunc) {
            shutdownFunc();
        }
        dlclose(g_SolsticeDLL);
#endif
        g_SolsticeDLL = nullptr;
    }
}

// Tab representing an open file
struct Tab {
    std::string FilePath;
    std::string CodeBuffer;
    bool UnsavedChanges = false;
    int Id; // Unique identifier

    Tab() : Id(-1) {}
    Tab(int id, const std::string& path) : FilePath(path), Id(id) {}
};

// Simple script editor state
struct EditorState {
    std::vector<Tab> Tabs;
    int ActiveTabId = -1;
    int NextTabId = 1; // Counter for unique tab IDs

    std::string CompilationOutput;
    std::string ScriptOutput;
    int SelectedTab = 0; // 0 = Compilation, 1 = Script Output

    // File browser
    std::string FileBrowserRootPath = "scripts";
    std::set<std::string> ExpandedDirectories;

    // File operation dialogs
    bool ShowNewFileDialog = false;
    bool ShowNewFolderDialog = false;
    bool ShowRenameDialog = false;
    bool ShowDeleteConfirmDialog = false;
    char DialogInputBuffer[256] = {0};
    std::string DialogTargetPath; // Path for rename/delete operations
    std::string DialogParentPath; // Parent directory for new file/folder

    // Legacy members (will be removed after tab migration)
    bool ShowFileBrowser = false;
    std::vector<std::string> RecentFiles;
};

void LoadFont() {
    // Make sure we're using the LibUI ImGui context
    ImGuiContext* context = LibUI::Core::GetContext();
    if (!context) {
        std::cerr << "LoadFont: ERROR - LibUI context is null!" << std::endl;
        return;
    }
    ImGui::SetCurrentContext(context);
    ImGuiIO& io = ImGui::GetIO();

    // Try multiple font paths
    const char* fontPaths[] = {
        "assets/fonts/Roboto-Medium.ttf",
        "../assets/fonts/Roboto-Medium.ttf",
        "../../assets/fonts/Roboto-Medium.ttf",
        "example/Disco/assets/fonts/Roboto-Medium.ttf",
        "example/Hyperbourne/assets/fonts/Roboto-Medium.ttf",
        "3rdparty/imgui/misc/fonts/Roboto-Medium.ttf"
    };

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 3;
    fontConfig.PixelSnapH = false;
    const float fontSize = 17.0f;

    ImFont* font = nullptr;
    for (const char* path : fontPaths) {
        if (std::filesystem::exists(path)) {
            font = io.Fonts->AddFontFromFileTTF(path, fontSize, &fontConfig);
            if (font) {
                std::cout << "Loaded font from: " << path << std::endl;
                break;
            }
        }
    }

    if (!font) {
        std::cout << "Could not load Roboto font, using default" << std::endl;
        io.Fonts->AddFontDefault();
    } else {
        io.FontDefault = font;
    }

    // Build font atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Note: The ImGui OpenGL3 backend will upload the font texture automatically
    // on the first NewFrame() call, so we don't need to do it here
}

// Moonwalk keywords for syntax highlighting
static const std::set<std::string> g_Keywords = {
    "let", "if", "else", "while", "for", "in", "switch", "case", "default",
    "break", "continue", "match", "function", "@function", "@Entry", "@noexport",
    "return", "module", "import", "class", "new", "true", "false"
};

// Check if a word is a keyword
bool IsKeyword(const std::string& word) {
    return g_Keywords.find(word) != g_Keywords.end();
}

// Render text with syntax highlighting
void RenderSyntaxHighlightedText(const std::string& text, ImVec2 size) {
    ImGui::BeginChild("##code_editor", size, false, ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Colors for syntax highlighting
    ImU32 keywordColor = IM_COL32(86, 156, 214, 255);      // Blue for keywords
    ImU32 stringColor = IM_COL32(206, 145, 120, 255);      // Orange for strings
    ImU32 numberColor = IM_COL32(181, 206, 168, 255);      // Green for numbers
    ImU32 commentColor = IM_COL32(106, 153, 85, 255);      // Green for comments
    ImU32 defaultColor = ImGui::GetColorU32(ImGuiCol_Text);

    std::string currentWord;
    bool inString = false;
    bool inComment = false;
    char stringChar = 0;
    size_t wordStart = 0;

    float x = startPos.x;
    float y = startPos.y;
    float lineHeight = ImGui::GetTextLineHeight();
    float charWidth = ImGui::CalcTextSize("A").x;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        // Handle comments
        if (!inString && i + 1 < text.size() && c == '/' && text[i + 1] == '/') {
            inComment = true;
        }
        if (inComment && c == '\n') {
            inComment = false;
        }

        if (inComment) {
            drawList->AddText(ImVec2(x, y), commentColor, &c, &c + 1);
            x += charWidth;
            if (c == '\n') {
                x = startPos.x;
                y += lineHeight;
            }
            continue;
        }

        // Handle strings
        if ((c == '"' || c == '\'') && (i == 0 || text[i - 1] != '\\')) {
            if (!inString) {
                inString = true;
                stringChar = c;
            } else if (c == stringChar) {
                inString = false;
            }
        }

        if (inString) {
            drawList->AddText(ImVec2(x, y), stringColor, &c, &c + 1);
            x += charWidth;
            if (c == '\n') {
                x = startPos.x;
                y += lineHeight;
            }
            continue;
        }

        // Handle words and numbers
        if (std::isalnum(c) || c == '_' || c == '@') {
            if (currentWord.empty()) {
                wordStart = i;
            }
            currentWord += c;
        } else {
            // Render accumulated word
            if (!currentWord.empty()) {
                ImU32 color = defaultColor;
                if (IsKeyword(currentWord)) {
                    color = keywordColor;
                } else if (std::isdigit(currentWord[0]) || (currentWord[0] == '-' && currentWord.size() > 1 && std::isdigit(currentWord[1]))) {
                    color = numberColor;
                }

                const char* wordStr = currentWord.c_str();
                drawList->AddText(ImVec2(x, y), color, wordStr, wordStr + currentWord.size());
                x += ImGui::CalcTextSize(wordStr).x;
                currentWord.clear();
            }

            // Render current character
            if (c == '\n') {
                x = startPos.x;
                y += lineHeight;
            } else if (c != '\r' && c != '\t') {
                drawList->AddText(ImVec2(x, y), defaultColor, &c, &c + 1);
                x += charWidth;
            } else if (c == '\t') {
                x += charWidth * 4; // Tab = 4 spaces
            }
        }
    }

    // Render any remaining word
    if (!currentWord.empty()) {
        ImU32 color = defaultColor;
        if (IsKeyword(currentWord)) {
            color = keywordColor;
        } else if (std::isdigit(currentWord[0]) || (currentWord[0] == '-' && currentWord.size() > 1 && std::isdigit(currentWord[1]))) {
            color = numberColor;
        }
        const char* wordStr = currentWord.c_str();
        drawList->AddText(ImVec2(x, y), color, wordStr, wordStr + currentWord.size());
    }

    ImGui::EndChild();
}

// Tab management functions
Tab* GetActiveTab(EditorState& state) {
    if (state.ActiveTabId == -1) return nullptr;
    for (auto& tab : state.Tabs) {
        if (tab.Id == state.ActiveTabId) {
            return &tab;
        }
    }
    return nullptr;
}

void OpenFileInTab(EditorState& state, const std::string& filePath) {
    // Check if file is already open
    for (auto& tab : state.Tabs) {
        if (tab.FilePath == filePath) {
            state.ActiveTabId = tab.Id;
            return;
        }
    }

    // Create new tab
    Tab newTab(state.NextTabId++, filePath);

    // Load file content
    if (std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath)) {
        std::ifstream file(filePath);
        if (file.is_open()) {
            newTab.CodeBuffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
        }
    }

    state.Tabs.push_back(newTab);
    state.ActiveTabId = newTab.Id;
}

void CloseTab(EditorState& state, int tabId) {
    auto it = std::remove_if(state.Tabs.begin(), state.Tabs.end(),
        [tabId](const Tab& tab) { return tab.Id == tabId; });

    if (it != state.Tabs.end()) {
        state.Tabs.erase(it, state.Tabs.end());

        // If we closed the active tab, switch to another
        if (state.ActiveTabId == tabId) {
            if (!state.Tabs.empty()) {
                state.ActiveTabId = state.Tabs.back().Id;
            } else {
                state.ActiveTabId = -1;
            }
        }
    }
}

void SwitchToTab(EditorState& state, int tabId) {
    for (const auto& tab : state.Tabs) {
        if (tab.Id == tabId) {
            state.ActiveTabId = tabId;
            return;
        }
    }
}

void CompileScript(EditorState& state) {
    if (!g_CompileFunc) {
        state.CompilationOutput += "Error: Compile function not available. Make sure SolsticeEngine.dll is loaded.\n";
        return;
    }

    Tab* activeTab = GetActiveTab(state);
    if (!activeTab) {
        state.CompilationOutput += "Error: No active tab to compile.\n";
        return;
    }

    state.CompilationOutput += "Compiling...\n";

    char errorBuffer[1024] = {0};
    int result = g_CompileFunc(activeTab->CodeBuffer.c_str(), errorBuffer, sizeof(errorBuffer));

    if (result == 0) {
        state.CompilationOutput += "✓ Compilation successful!\n";
    } else {
        state.CompilationOutput += "✗ Compilation failed:\n";
        if (errorBuffer[0] != '\0') {
            state.CompilationOutput += std::string(errorBuffer) + "\n";
        } else {
            state.CompilationOutput += "Unknown error\n";
        }
    }
}

void RunScript(EditorState& state) {
    if (!g_ExecuteFunc) {
        state.ScriptOutput += "Error: Execute function not available. Make sure SolsticeEngine.dll is loaded.\n";
        return;
    }

    Tab* activeTab = GetActiveTab(state);
    if (!activeTab) {
        state.ScriptOutput += "Error: No active tab to run.\n";
        return;
    }

    state.ScriptOutput += "Running script...\n";

    char outputBuffer[8192] = {0};
    char errorBuffer[1024] = {0};
    int result = g_ExecuteFunc(activeTab->CodeBuffer.c_str(), outputBuffer, sizeof(outputBuffer), errorBuffer, sizeof(errorBuffer));

    if (result == 0) {
        if (outputBuffer[0] != '\0') {
            state.ScriptOutput += outputBuffer;
        } else {
            state.ScriptOutput += "Script executed successfully (no output).\n";
        }
    } else {
        state.ScriptOutput += "✗ Execution failed:\n";
        if (errorBuffer[0] != '\0') {
            state.ScriptOutput += std::string(errorBuffer) + "\n";
        } else {
            state.ScriptOutput += "Unknown error\n";
        }
    }
}

// Forward declarations
void RenderFileBrowser(EditorState& state);
void RenderDirectoryTree(EditorState& state, const std::filesystem::path& dirPath, int depth = 0);
void LoadDirectoryFiles(EditorState& state, const std::filesystem::path& dirPath);

// File operation helper functions
void CreateNewFile(EditorState& state, const std::filesystem::path& parentDir, const std::string& fileName) {
    try {
        std::filesystem::path filePath = parentDir / fileName;
        if (fileName.length() < 3 || fileName.substr(fileName.length() - 3) != ".mw") {
            filePath = parentDir / (fileName + ".mw");
        }

        if (std::filesystem::exists(filePath)) {
            state.CompilationOutput += "Error: File already exists: " + filePath.string() + "\n";
            return;
        }

        // Create empty file
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << "-- New file\n";
            file.close();
            OpenFileInTab(state, filePath.string());
            state.CompilationOutput += "Created file: " + filePath.string() + "\n";
        } else {
            state.CompilationOutput += "Error: Failed to create file: " + filePath.string() + "\n";
        }
    } catch (const std::exception& e) {
        state.CompilationOutput += "Error creating file: " + std::string(e.what()) + "\n";
    }
}

void CreateNewFolder(EditorState& state, const std::filesystem::path& parentDir, const std::string& folderName) {
    try {
        std::filesystem::path folderPath = parentDir / folderName;

        if (std::filesystem::exists(folderPath)) {
            state.CompilationOutput += "Error: Folder already exists: " + folderPath.string() + "\n";
            return;
        }

        if (std::filesystem::create_directory(folderPath)) {
            state.CompilationOutput += "Created folder: " + folderPath.string() + "\n";
            // Refresh the tree by expanding parent
            state.ExpandedDirectories.insert(parentDir.string());
        } else {
            state.CompilationOutput += "Error: Failed to create folder: " + folderPath.string() + "\n";
        }
    } catch (const std::exception& e) {
        state.CompilationOutput += "Error creating folder: " + std::string(e.what()) + "\n";
    }
}

void DeleteFileOrFolder(EditorState& state, const std::filesystem::path& path) {
    try {
        if (std::filesystem::is_directory(path)) {
            std::filesystem::remove_all(path);
            state.CompilationOutput += "Deleted folder: " + path.string() + "\n";
        } else {
            std::filesystem::remove(path);
            state.CompilationOutput += "Deleted file: " + path.string() + "\n";

            // Close tab if file was open
            for (auto it = state.Tabs.begin(); it != state.Tabs.end();) {
                if (it->FilePath == path.string()) {
                    if (it->Id == state.ActiveTabId) {
                        if (state.Tabs.size() > 1) {
                            state.ActiveTabId = (it == state.Tabs.begin()) ? state.Tabs[1].Id : state.Tabs[0].Id;
                        } else {
                            state.ActiveTabId = -1;
                        }
                    }
                    it = state.Tabs.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } catch (const std::exception& e) {
        state.CompilationOutput += "Error deleting: " + std::string(e.what()) + "\n";
    }
}

void RenameFileOrFolder(EditorState& state, const std::filesystem::path& oldPath, const std::string& newName) {
    try {
        std::filesystem::path newPath = oldPath.parent_path() / newName;

        if (std::filesystem::exists(newPath)) {
            state.CompilationOutput += "Error: A file or folder with that name already exists\n";
            return;
        }

        std::filesystem::rename(oldPath, newPath);
        state.CompilationOutput += "Renamed: " + oldPath.string() + " -> " + newPath.string() + "\n";

        // Update tab if file was open
        for (auto& tab : state.Tabs) {
            if (tab.FilePath == oldPath.string()) {
                tab.FilePath = newPath.string();
            }
        }
    } catch (const std::exception& e) {
        state.CompilationOutput += "Error renaming: " + std::string(e.what()) + "\n";
    }
}

// Render dialog modals
void RenderFileOperationDialogs(EditorState& state) {
    // New File Dialog
    if (state.ShowNewFileDialog) {
        ImGui::OpenPopup("New File");
        state.ShowNewFileDialog = false;
        memset(state.DialogInputBuffer, 0, sizeof(state.DialogInputBuffer));
    }

    if (ImGui::BeginPopupModal("New File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file name:");
        ImGui::InputText("##newfile", state.DialogInputBuffer, sizeof(state.DialogInputBuffer));

        if (ImGui::Button("Create")) {
            if (strlen(state.DialogInputBuffer) > 0) {
                CreateNewFile(state, state.DialogParentPath, std::string(state.DialogInputBuffer));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // New Folder Dialog
    if (state.ShowNewFolderDialog) {
        ImGui::OpenPopup("New Folder");
        state.ShowNewFolderDialog = false;
        memset(state.DialogInputBuffer, 0, sizeof(state.DialogInputBuffer));
    }

    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        ImGui::InputText("##newfolder", state.DialogInputBuffer, sizeof(state.DialogInputBuffer));

        if (ImGui::Button("Create")) {
            if (strlen(state.DialogInputBuffer) > 0) {
                CreateNewFolder(state, state.DialogParentPath, std::string(state.DialogInputBuffer));
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Rename Dialog (also used for Save As)
    if (state.ShowRenameDialog) {
        ImGui::OpenPopup(state.DialogTargetPath.empty() ? "Save As" : "Rename");
        state.ShowRenameDialog = false;
        std::string currentName;
        if (state.DialogTargetPath.empty()) {
            // Save As for new file
            currentName = "untitled.mw";
        } else {
            currentName = std::filesystem::path(state.DialogTargetPath).filename().string();
        }
        memset(state.DialogInputBuffer, 0, sizeof(state.DialogInputBuffer));
        strncpy(state.DialogInputBuffer, currentName.c_str(), sizeof(state.DialogInputBuffer) - 1);
    }

    if (ImGui::BeginPopupModal(state.DialogTargetPath.empty() ? "Save As" : "Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.DialogTargetPath.empty()) {
            ImGui::Text("Enter file name:");
        } else {
            ImGui::Text("Enter new name:");
        }
        ImGui::InputText("##rename", state.DialogInputBuffer, sizeof(state.DialogInputBuffer));

        if (ImGui::Button(state.DialogTargetPath.empty() ? "Save" : "Rename")) {
            if (strlen(state.DialogInputBuffer) > 0) {
                if (state.DialogTargetPath.empty()) {
                    // Save As - create new file
                    Tab* activeTab = GetActiveTab(state);
                    if (activeTab) {
                        std::string fileName = state.DialogInputBuffer;
                        if (fileName.length() < 3 || fileName.substr(fileName.length() - 3) != ".mw") {
                            fileName += ".mw";
                        }
                        std::filesystem::path filePath = std::filesystem::path(state.DialogParentPath) / fileName;

                        std::ofstream file(filePath);
                        if (file.is_open()) {
                            file << activeTab->CodeBuffer;
                            file.close();
                            activeTab->FilePath = filePath.string();
                            activeTab->UnsavedChanges = false;
                            state.CompilationOutput += "Saved: " + filePath.string() + "\n";
                        } else {
                            state.CompilationOutput += "Error: Failed to save file: " + filePath.string() + "\n";
                        }
                    }
                } else {
                    // Rename existing file/folder
                    RenameFileOrFolder(state, state.DialogTargetPath, std::string(state.DialogInputBuffer));
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Delete Confirmation Dialog
    if (state.ShowDeleteConfirmDialog) {
        ImGui::OpenPopup("Delete Confirmation");
        state.ShowDeleteConfirmDialog = false;
    }

    if (ImGui::BeginPopupModal("Delete Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::string itemName = std::filesystem::path(state.DialogTargetPath).filename().string();
        ImGui::Text("Are you sure you want to delete '%s'?", itemName.c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "This action cannot be undone!");

        if (ImGui::Button("Delete")) {
            DeleteFileOrFolder(state, state.DialogTargetPath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void RenderEditor(EditorState& state) {
    // Create fullscreen window
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("Sharpon Editor", nullptr, windowFlags)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    // Create new untitled tab
                    Tab newTab(state.NextTabId++, "");
                    newTab.CodeBuffer = "-- New file\n";
                    state.Tabs.push_back(newTab);
                    state.ActiveTabId = newTab.Id;
                }
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    // File browser is always visible in sidebar
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    Tab* activeTab = GetActiveTab(state);
                    if (activeTab) {
                        if (activeTab->FilePath.empty()) {
                            // Show save as dialog
                            state.DialogParentPath = state.FileBrowserRootPath;
                            state.DialogTargetPath = ""; // Indicates this is a Save As
                            state.ShowRenameDialog = true; // Reuse rename dialog for Save As
                        } else {
                            std::ofstream file(activeTab->FilePath);
                            if (file.is_open()) {
                                file << activeTab->CodeBuffer;
                                file.close();
                                activeTab->UnsavedChanges = false;
                                state.CompilationOutput += "Saved: " + activeTab->FilePath + "\n";
                            } else {
                                state.CompilationOutput += "Error: Failed to save file: " + activeTab->FilePath + "\n";
                            }
                        }
                    }
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    Tab* activeTab = GetActiveTab(state);
                    if (activeTab) {
                        state.DialogParentPath = activeTab->FilePath.empty() ? state.FileBrowserRootPath : std::filesystem::path(activeTab->FilePath).parent_path().string();
                        state.DialogTargetPath = activeTab->FilePath;
                        state.ShowRenameDialog = true; // Reuse rename dialog for Save As
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    // Will be handled by main loop
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Compile", "F5")) {
                    CompileScript(state);
                }
                if (ImGui::MenuItem("Run", "F6")) {
                    RunScript(state);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Toolbar with action buttons
        if (ImGui::Button("Compile (F5)")) {
            CompileScript(state);
        }
        ImGui::SameLine();
        if (ImGui::Button("Run (F6)")) {
            RunScript(state);
        }
        ImGui::Separator();

        // Tab bar
        if (ImGui::BeginTabBar("##file_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
            for (size_t i = 0; i < state.Tabs.size(); i++) {
                Tab& tab = state.Tabs[i];
                bool isActive = (tab.Id == state.ActiveTabId);

                std::string tabLabel = tab.FilePath.empty() ? "Untitled" : std::filesystem::path(tab.FilePath).filename().string();
                if (tab.UnsavedChanges) {
                    tabLabel += " *";
                }

                ImGuiTabItemFlags flags = isActive ? ImGuiTabItemFlags_SetSelected : 0;
                bool open = true;

                if (ImGui::BeginTabItem(tabLabel.c_str(), &open, flags)) {
                    if (!isActive) {
                        SwitchToTab(state, tab.Id);
                    }
                    ImGui::EndTabItem();
                }

                if (!open) {
                    CloseTab(state, tab.Id);
                }
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();

        // Get available space and split it
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        float outputHeight = 200.0f; // Fixed height for output panel
        float editorHeight = availableSize.y - outputHeight - ImGui::GetStyle().ItemSpacing.y;
        float sidebarWidth = 250.0f;

        // Main horizontal split: File Browser (left) | Editor + Output (right)
        ImGui::BeginChild("##main_split", ImVec2(0, editorHeight + outputHeight + ImGui::GetStyle().ItemSpacing.y), false);

        // File browser sidebar (left)
        RenderFileBrowser(state);

        ImGui::SameLine();

        // Editor and output section (right)
        ImGui::BeginChild("##editor_output_split", ImVec2(0, 0), false);

        // Code editor section (top)
        ImGui::BeginChild("##editor_section", ImVec2(0, editorHeight), false);

        Tab* activeTab = GetActiveTab(state);
        if (activeTab) {
            // Calculate line count for line numbers
            int lineCount = 1;
            for (char c : activeTab->CodeBuffer) {
                if (c == '\n') lineCount++;
            }

            // Create a split view: line numbers on left, editor on right
            ImGui::BeginChild("##editor_container", ImVec2(0, 0), false);

            // Line numbers panel (fixed width)
            float lineNumberWidth = ImGui::CalcTextSize("9999").x + 10.0f;
            ImGui::BeginChild("##line_numbers", ImVec2(lineNumberWidth, 0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            for (int i = 1; i <= lineCount; i++) {
                ImGui::Text("%4d", i);
            }
            ImGui::PopStyleColor();

            ImGui::EndChild();

            ImGui::SameLine();

            // Code editor
            ImGui::BeginChild("##editor", ImVec2(0, 0), false);

            // Right-click context menu for editor
            if (ImGui::BeginPopupContextWindow("##editor_context", ImGuiPopupFlags_MouseButtonRight)) {
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                    ImGui::SetClipboardText(activeTab->CodeBuffer.c_str());
                    // TODO: Implement actual cut (remove selected text)
                }
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                    ImGui::SetClipboardText(activeTab->CodeBuffer.c_str());
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                    const char* clipboard = ImGui::GetClipboardText();
                    if (clipboard) {
                        activeTab->CodeBuffer += clipboard;
                        activeTab->UnsavedChanges = true;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                    // TODO: Implement select all
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                    // TODO: Implement undo
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                    // TODO: Implement redo
                }
                ImGui::EndPopup();
            }

            char* buffer = activeTab->CodeBuffer.data();
            size_t bufferSize = activeTab->CodeBuffer.size() + 1024; // Add extra space
            activeTab->CodeBuffer.resize(bufferSize);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
            if (LibUI::Widgets::InputTextMultiline("##editor_input", activeTab->CodeBuffer.data(), bufferSize, ImVec2(-1, -1), flags)) {
                activeTab->UnsavedChanges = true;
            // Trim buffer to actual size
                size_t len = strlen(activeTab->CodeBuffer.data());
                activeTab->CodeBuffer.resize(len);
            }

            ImGui::EndChild();
            ImGui::EndChild();
        } else {
            // No active tab - show empty state
            ImGui::Text("No file open. Create a new file or open an existing one.");
        }

        ImGui::EndChild();

        ImGui::Separator();

        // Output section (bottom) with tabs
        ImGui::BeginChild("##output_section", ImVec2(0, outputHeight), false);

        // Tabs for Compilation and Script Output
        if (ImGui::BeginTabBar("##output_tabs")) {
            if (ImGui::BeginTabItem("Compilation")) {
                state.SelectedTab = 0;
                ImVec2 outputSize = ImGui::GetContentRegionAvail();
                // Only show scrollbars when content overflows
                ImGuiWindowFlags outputFlags = ImGuiWindowFlags_HorizontalScrollbar;
                if (ImGui::BeginChild("##compilation_output", outputSize, true, outputFlags)) {
                    ImGui::TextUnformatted(state.CompilationOutput.c_str());
                    // Auto-scroll to bottom
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f) {
                        ImGui::SetScrollHereY(1.0f);
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Script Output")) {
                state.SelectedTab = 1;
    ImVec2 outputSize = ImGui::GetContentRegionAvail();
                // Only show scrollbars when content overflows
                ImGuiWindowFlags outputFlags = ImGuiWindowFlags_HorizontalScrollbar;
                if (ImGui::BeginChild("##script_output", outputSize, true, outputFlags)) {
                    ImGui::TextUnformatted(state.ScriptOutput.c_str());
            // Auto-scroll to bottom
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndChild(); // End output section
        ImGui::EndChild(); // End editor_output_split
        ImGui::EndChild(); // End main_split
    }
    ImGui::End();
}

// Render directory tree recursively
void RenderDirectoryTree(EditorState& state, const std::filesystem::path& dirPath, int depth) {
    if (depth > 10) return; // Prevent infinite recursion

    try {
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
            return;
        }

        std::string dirKey = dirPath.string();
        bool isExpanded = state.ExpandedDirectories.find(dirKey) != state.ExpandedDirectories.end();

        // Get directory name for display
        std::string dirName = dirPath.filename().string();
        if (dirName.empty()) {
            dirName = dirPath.string();
        }

        // Create tree node for directory
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (isExpanded) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        bool nodeOpen = ImGui::TreeNodeEx(dirName.c_str(), flags);

        // Handle right-click context menu for directory
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Load Directory")) {
                LoadDirectoryFiles(state, dirPath);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New File")) {
                state.DialogParentPath = dirPath.string();
                state.ShowNewFileDialog = true;
            }
            if (ImGui::MenuItem("New Folder")) {
                state.DialogParentPath = dirPath.string();
                state.ShowNewFolderDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
                state.DialogTargetPath = dirPath.string();
                state.ShowDeleteConfirmDialog = true;
            }
            if (ImGui::MenuItem("Rename")) {
                state.DialogTargetPath = dirPath.string();
                state.ShowRenameDialog = true;
            }
            ImGui::EndPopup();
        }

        // Toggle expanded state
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            if (isExpanded) {
                state.ExpandedDirectories.erase(dirKey);
            } else {
                state.ExpandedDirectories.insert(dirKey);
            }
        }

        if (nodeOpen) {
            // List files and subdirectories
            std::vector<std::filesystem::path> entries;
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                entries.push_back(entry.path());
            }

            // Sort: directories first, then files
            std::sort(entries.begin(), entries.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
                bool aIsDir = std::filesystem::is_directory(a);
                bool bIsDir = std::filesystem::is_directory(b);
                if (aIsDir != bIsDir) {
                    return aIsDir > bIsDir; // Directories first
                }
                return a.filename().string() < b.filename().string();
            });

            for (const auto& entry : entries) {
                if (std::filesystem::is_directory(entry)) {
                    RenderDirectoryTree(state, entry, depth + 1);
                } else {
                    // Render file
                    std::string fileName = entry.filename().string();
                    ImGui::TreeNodeEx(fileName.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet);

                    // Handle file click
                    if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0)) {
                        OpenFileInTab(state, entry.string());
                    }

                    // Handle right-click context menu for file
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Open")) {
                            OpenFileInTab(state, entry.string());
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem("Delete")) {
                            state.DialogTargetPath = entry.string();
                            state.ShowDeleteConfirmDialog = true;
                        }
                        if (ImGui::MenuItem("Rename")) {
                            state.DialogTargetPath = entry.string();
                            state.ShowRenameDialog = true;
                        }
                        ImGui::EndPopup();
                    }
                }
            }

            ImGui::TreePop();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // Skip directories we can't access
    }
}

void LoadDirectoryFiles(EditorState& state, const std::filesystem::path& dirPath) {
    try {
        int fileCount = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".mw") {
                    OpenFileInTab(state, entry.path().string());
                    fileCount++;
                }
            }
        }
        state.CompilationOutput += "Loaded " + std::to_string(fileCount) + " file(s) from " + dirPath.string() + "\n";
    } catch (const std::exception& e) {
        state.CompilationOutput += "Error loading directory: " + std::string(e.what()) + "\n";
    }
}

void RenderFileBrowser(EditorState& state) {
    float sidebarWidth = 250.0f;
    ImGui::BeginChild("##file_browser", ImVec2(sidebarWidth, 0), true);

    // Header
    ImGui::Text("File Browser");
    ImGui::Separator();

    // Current path display
    ImGui::TextWrapped("Path: %s", state.FileBrowserRootPath.c_str());
    ImGui::Separator();

    // Load Directory button
    if (ImGui::Button("Load Directory")) {
        if (std::filesystem::exists(state.FileBrowserRootPath)) {
            LoadDirectoryFiles(state, state.FileBrowserRootPath);
        }
    }
    ImGui::Separator();

    // Render directory tree
    if (std::filesystem::exists(state.FileBrowserRootPath)) {
        RenderDirectoryTree(state, state.FileBrowserRootPath);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Directory not found: %s", state.FileBrowserRootPath.c_str());
    }

    ImGui::EndChild();
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    int sdlInitResult = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (sdlInitResult < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set OpenGL attributes BEFORE creating the window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
    SDL_Window* window = SDL_CreateWindow("Sharpon - Script Editor", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Make the OpenGL context current (required before initializing ImGui OpenGL3 renderer)
    if (!SDL_GL_MakeCurrent(window, glContext)) {
        std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Enable vsync
    SDL_GL_SetSwapInterval(1);

    // Verify context is current before initializing LibUI
    SDL_Window* verifyWindow = SDL_GL_GetCurrentWindow();
    SDL_GLContext verifyContext = SDL_GL_GetCurrentContext();
    if (verifyWindow != window || verifyContext != glContext) {
        std::cerr << "ERROR: OpenGL context verification failed!" << std::endl;
        std::cerr << "  Expected window: " << window << ", got: " << verifyWindow << std::endl;
        std::cerr << "  Expected context: " << glContext << ", got: " << verifyContext << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    // Initialize LibUI
    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI initialization failed" << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Load font
    LoadFont();

    // Load Solstice engine
    bool engineLoaded = LoadSolsticeEngine();
    if (engineLoaded) {
        std::cout << "Solstice engine loaded successfully" << std::endl;
    } else {
        std::cout << "Warning: Solstice engine not loaded (editor will still work)" << std::endl;
    }

    // Editor state
    EditorState editorState;
    editorState.CompilationOutput = "Sharpon Script Editor v0.1.0\n";
    editorState.ScriptOutput = "";
    if (engineLoaded) {
        editorState.CompilationOutput += "✓ Solstice engine loaded\n";
    } else {
        editorState.CompilationOutput += "⚠ Solstice engine not loaded (compilation/execution may not work)\n";
    }

    // Create initial untitled tab
    Tab initialTab(editorState.NextTabId++, "");
    initialTab.CodeBuffer = "-- Sharpon Script Editor\n-- Start typing your script here...\n\n@Entry {\n    print(\"Hello, Moonwalk!\");\n}\n";
    editorState.Tabs.push_back(initialTab);
    editorState.ActiveTabId = initialTab.Id;

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            LibUI::Core::ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                // Window resize is handled automatically by ImGui
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                // Handle keyboard shortcuts
                // In SDL3, use event.key.key instead of event.key.keysym.sym
                if (event.key.key == SDLK_F5) {
                    CompileScript(editorState);
                } else if (event.key.key == SDLK_F6) {
                    RunScript(editorState);
                }
            }
        }

        // New frame
        LibUI::Core::NewFrame();

        // Render UI (single fullscreen window)
        RenderEditor(editorState);

        // Render file operation dialogs
        RenderFileOperationDialogs(editorState);

        // Render
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        LibUI::Core::Render();

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    UnloadSolsticeEngine();
    LibUI::Core::Shutdown();
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

