#include "crash_diagnostics.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xmmintrin.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
constexpr DWORD kProbe = 0xE04E3301;
bool delivered = false;
DWORD observed_mxcsr = 0;
LONG CALLBACK RecoverProbe(EXCEPTION_POINTERS* exception) {
  if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_INEXACT_RESULT ||
      exception->ExceptionRecord->ExceptionCode == kProbe) {
    delivered = true;
    observed_mxcsr = exception->ContextRecord->MxCsr;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
DWORD WINAPI SmallStackProbe(void*) {
  delivered = false;
  RaiseException(kProbe, 0, 0, nullptr);
  if (!delivered) return 1;
  delivered = false;
  const auto mxcsr = _mm_getcsr();
  RaiseException(EXCEPTION_FLT_INEXACT_RESULT, 0, 0, nullptr);
  return delivered && observed_mxcsr == mxcsr ? 0 : 2;
}
std::string Read(const fs::path& file) {
  std::ifstream input(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main() {
  const auto base = fs::temp_directory_path();
  const auto original = fs::current_path();
  const auto root = base / ("n3-diagnostics-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
  if (!fs::create_directory(root)) return 1;
  int result = 0;
  void* recovery = nullptr;
  try {
    fs::create_directory(root / "working");
    fs::current_path(root / "working");
    std::ofstream(root / "blocked") << "file";
    Require(!InstallCrashDiagnostics(root / "blocked" / "logs"), "Invalid diagnostics destination was accepted");
    const auto logs = root / "not-for-report" / "logs";
    Require(InstallCrashDiagnostics(logs), "Cannot install observer");
    Require(!InstallCrashDiagnostics(logs), "Duplicate observer registered");
    recovery = AddVectoredExceptionHandler(0, RecoverProbe);
    Require(recovery != nullptr, "Cannot install test recovery handler");
    const auto report = logs / "floating-point-fault.txt";
    RaiseException(kProbe, 0, 0, nullptr);
    Require(delivered && !fs::exists(report), "Non-FP exception was swallowed or logged");
    delivered = false;
    const auto mxcsr = _mm_getcsr();
    RaiseException(EXCEPTION_FLT_INEXACT_RESULT, 0, 0, nullptr);
    Require(delivered && observed_mxcsr == mxcsr, "Observer consumed the fault or changed exception context");
    const auto text = Read(report);
    Require(text.find("diagnostics_format=2") != std::string::npos &&
            text.find("exception=0xC000008F") != std::string::npos &&
            text.find("module=n3_crash_diagnostics_test.exe") != std::string::npos,
            "Expected exception and stack frames were not recorded");
    Require(text.find("not-for-report") == std::string::npos &&
            text.find(":\\") == std::string::npos && text.find(":/") == std::string::npos,
            "Absolute path leaked into fault report");
    Require(!fs::exists(root / "working" / "logs"), "Report was written to the working directory");
    const auto thread = CreateThread(nullptr, 64 * 1024, SmallStackProbe, nullptr,
                                    STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    Require(thread != nullptr, "Cannot create a small-stack test thread");
    const auto waited = WaitForSingleObject(thread, 10000);
    DWORD exit_code = 1;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    Require(waited == WAIT_OBJECT_0 && exit_code == 0, "Observer failed on a 64 KiB stack");
    std::cout << "Profile destination, private stack frames, exception pass-through, context preservation, 64 KiB stack, and setup failure checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
  if (recovery) RemoveVectoredExceptionHandler(recovery);
  fs::current_path(original);
  if (fs::weakly_canonical(root).parent_path() == fs::weakly_canonical(base) &&
      root.filename().string().starts_with("n3-diagnostics-test-")) fs::remove_all(root);
  return result;
}
