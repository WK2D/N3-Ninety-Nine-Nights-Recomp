#pragma once
#include <cstdint>
void InitializeFrameRate();
uint32_t ConfiguredFrameRate();
bool SetFrameRate(uint32_t fps);
void ApplyFrameRateAtBoundary(uint8_t* base);
