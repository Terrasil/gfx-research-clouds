#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <glad/gl.h>
// PNG writer with stored DEFLATE blocks, no extra image library required.
namespace cloud_png {
using Bytes=std::vector<std::uint8_t>;
inline void be(Bytes& b,std::uint32_t x){for(int s=24;s>=0;s-=8)b.push_back(std::uint8_t(x>>s));}
inline std::uint32_t crc(const Bytes& b,size_t start){std::uint32_t c=0xffffffffu;for(size_t i=start;i<b.size();++i){c^=b[i];for(int j=0;j<8;++j)c=(c>>1)^(0xedb88320u&std::uint32_t(-int(c&1)));}return c^0xffffffffu;}
inline void chunk(Bytes& png,const char* type,const Bytes& data){be(png,std::uint32_t(data.size()));size_t start=png.size();for(int i=0;i<4;++i)png.push_back(std::uint8_t(type[i]));png.insert(png.end(),data.begin(),data.end());be(png,crc(png,start));}
inline void save(const std::filesystem::path& path,int w,int h){
 Bytes rgb(size_t(w)*h*3);GLint align=4;glGetIntegerv(GL_PACK_ALIGNMENT,&align);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());glPixelStorei(GL_PACK_ALIGNMENT,align);
 Bytes scan;scan.reserve((size_t(w)*3+1)*h);for(int y=h-1;y>=0;--y){scan.push_back(0);auto first=rgb.begin()+size_t(y)*w*3;scan.insert(scan.end(),first,first+w*3);}
 Bytes def={0x78,0x01};for(size_t i=0;i<scan.size();){size_t len=std::min<size_t>(65535,scan.size()-i);def.push_back(i+len==scan.size()?1:0);std::uint16_t n=std::uint16_t(len),inv=std::uint16_t(~n);def.push_back(n&255);def.push_back(n>>8);def.push_back(inv&255);def.push_back(inv>>8);def.insert(def.end(),scan.begin()+i,scan.begin()+i+len);i+=len;}
 std::uint32_t a=1,b=0;for(auto x:scan){a=(a+x)%65521u;b=(b+a)%65521u;}be(def,(b<<16)|a);
 Bytes png={137,80,78,71,13,10,26,10},ihdr;be(ihdr,w);be(ihdr,h);ihdr.insert(ihdr.end(),{8,2,0,0,0});chunk(png,"IHDR",ihdr);chunk(png,"IDAT",def);chunk(png,"IEND",{});
 std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size()));if(!f)throw std::runtime_error("PNG write failed: "+path.string());
}
}
