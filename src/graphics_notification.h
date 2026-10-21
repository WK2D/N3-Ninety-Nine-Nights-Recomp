#pragma once
#include <memory>
#include <rex/ui/imgui_dialog.h>

std::unique_ptr<rex::ui::ImGuiDialog> CreateGraphicsNotification(rex::ui::ImGuiDrawer* drawer);
void NotifyGraphicsSaved(bool success);
