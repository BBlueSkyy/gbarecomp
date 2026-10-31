#pragma once
#include "gba_texture_replacement.h"
#include <memory>
#include <string>
namespace gbarecomp {
std::unique_ptr<gba::TextureReplacementPack> load_hd_texture_pack(const std::string& directory,const std::string& rom_sha1,std::string& report);
bool write_hd_rgb_png(const char* path,const uint8_t* rgb,unsigned width,unsigned height);
bool write_hd_tile_catalog(const gba::TextureReplacementPack& pack,const std::string& directory);
}
