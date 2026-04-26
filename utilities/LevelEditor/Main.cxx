#include "JackhammerApp.hxx"

#include "LibUI/Tools/Win32ExceptionDiag.hxx"

int main(int argc, char** argv) {
    LibUI::Tools::Win32InitCrashDiagnostics();
#if defined(_WIN32)
    LibUI::Tools::Win32InstallUtilityTopLevelFilter("Jackhammer");
#endif
    return Jackhammer::RunApp(argc, argv);
}
