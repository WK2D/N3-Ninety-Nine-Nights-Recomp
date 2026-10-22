#include "graphics_menu.h"
#include "graphics_menu_assets.h"
#include "frame_rate.h"
#include "display_settings.h"
#include "profile_config.h"
#include "graphics_notification.h"
#include "graphics_settings.h"
#include "graphics_filtering.h"
#include <rex/runtime.h>
#include <rex/ppc.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <fstream>
#include <array>
#include <iterator>
#include <stdexcept>
#include <cstring>
#include <algorithm>

extern "C" void sub_821CD7C0(PPCContext&, uint8_t*);
extern "C" void sub_821DC2C0(PPCContext&, uint8_t*);
extern "C" void sub_821DCCB0(PPCContext&, uint8_t*);
extern "C" void sub_821DC710(PPCContext&, uint8_t*);
extern "C" void sub_821CB2C8(PPCContext&, uint8_t*);
REXCVAR_DECLARE(bool, frame_pacing_trace);

namespace {
bool installed = false, pending = false;
bool focus_pending = false;
uint32_t layout_name = 0, graphics_owner = 0, graphics_root = 0;
std::filesystem::path config_path;
constexpr std::array<uint32_t, 4> display_heights{720, 1080, 1440, 2160};
uint32_t Read32(uint8_t* base, uint32_t at) { return rex::memory::load_and_swap<uint32_t>(base + at); }
void Write32(uint8_t* base, uint32_t at, uint32_t n) { rex::memory::store_and_swap<uint32_t>(base + at, n); }
std::vector<uint8_t> ReadFile(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  if (!in || std::filesystem::file_size(file) > 2 * 1024 * 1024) throw std::runtime_error("Cannot read menu data");
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void WriteFile(const std::filesystem::path& file, const std::vector<uint8_t>& bytes) {
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  if (!out) throw std::runtime_error("Cannot write menu cache");
}
using GuestFunc = void (*)(PPCContext&, uint8_t*);
uint32_t Call(PPCContext& ctx, uint8_t* base, GuestFunc fn, uint32_t a, uint32_t b = 0, uint32_t c = 0) {
  const auto saved = ctx; ctx.r3.u64 = a; ctx.r4.u64 = b; ctx.r5.u64 = c;
  fn(ctx, base); const auto result = ctx.r3.u32; ctx = saved; return result;
}
void ConfigureSlider(PPCContext& ctx, uint8_t* base, uint32_t slider, uint32_t first_text,
                     uint32_t count, uint32_t value) {
  Call(ctx, base, sub_821DC2C0, slider);
  for (uint32_t i = 0; i < count; ++i) Call(ctx, base, sub_821DCCB0, slider, first_text + i, i);
  Write32(base, slider + 192, 0); Write32(base, slider + 196, count - 1);
  Write32(base, slider + 200, count - 1); Write32(base, slider + 204, value);
}
void Persist() {
  try { SaveProfileConfig(config_path); NotifyGraphicsSaved(true); }
  catch (...) { NotifyGraphicsSaved(false); REXLOG_WARN("Graphics settings could not be saved"); }
}
}

bool InstallGraphicsMenu(rex::Runtime& runtime, const std::filesystem::path& root,
                         const std::filesystem::path& profile) {
  installed = false; pending = false; focus_pending = false; graphics_owner = graphics_root = 0;
  ConfiguredAnisotropicChoice();
  auto* vfs = runtime.file_system();
  std::vector<std::string> links;
  bool mounted = false;
  constexpr auto mount = "\\Device\\N3GraphicsMenu";
  try {
    const auto cache = profile / "ui" / "graphics-menu-v2";
    for (const auto* folder : {"Menu", "Text"}) {
      for (const auto& e : std::filesystem::recursive_directory_iterator(root / "data" / folder)) {
        if (!e.is_regular_file()) continue;
        const auto rel = std::filesystem::relative(e.path(), root / "data");
        const auto dest = cache / rel; std::filesystem::create_directories(dest.parent_path());
        std::filesystem::copy_file(e.path(), dest, std::filesystem::copy_options::overwrite_existing);
      }
    }
    WriteFile(cache / "Menu" / "uititle.uitb", n3::graphics::ExtendTitleLayout(ReadFile(cache / "Menu" / "uititle.uitb")));
    WriteFile(cache / "Menu" / "uigraphics.uitb", n3::graphics::BuildGraphicsLayout(ReadFile(cache / "Menu" / "uioption.uitb")));
    constexpr auto extra =
      "\r\n1950\t\"VSync\"\r\n1951\t\"Off\"\r\n1952\t\"On\""
      "\r\n1960\t\"720p\"\r\n1961\t\"1080p\"\r\n1962\t\"1440p\"\r\n1963\t\"4K\""
      "\r\n1965\t\"720p (1x)\"\r\n1966\t\"1440p (2x)\"\r\n1967\t\"4K (3x)\""
      "\r\n1975\t\"Display\"\r\n1976\t\"Rendering\""
      "\r\n1977\t\"@(DPad_Small) Select  @(B) Back\""
      "\r\n1978\t\"Render resolution\"\r\n1979\t\"Display resolution\""
      "\r\n1980\t\"1x\"\r\n1981\t\"2x\"\r\n1982\t\"4x\"\r\n1983\t\"8x\"\r\n1984\t\"16x\""
      "\r\n1989\t\"Filtering\"\r\n1990\t\"30 FPS\"\r\n1991\t\"40 FPS\"\r\n1992\t\"60 FPS\""
      "\r\n1997\t\"Frame rate\"\r\n1998\t\"Anisotropic filtering\"\r\n1999\t\"Graphics\"\r\n";
    for (const auto* lang : {"ENG", "FRA", "GER", "ITA", "SPA"}) {
      std::string lower = lang; for (auto& c : lower) c = char(c + ('a' - 'A'));
      const auto file = cache / "Text" / lang / ("defaulttexttable0000_" + lower + ".txt");
      auto data = ReadFile(file); data.insert(data.end(), extra, extra + std::strlen(extra)); WriteFile(file, data);
    }
    std::string source;
    if (!vfs->FindSymbolicLink("game:", source)) throw std::runtime_error("Game mount unavailable");
    auto device = std::make_unique<rex::filesystem::HostPathDevice>(mount, cache, true);
    if (!device->Initialize() || !vfs->RegisterDevice(std::move(device))) throw std::runtime_error("Menu mount failed");
    mounted = true;
    for (const auto* folder : {"Menu", "Text"}) {
      const auto link = source + "\\data\\" + folder;
      std::string existing;
      if (vfs->FindSymbolicLink(link, existing) || !vfs->RegisterSymbolicLink(link, std::string(mount) + "\\" + folder))
        throw std::runtime_error("Menu link failed");
      links.push_back(link);
    }
    layout_name = runtime.memory()->SystemHeapAlloc(32);
    if (!layout_name) throw std::runtime_error("Menu name allocation failed");
    std::memcpy(runtime.virtual_membase() + layout_name, "UiGraphics.uit", sizeof("UiGraphics.uit"));
    config_path = profile / "n3.toml";
    installed = true; REXLOG_INFO("Main-menu Graphics page installed"); return true;
  } catch (...) {
    for (const auto& link : links) vfs->UnregisterSymbolicLink(link);
    if (mounted) vfs->UnregisterDevice(mount);
    REXLOG_WARN("Graphics page could not be installed; original menus retained"); return false;
  }
}

extern "C" void __imp__sub_8218CDC8(PPCContext& ctx, uint8_t* base);
extern "C" void sub_8218CDC8(PPCContext& ctx, uint8_t* base) {
  const auto message = ctx.r4.u32, sender = Read32(base, message);
  if (installed && sender && Read32(base, sender) == n3::graphics::kButtonId &&
      Read32(base, message + 4) == 1100 && Read32(base, message + 12) == 0) {
    pending = true;
    // Follow the game's own Options transition with a dedicated layout.
    Write32(base, sender, 0x02901004);
    __imp__sub_8218CDC8(ctx, base);
    Write32(base, sender, n3::graphics::kButtonId);
    return;
  }
  __imp__sub_8218CDC8(ctx, base);
}

extern "C" void __imp__sub_82272470(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82272470(PPCContext& ctx, uint8_t* base) {
  if (installed && pending && std::strcmp(reinterpret_cast<const char*>(base + ctx.r4.u32), "UiOption.uit") == 0) {
    ctx.r4.u64 = layout_name;
  }
  __imp__sub_82272470(ctx, base);
}

extern "C" void __imp__sub_821A8030(PPCContext& ctx, uint8_t* base);
extern "C" void sub_821A8030(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  // The native layout cache can skip the loader on later visits. The title
  // transition identifies Graphics even when no fresh layout load occurs.
  const bool graphics = installed && pending;
  __imp__sub_821A8030(ctx, base);
  if (!graphics) { graphics_owner = graphics_root = 0; return; }
  graphics_owner = owner; graphics_root = Read32(base, owner + 56);
  focus_pending = true;
  pending = false;
  const auto fps = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906001);
  const auto aniso = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906002);
  const auto af_index = ConfiguredAnisotropicChoice();
  const auto selected = ConfiguredFrameRate();
  ConfigureSlider(ctx, base, fps, 1990, 3, selected == 60 ? 2 : selected == 40 ? 1 : 0);
  ConfigureSlider(ctx, base, aniso, 1980, n3::graphics::kAnisotropicOverrides.size(), af_index);
  const auto display = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906003);
  const auto render = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906004);
  uint32_t display_index = 0;
  for (uint32_t i = 0; i < display_heights.size(); ++i)
    if (display_heights[i] == ConfiguredDisplayHeight()) display_index = i;
  int shared_scale = 1, x = 1, y = 1;
  try {
    shared_scale = std::stoi(rex::cvar::GetFlagByName("resolution_scale"));
    x = rex::cvar::HasNonDefaultValue("draw_resolution_scale_x") ? std::stoi(rex::cvar::GetFlagByName("draw_resolution_scale_x")) : shared_scale;
    y = rex::cvar::HasNonDefaultValue("draw_resolution_scale_y") ? std::stoi(rex::cvar::GetFlagByName("draw_resolution_scale_y")) : shared_scale;
  } catch (...) {}
  ConfigureSlider(ctx, base, display, 1960, 4, display_index);
  ConfigureSlider(ctx, base, render, 1965, 3, uint32_t(std::clamp(std::max(x, y), 1, 3) - 1));
  const auto sync = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906005);
  ConfigureSlider(ctx, base, sync, 1951, 2, rex::cvar::Query<bool>("n3_vsync") ? 1 : 0);
  REXLOG_INFO("Graphics page opened");
}

extern "C" void __imp__sub_821A8370(PPCContext& ctx, uint8_t* base);
extern "C" void sub_821A8370(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_821A8370(ctx, base);
  if (owner == graphics_owner && rex::cvar::GetFlagByName("n3_live_resolution_failed") == "true") {
    rex::cvar::SetFlagByName("n3_live_resolution_failed", "false");
    const auto render = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03906004);
    const auto active = std::clamp(rex::cvar::Query<uint32_t>("n3_render_scale_active"), 1u, 3u);
    ConfigureSlider(ctx, base, render, 1965, 3, active - 1);
    Persist(); NotifyGraphicsSaved(false);
  }
  if (owner == graphics_owner && focus_pending && Read32(base, owner + 16) == 2) {
    const auto button = Call(ctx, base, sub_821CD7C0, graphics_root, 0x03901001);
    Call(ctx, base, sub_821CB2C8, button);
    focus_pending = false;
    REXLOG_INFO("Graphics page focused");
  }
}

extern "C" void __imp__sub_821A8B30(PPCContext& ctx, uint8_t* base);
extern "C" void sub_821A8B30(PPCContext& ctx, uint8_t* base) {
  const auto root = ctx.r3.u32, message = ctx.r4.u32;
  const auto kind = Read32(base, message + 4), code = Read32(base, message + 12);
  if (root == graphics_root && graphics_root) {
    const auto sender = Read32(base, message);
    const auto id = sender ? Read32(base, sender) : 0;
    if (REXCVAR_GET(frame_pacing_trace) && (kind == 1100 || kind == 400))
      REXLOG_INFO("Graphics input: type {} code {} control {:08X}", kind, code, id);
    if (kind == 1100 && code == 1) {
      Persist(); graphics_owner = graphics_root = 0;
    } else if ((kind == 1100 && (code == 8 || code == 9 || code == 0)) || kind == 400) {
      const auto type = id & 0xFFFFF000u;
      const auto ordinal = id & 0xFFFu;
      if ((type == 0x03901000 || type == 0x03906000) && ordinal >= 1 && ordinal <= 5) {
        const auto slider = Call(ctx, base, sub_821CD7C0, root, 0x03906000 + ordinal);
        if (kind == 1100) Call(ctx, base, sub_821DC710, slider, code == 8 ? 0 : 1);
        const auto index = Read32(base, slider + 192) + Read32(base, slider + 204);
        if (!ApplyGraphicsChoice(ordinal, index)) { NotifyGraphicsSaved(false); ctx.r3.u64 = 0; return; }
        Persist(); REXLOG_INFO("Graphics setting applied: row {} index {}", ordinal, index);
        ctx.r3.u64 = 0; return;
      }
    }
  }
  __imp__sub_821A8B30(ctx, base);
}
