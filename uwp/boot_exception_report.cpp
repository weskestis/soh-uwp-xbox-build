#include "boot_exception_report.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>

namespace {

using Writer = void (*)(const char*);

void Write(Writer writer, const char* format, ...) {
    char line[768] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    writer(line);
}

void WriteAddress(Writer writer, const char* label, unsigned frame, std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) == 0 ||
        memory.Type != MEM_IMAGE || memory.AllocationBase == nullptr) {
        Write(writer, "crash.%s=%u address=%p module=(not-an-image)", label, frame,
              reinterpret_cast<const void*>(address));
        return;
    }

    wchar_t path[1024] = {};
    const DWORD length = GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase),
                                          path, static_cast<DWORD>(sizeof(path) / sizeof(path[0])));
    const wchar_t* name = L"(unknown)";
    if (length > 0 && length < sizeof(path) / sizeof(path[0])) {
        name = path;
        for (const wchar_t* p = path; *p != L'\0'; ++p) {
            if (*p == L'\\' || *p == L'/') name = p + 1;
        }
    }
    const auto base = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
    Write(writer, "crash.%s=%u address=%p module=%ls base=%p rva=0x%llX", label, frame,
          reinterpret_cast<const void*>(address), name, memory.AllocationBase,
          static_cast<unsigned long long>(address - base));
}

// A bounded walk of the captured fault CONTEXT, not a scan of arbitrary stack
// words or a trace of this exception filter. All memory reads are protected by
// the caller's nested SEH boundary in case the fault corrupted the stack.
void WriteStack(Writer writer, const CONTEXT* captured) {
#if defined(_M_X64)
    CONTEXT context = *captured;
    for (unsigned frame = 0; frame < 24 && context.Rip != 0; ++frame) {
        WriteAddress(writer, "frame", frame, static_cast<std::uintptr_t>(context.Rip));
        const DWORD64 oldRsp = context.Rsp;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
        if (function != nullptr) {
            void* handlerData = nullptr;
            DWORD64 establisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context,
                             &handlerData, &establisherFrame, nullptr);
        } else {
            // A leaf has no unwind entry. Its return address is at the captured
            // stack pointer; a bad pointer is handled by the enclosing SEH.
            context.Rip = *reinterpret_cast<const DWORD64*>(context.Rsp);
            context.Rsp += sizeof(DWORD64);
        }
        if (context.Rsp <= oldRsp || context.Rsp - oldRsp > 16 * 1024 * 1024) {
            writer("crash.stack.stop=invalid-progress");
            return;
        }
    }
#else
    (void)captured;
    writer("crash.stack.unavailable=unsupported-architecture");
#endif
}

void WriteDetails(const EXCEPTION_POINTERS* exception, Writer writer) {
    if (exception == nullptr || exception->ExceptionRecord == nullptr) {
        writer("crash.detail.unavailable=no-exception-record");
        return;
    }
    const EXCEPTION_RECORD* record = exception->ExceptionRecord;
    if (record->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
        writer("crash.detail.unavailable=stack-overflow");
        return;
    }
    WriteAddress(writer, "fault", 0, reinterpret_cast<std::uintptr_t>(record->ExceptionAddress));
    Write(writer, "crash.parameters=%lu", record->NumberParameters);
    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && record->NumberParameters >= 2) {
        const ULONG_PTR operation = record->ExceptionInformation[0];
        const char* name = operation == 0 ? "read" : operation == 1 ? "write" :
                           operation == 8 ? "execute" : "unknown";
        Write(writer, "crash.access=%s operation=%llu target=%p", name,
              static_cast<unsigned long long>(operation),
              reinterpret_cast<const void*>(record->ExceptionInformation[1]));
    }
    if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && record->NumberParameters >= 3) {
        Write(writer, "crash.in-page-status=0x%llX",
              static_cast<unsigned long long>(record->ExceptionInformation[2]));
    }
    const CONTEXT* context = exception->ContextRecord;
    if (context == nullptr) {
        writer("crash.context.unavailable");
        return;
    }
#if defined(_M_X64)
    Write(writer, "crash.registers rip=%016llX rsp=%016llX rbp=%016llX",
          context->Rip, context->Rsp, context->Rbp);
    Write(writer, "crash.registers rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX",
          context->Rax, context->Rbx, context->Rcx, context->Rdx);
    Write(writer, "crash.registers rsi=%016llX rdi=%016llX r8=%016llX r9=%016llX",
          context->Rsi, context->Rdi, context->R8, context->R9);
    Write(writer, "crash.registers r10=%016llX r11=%016llX r12=%016llX r13=%016llX r14=%016llX r15=%016llX",
          context->R10, context->R11, context->R12, context->R13, context->R14, context->R15);
#endif
    WriteStack(writer, context);
}

} // namespace

void Zelda3DUwp_WriteExceptionReport(const EXCEPTION_POINTERS* exception, Writer writeLine) {
    if (writeLine == nullptr) return;
    __try {
        WriteDetails(exception, writeLine);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        writeLine("crash.detail.stopped=unreadable-context-or-stack");
    }
}
