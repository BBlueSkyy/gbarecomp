#include "hd_texture_loader.h"
#include "mini_json.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../third_party/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#if defined(__ANDROID__) && defined(GBARECOMP_HAVE_SDL2)
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif
namespace gbarecomp {
namespace {
bool decode(const std::filesystem::path& path,gba::HdTile& out) {
    std::error_code ec;auto n=std::filesystem::file_size(path,ec);
    if(ec||!n||n>2*1024*1024)return false;
    std::vector<uint8_t> data(static_cast<size_t>(n));
    std::ifstream file(path,std::ios::binary);if(!file.read(reinterpret_cast<char*>(data.data()),n))return false;
    int w=0,h=0,c=0;
    if(stbi_info_from_memory(data.data(),int(n),&w,&h,&c)){
        if(w!=h||w<16||w>128||w%8)return false;
        auto* rgba=stbi_load_from_memory(data.data(),int(n),&w,&h,&c,4);
        if(!rgba)return false;
        out.size=unsigned(w);out.rgba.assign(rgba,rgba+size_t(w)*h*4);stbi_image_free(rgba);return true;
    }
    if(n<12||std::memcmp(data.data(),"RIFF",4)||std::memcmp(data.data()+8,"WEBP",4))return false;
#if defined(__ANDROID__) && defined(GBARECOMP_HAVE_SDL2)
    // BitmapFactory supports WebP on all supported Android versions. Decode
    // runs only at explicit load/reload, on SDL's attached runtime thread.
    auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if(!env)return false;
    jobject activity=static_cast<jobject>(SDL_AndroidGetActivity());
    if(!activity)return false;
    jclass cls=env->GetObjectClass(activity);
    jmethodID method=env->GetStaticMethodID(cls,"decodeHdTile","(Ljava/lang/String;)[I");
    jstring name=env->NewStringUTF(path.string().c_str());
    auto image=method?static_cast<jintArray>(env->CallStaticObjectMethod(cls,method,name)):nullptr;
    if(env->ExceptionCheck()){env->ExceptionClear();image=nullptr;}
    bool ok=false;
    if(image&&env->GetArrayLength(image)>=2){
        jint dims[2];env->GetIntArrayRegion(image,0,2,dims);w=dims[0];h=dims[1];
        if(w==h&&w>=16&&w<=128&&w%8==0&&env->GetArrayLength(image)==2+w*h){
            std::vector<jint> argb(size_t(w)*h);env->GetIntArrayRegion(image,2,w*h,argb.data());
            out.size=w;out.rgba.resize(size_t(w)*h*4);
            for(size_t i=0;i<argb.size();++i){uint32_t v=uint32_t(argb[i]);out.rgba[i*4]=v>>16;out.rgba[i*4+1]=v>>8;out.rgba[i*4+2]=v;out.rgba[i*4+3]=v>>24;}ok=true;
        }
    }
    if(image)env->DeleteLocalRef(image);
    env->DeleteLocalRef(name);env->DeleteLocalRef(cls);env->DeleteLocalRef(activity);return ok;
#elif defined(__linux__)
    void* lib=dlopen("libwebp.so.7",RTLD_LAZY|RTLD_LOCAL);
    if(!lib)lib=dlopen("libwebp.so",RTLD_LAZY|RTLD_LOCAL);
    if(!lib)return false;
    auto info=reinterpret_cast<int(*)(const uint8_t*,size_t,int*,int*)>(dlsym(lib,"WebPGetInfo"));
    auto unpack=reinterpret_cast<uint8_t*(*)(const uint8_t*,size_t,uint8_t*,size_t,int)>(dlsym(lib,"WebPDecodeRGBAInto"));
    bool ok=info&&unpack&&info(data.data(),data.size(),&w,&h)&&w==h&&w>=16&&w<=128&&w%8==0;
    if(ok){out.size=w;out.rgba.resize(size_t(w)*h*4);ok=unpack(data.data(),data.size(),out.rgba.data(),out.rgba.size(),w*4)!=nullptr;}
    dlclose(lib);return ok;
#else
    return false; // PNG always available; unsupported codecs fail to native.
#endif
}
}
std::unique_ptr<gba::TextureReplacementPack> load_hd_texture_pack(const std::string& directory,const std::string& rom_sha1,std::string& report){
    namespace fs=std::filesystem;std::error_code ec;
    auto root=fs::canonical(directory,ec);
    if(ec){report="HD pack directory unavailable; original art retained";return {};}
    auto manifest=root/"pack.json";auto size=fs::file_size(manifest,ec);
    if(ec||size>4*1024*1024){report="Missing or oversized pack.json";return {};}
    std::ifstream file(manifest);std::string contents((std::istreambuf_iterator<char>(file)),{});
    json::Value doc;if(!json::parse(contents,doc,&report))return {};
    if(doc.integer("version",0)!=1||doc.str("rom_sha1")!=rom_sha1){report="HD pack version or ROM SHA-1 mismatch";return {};}
    auto* tiles=doc.get("tiles");if(!tiles||!tiles->is_array()||tiles->array.size()>gba::TextureReplacementPack::kMaxTiles){report="Invalid HD tile list";return {};}
    auto pack=std::make_unique<gba::TextureReplacementPack>();unsigned skipped=0;
    for(const auto& entry:tiles->array){
        auto id=entry.str("key");uint64_t key=0;
        if(id.size()!=16||id.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos){++skipped;continue;}
        for(char c:id)key=(key<<4)|unsigned(c>='0'&&c<='9'?c-'0':(c|32)-'a'+10);
        fs::path rel=entry.str("file");
        if(rel.empty()||rel.is_absolute()||rel.has_root_name()){++skipped;continue;}
        auto path=fs::canonical(root/rel,ec);
        if(ec){++skipped;continue;}
        auto relative=path.lexically_relative(root);
        if(relative.empty()||*relative.begin()==".."){++skipped;continue;}
        gba::HdTile image;
        if(!decode(path,image)||!pack->add(key,std::move(image))){++skipped;continue;}
    }
    report="HD pack: "+std::to_string(pack->size())+" tiles, "+std::to_string(pack->bytes())+" decoded bytes; "+std::to_string(skipped)+" skipped";
    if(!pack->size())return {};
    return pack;
}
bool write_hd_rgb_png(const char* path,const uint8_t* rgb,unsigned width,unsigned height){return stbi_write_png(path,int(width),int(height),3,rgb,int(width)*3)!=0;}
bool write_hd_tile_catalog(const gba::TextureReplacementPack& pack,const std::string& directory){
    namespace fs=std::filesystem;std::error_code ec;fs::create_directories(directory,ec);if(ec)return false;
    std::ofstream index(fs::path(directory)/"catalog.tsv");if(!index)return false;
    index<<"key\tkind\tfile\n";
    for(const auto& [key,entry]:pack.captured()){
        char name[32];std::snprintf(name,sizeof(name),"%016llx.png",static_cast<unsigned long long>(key));
        auto path=fs::path(directory)/name;
        if(!stbi_write_png(path.string().c_str(),8,8,3,entry.rgb.data(),24))return false;
        index<<std::string(name,16)<<'\t'<<(entry.obj?"obj":"bg")<<'\t'<<name<<'\n';
    } return bool(index);
}
}
