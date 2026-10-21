#pragma once
#include <cstdint>
namespace rex::ui { class Window; class WindowedAppContext; }
void InstallDisplaySettings(rex::ui::WindowedAppContext& context, rex::ui::Window& window);
void ShutdownDisplaySettings();
uint32_t ConfiguredDisplayHeight();
bool SetDisplayHeight(uint32_t height);
void RecordDisplayOutputSize(uint32_t width, uint32_t height);
