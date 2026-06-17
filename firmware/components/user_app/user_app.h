#pragma once

#include <cstddef>
#include <cstdint>

void UserApp_AppInit(void);
void UserApp_UiInit(void);
void UserApp_TaskInit(void);
void UserApp_MarkOtaAppValid(void);
void UserApp_RecordDiagnosticLog(const char *source, const char *event, const char *detail);
void UserApp_UpdateOtaProgress(const char *phase, const char *message, uint32_t bytes,
                               uint32_t total, bool active);
size_t UserApp_WriteDiagnosticsJson(char *dest, size_t dest_size);
size_t UserApp_WriteLogsText(char *dest, size_t dest_size);
size_t UserApp_ReadLogEntriesAfter(uint32_t after_sequence, char *dest, size_t dest_size,
                                   uint32_t *last_sequence);
