#include "crash_diagnostics.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

namespace {
LONG CALLBACK LogFloatingPointFault(EXCEPTION_POINTERS* exception) {
  const auto code = exception->ExceptionRecord->ExceptionCode;
  if (code < EXCEPTION_FLT_DENORMAL_OPERAND || code > EXCEPTION_FLT_UNDERFLOW) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  const HANDLE file = CreateFileW(L"logs/floating-point-fault.txt", FILE_APPEND_DATA,
                                FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;
  char line[1024];
  DWORD written;
  auto write = [&](int length) { WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr); };
  CONTEXT context = *exception->ContextRecord;
  write(std::snprintf(line, sizeof(line), "exception=0x%08lX MXCSR=0x%08lX thread=%lu\r\n",
                      code, context.MxCsr, GetCurrentThreadId()));
  for (int frame = 0; frame < 32 && context.Rip; ++frame) {
    HMODULE module = nullptr;
    char name[MAX_PATH] = "unknown";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<const char*>(context.Rip), &module);
    if (module) GetModuleFileNameA(module, name, MAX_PATH);
    write(std::snprintf(line, sizeof(line), "%02d pc=0x%016llX rva=0x%llX module=%s\r\n", frame,
                        context.Rip, context.Rip - reinterpret_cast<DWORD64>(module), name));
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
  // Preserve the fault. This observer does not change guest or host FP behavior.
  return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

void InstallCrashDiagnostics() { AddVectoredExceptionHandler(1, LogFloatingPointFault); }
#else
void InstallCrashDiagnostics() {}
#endif
