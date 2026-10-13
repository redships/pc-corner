#pragma once
#include <windows.h>

typedef struct {
    DWORDLONG totalPhys;
    DWORDLONG availPhys;
    DWORDLONG totalPageFile;
    DWORDLONG availPageFile;
    DWORDLONG totalVirtual;
    DWORDLONG availVirtual;
} RAM_INFO;

typedef struct {
    unsigned long long vramTotalBytes;
    unsigned long long vramUsedBytes;
    int available;
} VRAM_INFO;

typedef struct {
    int cpuTempC;
    int gpuTempC;
} TEMP_INFO;

void query_ram(RAM_INFO *out);
void query_vram(VRAM_INFO *out);
void query_temps(TEMP_INFO *out);
