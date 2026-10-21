#include "crash_diagnostics.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
wchar_t report_path[32768]{};
wchar_t module_path[32768]{};
char module_name[1024]{};
char line[2048]{};
void* observer = nullptr;
volatile LONG writing = 0;

// Scratch buffers are protected by writing. Exception callbacks also run on
// small guest/MMIO stacks, so large stack arrays must never enter this path.
const char* ModuleName(HMODULE module) {
  if (!module) return "unknown";
  const auto length = GetModuleFileNameW(module, module_path, static_cast<DWORD>(std::size(module_path)));
  if (!length || length >= std::size(module_path)) return "unknown";
  const auto* base = module_path;
  for (const auto* p = module_path; *p; ++p) if (*p == L'\\' || *p == L'/') base = p + 1;
  if (WideCharToMultiByte(CP_UTF8, 0, base, -1, module_name, sizeof(module_name), nullptr, nullptr) <= 0)
    return "unknown";
  return module_name;
}

LONG CALLBACK LogFloatingPointFault(EXCEPTION_POINTERS* exception) {
  const auto code = exception->ExceptionRecord->ExceptionCode;
  if (code < EXCEPTION_FLT_DENORMAL_OPERAND || code > EXCEPTION_FLT_UNDERFLOW) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (InterlockedCompareExchange(&writing, 1, 0)) return EXCEPTION_CONTINUE_SEARCH;
  const HANDLE file = CreateFileW(report_path, FILE_APPEND_DATA,
                                FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    InterlockedExchange(&writing, 0);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  DWORD written;
  auto write = [&](int length) {
    if (length > 0) WriteFile(file, line,
        static_cast<DWORD>(std::min<size_t>(length, sizeof(line) - 1)), &written, nullptr);
  };
  CONTEXT context = *exception->ContextRecord;
  write(std::snprintf(line, sizeof(line), "diagnostics_format=2\r\nexception=0x%08lX MXCSR=0x%08lX thread=%lu\r\n",
                      code, context.MxCsr, GetCurrentThreadId()));
  for (int frame = 0; frame < 32 && context.Rip; ++frame) {
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<const char*>(context.Rip), &module);
    write(std::snprintf(line, sizeof(line), "%02d pc=0x%016llX rva=0x%llX module=%s\r\n", frame,
                        context.Rip, context.Rip - reinterpret_cast<DWORD64>(module), ModuleName(module)));
    DWORD64 image_base = 0;
    const auto entry = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
    if (entry) {
      void* handler_data = nullptr;
      DWORD64 establisher = 0;
      RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, entry, &context,
                       &handler_data, &establisher, nullptr);
    } else {
      SIZE_T read = 0;
      if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context.Rsp),
                              &context.Rip, sizeof(context.Rip), &read) || read != sizeof(context.Rip)) break;
      context.Rsp += sizeof(DWORD64);
    }
  }
  FlushFileBuffers(file);
  CloseHandle(file);
  InterlockedExchange(&writing, 0);
  // Preserve the fault. This observer does not change guest or host FP behavior.
  return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

bool InstallCrashDiagnostics(const std::filesystem::path& log_directory) noexcept {
  if (observer) return false;
  try {
    const auto directory = std::filesystem::absolute(log_directory);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    const auto filename = (directory / "floating-point-fault.txt").wstring();
    if (filename.size() >= std::size(report_path)) return false;
    std::copy(filename.begin(), filename.end(), report_path);
    report_path[filename.size()] = L'\0';
    observer = AddVectoredExceptionHandler(1, LogFloatingPointFault);
    return observer != nullptr;
  } catch (...) { return false; }
}
#else
bool InstallCrashDiagnostics(const std::filesystem::path&) noexcept { return false; }
#endif
