#include "hd_texture_loader.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 const char* fixture=argc>1?argv[1]:HD_FIXTURES;
 std::string report;auto p=gbarecomp::load_hd_texture_pack(fixture,"fixture",report);
 assert(p&&p->size()==2);assert(p->bytes()==2*24*24*4);
 assert(!gbarecomp::load_hd_texture_pack(fixture,"wrong-rom",report));
 assert(!gbarecomp::load_hd_texture_pack("/missing-pack","fixture",report));
 std::cout<<"PNG/WebP load, ROM mismatch, missing pack passed\n";
}
