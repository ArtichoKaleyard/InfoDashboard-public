#pragma once

#include <cstddef>

void UserApp_AppInit(void);
void UserApp_UiInit(void);
void UserApp_TaskInit(void);
size_t UserApp_WriteDiagnosticsJson(char *dest, size_t dest_size);
size_t UserApp_WriteLogsText(char *dest, size_t dest_size);
