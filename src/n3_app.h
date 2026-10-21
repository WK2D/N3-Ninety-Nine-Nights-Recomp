// n3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include "crash_diagnostics.h"
#include "runtime_fixes.h"
#include "game_setup.h"
#include "frame_rate.h"
#include <rex/ui/flags.h>

class N3App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<N3App>(new N3App(ctx, "n3",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) config.gpu_plugin = "xenos";
    if (rex::cvar::GetFlagSource("fullscreen") == rex::cvar::Source::kDefault)
      REXCVAR_SET(fullscreen, false);
    if (rex::cvar::GetFlagSource("window_width") == rex::cvar::Source::kDefault)
      REXCVAR_SET(window_width, 1280);
    if (rex::cvar::GetFlagSource("window_height") == rex::cvar::Source::kDefault)
      REXCVAR_SET(window_height, 720);
  }

  void OnPostInitLogging() override { InstallCrashDiagnostics(game_profile_ / "logs"); }

  void OnPostLoadXexImage() override {
    InstallRuntimeFixes(*runtime());
    InitializeFrameRate();
  }

  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)>) override {
    auto paths = ResolveGameData(defaults);
    if (!paths && window()) window()->RequestClose();
    return paths;
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    ConfigureGamePaths(paths);
    paths.config_path = paths.user_data_root / "n3.toml";
  }

  void OnConfigureLogging(rex::LogConfig& config) override {
    config.log_dir = game_profile_ / "logs";
  }

 private:
  std::filesystem::path game_profile_;

  void ConfigureGamePaths(rex::PathConfig& paths) {
    ConfigureGameDataPaths(paths);
    game_profile_ = paths.user_data_root;
  }

  // Override virtual hooks for customization:
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
};
