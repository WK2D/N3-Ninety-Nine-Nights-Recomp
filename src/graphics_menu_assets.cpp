#include "graphics_menu_assets.h"
#include <string>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <set>

namespace n3::graphics {
namespace {
struct Token { uint32_t type, number = 0; std::string text; std::vector<uint8_t> raw; };
uint32_t Read(std::span<const uint8_t> b, size_t& p) {
  if (p > b.size() || b.size() - p < 4) throw std::runtime_error("Truncated menu layout");
  uint32_t n = 0; for (int i = 0; i < 4; ++i) n = (n << 8) | b[p++]; return n;
}
void Write(std::vector<uint8_t>& b, uint32_t n) {
  for (int i = 24; i >= 0; i -= 8) b.push_back(uint8_t(n >> i));
}
Token String(uint32_t type, std::string text) {
  Token t{type, 0, std::move(text), {}};
  Write(t.raw, type); Write(t.raw, uint32_t(t.text.size() + 1));
  t.raw.insert(t.raw.end(), t.text.begin(), t.text.end()); t.raw.push_back(0);
  Write(t.raw, 1); Write(t.raw, 0); return t;
}
Token Number(uint32_t n) {
  Token t{5, n, {}, {}}; Write(t.raw, 5); Write(t.raw, n); Write(t.raw, 1); Write(t.raw, 0); return t;
}
size_t EndBrace(const std::vector<Token>& ts, size_t start) {
  int depth = 0;
  for (size_t i = start; i < ts.size(); ++i) {
    if (ts[i].text == "{") ++depth;
    if (ts[i].text == "}" && --depth == 0) return i;
  }
  throw std::runtime_error("Unbalanced menu layout");
}
struct Layout {
  std::vector<Token> ts;
  std::vector<uint8_t> tail;
  explicit Layout(std::span<const uint8_t> b) {
    if (b.size() > 2 * 1024 * 1024) throw std::runtime_error("Menu layout exceeds limit");
    size_t p = 0; const auto count = Read(b, p);
    if (count > 100000) throw std::runtime_error("Invalid menu token count");
    ts.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      const auto begin = p; Token t{Read(b, p), 0, {}, {}};
      if (t.type >= 2 && t.type <= 4) {
        const auto len = Read(b, p);
        if (!len || len > 4096 || p > b.size() || len > b.size() - p || b[p + len - 1])
          throw std::runtime_error("Invalid menu string");
        t.text.assign(reinterpret_cast<const char*>(b.data() + p), len - 1); p += len;
      } else if (t.type == 5 || t.type == 6) t.number = Read(b, p);
      else throw std::runtime_error("Unknown menu token");
      Read(b, p); Read(b, p);
      t.raw.assign(b.begin() + begin, b.begin() + p); ts.push_back(std::move(t));
    }
    tail.assign(b.begin() + p, b.end());
  }
  size_t Find(const std::string& kind, const std::string& name) const {
    for (size_t i = 0; i + 2 < ts.size(); ++i)
      if (ts[i].text == kind && ts[i + 2].text == name) return i;
    throw std::runtime_error("Unsupported menu structure");
  }
  std::vector<uint8_t> Encode() const {
    std::vector<uint8_t> out; Write(out, uint32_t(ts.size()));
    for (const auto& t : ts) out.insert(out.end(), t.raw.begin(), t.raw.end());
    out.insert(out.end(), tail.begin(), tail.end()); return out;
  }
};
void Set(std::vector<Token>& block, const std::string& field, uint32_t value) {
  for (size_t i = 0; i + 2 < block.size(); ++i)
    if (block[i].text == field && block[i + 1].text == "=") { block[i + 2] = Number(value); return; }
  block.insert(block.end() - 1, {String(3, field), String(2, "="), Number(value)});
}
std::vector<Token> Attach(uint32_t id, uint32_t x, uint32_t y) {
  return {String(3, "ADDITEM"), String(2, "="), String(3, "CLASSID"), String(2, "("), Number(id),
    String(2, ")"), String(2, ","), String(3, "POS"), String(2, "("), Number(x), String(2, ","), Number(y), String(2, ")")};
}
}

std::vector<uint8_t> ExtendTitleLayout(std::span<const uint8_t> b) {
  Layout l(b);
  for (const auto& t : l.ts) if (t.type == 5 && t.number == kButtonId) throw std::runtime_error("Menu already extended");
  const auto layer = l.Find("LAYER", "SN_Title2"), button = l.Find("BUTTON", "BT_Tutorial");
  const auto layer_end = EndBrace(l.ts, layer), button_end = EndBrace(l.ts, button);
  size_t attach = layer; while (attach < layer_end && l.ts[attach].text != "ATTACH") ++attach;
  if (attach == layer_end || button >= layer_end) throw std::runtime_error("Unsupported title menu");
  const auto attach_end = EndBrace(l.ts, attach);
  std::vector<Token> clone(l.ts.begin() + button, l.ts.begin() + button_end + 1);
  clone[2] = String(4, "BT_PCGraphics"); Set(clone, "CLASSID", kButtonId); Set(clone, "TEXTID", kTextId);
  const auto entry = Attach(kButtonId, 390, 585);
  l.ts.insert(l.ts.begin() + layer_end, clone.begin(), clone.end());
  l.ts.insert(l.ts.begin() + attach_end, entry.begin(), entry.end());
  return l.Encode();
}

std::vector<uint8_t> BuildGraphicsLayout(std::span<const uint8_t> b) {
  Layout l(b);
  l.Find("LAYER", "SN_Option2"); l.Find("BUTTON", "BT_OptionContrast");
  const auto slider = l.Find("SLIDER", "SD_OptionCamY");
  const auto slider_end = EndBrace(l.ts, slider);
  const std::vector<Token> slider_template(l.ts.begin() + slider, l.ts.begin() + slider_end + 1);
  const std::set<std::string> visible{
    "IM_OptionBGM", "IM_OptionSE", "BT_OptionBGM", "BT_OptionSE", "SD_OptionBGM", "SD_OptionSE",
    "IM_OptionVol_center", "IM_OptionVol_right", "TT_OptionVol", "IM_OptionCam_center", "IM_OptionCam_right", "TT_OptionCam",
    "IM_Option_BG", "IM_Option_Underline", "TT_Option", "TT_OptionHelp", "IM_Option_Underline_down"};
  std::vector<Token> result;
  for (size_t i = 0; i < l.ts.size();) {
    const auto& kind = l.ts[i].text;
    if (kind != "BUTTON" && kind != "SLIDER" && kind != "IMAGE" && kind != "TEXT") { result.push_back(l.ts[i++]); continue; }
    if (i + 2 >= l.ts.size()) throw std::runtime_error("Invalid menu node");
    const auto name = l.ts[i + 2].text;
    const auto end = EndBrace(l.ts, i);
    std::vector<Token> block(l.ts.begin() + i, l.ts.begin() + end + 1);
    if (name == "SD_OptionBGM" || name == "SD_OptionSE") {
      const bool fps = name == "SD_OptionBGM";
      block = slider_template; block[2] = String(4, name);
      Set(block, "CLASSID", fps ? 0x03906001 : 0x03906002);
      for (size_t j = 0; j + 4 < block.size(); ++j)
        if (block[j].text == "RANGE") block[j + 4] = Number(fps ? 2 : 5);
    }
    if (!visible.contains(name)) Set(block, "ATTRIBUTE", 16);
    if (name == "BT_OptionBGM" || name == "TT_OptionVol") Set(block, "TEXTID", 1997);
    if (name == "BT_OptionSE") Set(block, "TEXTID", 1998);
    if (name == "TT_OptionCam") Set(block, "TEXTID", 1989);
    if (name == "TT_Option") { Set(block, "ATTRIBUTE", 19); Set(block, "TEXTID", 1999); }
    if (name == "TT_OptionVol" || name == "TT_OptionCam" || name == "BT_OptionBGM" || name == "BT_OptionSE" ||
        name == "IM_OptionBGM" || name == "IM_OptionSE") {
      for (size_t j = 0; j + 2 < block.size(); ++j)
        if (block[j].text == "SIZE") block[j + 2] = Number(name.starts_with("BT_") ? 500 : name.starts_with("IM_") ? 800 : 300);
    }
    result.insert(result.end(), block.begin(), block.end()); i = end + 1;
  }
  l.ts = std::move(result);
  // Move the second row alongside the second sidebar banner.
  const std::map<uint32_t, uint32_t> ys{{0x03905002, 290}, {0x03901002, 294}, {0x03906002, 294}};
  for (size_t i = 0; i + 12 < l.ts.size(); ++i)
    if (l.ts[i].text == "ADDITEM" && l.ts[i + 4].type == 5) {
      const auto found = ys.find(l.ts[i + 4].number);
      if (found != ys.end()) l.ts[i + 11] = Number(found->second);
      if (l.ts[i + 4].number == 0x03906001 || l.ts[i + 4].number == 0x03906002)
        l.ts[i + 9] = Number(940);
      if (l.ts[i + 4].number == 0x00603001) {
        l.ts[i + 9] = Number(500); l.ts[i + 11] = Number(32);
      }
    }
  return l.Encode();
}
}
