#pragma once

#include <Windows.h>

// Inspect the captured fault context without desktop DbgHelp or a symbol server.
// Each completed line is handed to the caller's durable logger. Reporting faults
// are contained and never replace the original exception record.
void Zelda3DUwp_WriteExceptionReport(const EXCEPTION_POINTERS* exception,
                                   void (*writeLine)(const char*));
