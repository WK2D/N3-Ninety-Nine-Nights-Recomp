#include "graphics_notification.h"
#include <imgui.h>
#include <atomic>
#include <chrono>

namespace {
std::atomic<int64_t> until{0};
std::atomic<bool> saved{true};
int64_t Now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
class GraphicsNotification final : public rex::ui::ImGuiDialog {
 public:
  explicit GraphicsNotification(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}
 private:
  void OnDraw(ImGuiIO& io) override {
    if (Now() >= until.load()) return;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.85f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.01f, 0.16f, 0.14f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.1f, 0.85f, 0.8f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 12));
    constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##GraphicsSaved", nullptr, flags))
      ImGui::TextUnformatted(saved.load() ? "Graphics settings saved" : "Graphics settings could not be applied or saved");
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
  }
};
}
std::unique_ptr<rex::ui::ImGuiDialog> CreateGraphicsNotification(rex::ui::ImGuiDrawer* drawer) {
  return std::make_unique<GraphicsNotification>(drawer);
}
void NotifyGraphicsSaved(bool success) { saved.store(success); until.store(Now() + 2500); }
