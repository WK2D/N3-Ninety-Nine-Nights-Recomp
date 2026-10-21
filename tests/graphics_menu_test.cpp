#include "graphics_menu_assets.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;
void Require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
void U32(Bytes& b, uint32_t n) { for (int i = 24; i >= 0; i -= 8) b.push_back(uint8_t(n >> i)); }
Bytes Token(uint32_t type, const std::string& value) {
  Bytes b; U32(b, type); U32(b, uint32_t(value.size() + 1));
  b.insert(b.end(), value.begin(), value.end()); b.push_back(0); U32(b, 1); U32(b, 0); return b;
}
Bytes Number(uint32_t value) { Bytes b; U32(b, 5); U32(b, value); U32(b, 1); U32(b, 0); return b; }
Bytes Fixture() {
  std::vector<Bytes> ts{Token(3,"LAYER"),Token(2,"("),Token(4,"SN_Title2"),Token(2,")"),Token(2,"{"),
    Token(3,"ATTACH"),Token(2,"{"),Token(2,"}"),Token(3,"BUTTON"),Token(2,"("),Token(4,"BT_Tutorial"),
    Token(2,")"),Token(2,"{"),Token(3,"CLASSID"),Token(2,"="),Number(0x02901005),
    Token(3,"TEXTID"),Token(2,"="),Number(103),Token(2,"}"),
    Token(3,"IMAGE"),Token(2,"("),Token(4,"IM_OptionContrast_center"),Token(2,")"),Token(2,"{"),
    Token(3,"CLASSID"),Token(2,"="),Number(0x03805009),Token(2,"}"),
    Token(3,"IMAGE"),Token(2,"("),Token(4,"IM_OptionContrast_right"),Token(2,")"),Token(2,"{"),
    Token(3,"CLASSID"),Token(2,"="),Number(0x0380500A),Token(2,"}"),Token(2,"}")};
  Bytes b; U32(b,uint32_t(ts.size())); for (const auto& t:ts) b.insert(b.end(),t.begin(),t.end());
  b.insert(b.end(), {'m','e','t','a',0}); return b;
}
size_t Count(const Bytes& b, const std::string& s) {
  size_t n=0; for(size_t i=0;i+s.size()<=b.size();++i) if(!std::memcmp(b.data()+i,s.data(),s.size())) ++n; return n;
}
void Reject(const Bytes& b) {
  bool rejected=false; try { n3::graphics::ExtendTitleLayout(b); } catch(const std::exception&) { rejected=true; }
  Require(rejected,"Malformed or duplicate layout was accepted");
}
int main(int argc, char** argv) {
  try {
    auto b=Fixture(); const auto original=b;
    const auto extended=n3::graphics::ExtendTitleLayout(b);
    Require(b==original,"Original menu bytes were modified");
    Require(Count(extended,"BT_PCGraphics")==1 && Count(extended,"BT_Tutorial")==1,"New button missing or original button lost");
    Require(std::equal(b.end()-5,b.end(),extended.end()-5),"Trailing metadata lost");
    Require(extended.size()>b.size(),"Menu was not extended"); Reject(extended);
    for(size_t size=0;size<b.size()-5;++size) Reject(Bytes(b.begin(),b.begin()+size));
    auto bad=b;bad[4]=0xff;Reject(bad);
    bad=b;bad[8]=0xff;Reject(bad);
    bad=b;bad[3]=0xff;Reject(bad);
    if(argc>=2) {
      std::ifstream in(argv[1],std::ios::binary);
      Bytes actual{std::istreambuf_iterator<char>(in),{}};
      auto patched=n3::graphics::ExtendTitleLayout(actual);
      Require(Count(patched,"BT_PCGraphics")==1,"Supported game menu did not gain a Graphics entry");
      Reject(patched);
      std::cout<<"Supported local menu transformation passed\n";
    }
    if(argc==3) {
      std::ifstream in(argv[2],std::ios::binary);
      Bytes actual{std::istreambuf_iterator<char>(in),{}};
      const auto copy=actual;
      auto page=n3::graphics::BuildGraphicsLayout(actual);
      Require(actual==copy && Count(page,"SD_OptionBGM")==1 && Count(page,"SD_OptionSE")==1,"Graphics layout lost required controller nodes");
      std::cout<<"Supported local Graphics page transformation passed\n";
    }
    std::cout<<"Menu extension, source preservation, metadata, truncation and duplicate rejection passed\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
