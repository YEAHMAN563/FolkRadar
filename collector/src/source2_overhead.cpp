#include "../include/source2_overhead.h"
#include <Windows.h>
#include <wincodec.h>
#include <Shlwapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;
namespace {

static uint16_t U16(const std::vector<uint8_t>& b, size_t o) { return o + 2 <= b.size() ? uint16_t(b[o] | (b[o+1] << 8)) : 0; }
static uint32_t U32(const std::vector<uint8_t>& b, size_t o) { return o + 4 <= b.size() ? uint32_t(b[o] | (b[o+1] << 8) | (b[o+2] << 16) | (b[o+3] << 24)) : 0; }

static bool Lz4Decode(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize) {
    size_t si = 0, di = 0;
    while (si < srcSize && di < dstSize) {
        uint8_t token = src[si++];
        size_t lit = token >> 4;
        if (lit == 15) { uint8_t x; do { if (si >= srcSize) return false; x = src[si++]; lit += x; } while (x == 255); }
        if (si + lit > srcSize || di + lit > dstSize) return false;
        std::memcpy(dst + di, src + si, lit); si += lit; di += lit;
        if (si >= srcSize) break;
        if (si + 2 > srcSize) return false;
        size_t off = size_t(src[si] | (src[si+1] << 8)); si += 2;
        if (off == 0 || off > di) return false;
        size_t match = token & 15;
        if (match == 15) { uint8_t x; do { if (si >= srcSize) return false; x = src[si++]; match += x; } while (x == 255); }
        match += 4;
        if (di + match > dstSize) return false;
        for (size_t i = 0; i < match; ++i) dst[di++] = dst[di - off];
    }
    return di == dstSize;
}

static std::vector<fs::path> CandidateGameRoots() {
    std::vector<fs::path> roots;
    for (const char* var : {"CS2_GAME_PATH", "STEAM_GAME_PATH"}) {
        if (const char* v = std::getenv(var); v && *v) roots.emplace_back(v);
    }
    roots.emplace_back(R"(E:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo)");
    roots.emplace_back(R"(C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo)");
    roots.emplace_back(R"(C:\Program Files\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo)");
    return roots;
}

static bool ReadVpkEntry(const fs::path& dirFile, const std::string& wanted, std::vector<uint8_t>& out) {
    std::ifstream f(dirFile, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto n = static_cast<size_t>(f.tellg()); f.seekg(0);
    std::vector<uint8_t> b(n); f.read(reinterpret_cast<char*>(b.data()), n);
    if (n < 28 || U32(b,0) != 0x55aa1234 || U32(b,4) != 2) return false;
    const size_t treeStart = 28, treeEnd = treeStart + U32(b,8);
    if (treeEnd > b.size()) return false;
    size_t p = treeStart;
    auto str = [&](std::string& s)->bool { if (p >= treeEnd) return false; size_t q=p; while (q<treeEnd && b[q]) ++q; if (q>=treeEnd) return false; s.assign(reinterpret_cast<const char*>(b.data()+p), q-p); p=q+1; return true; };
    for (;;) {
        std::string ext; if (!str(ext) || ext.empty()) break;
        for (;;) {
            std::string path; if (!str(path)) return false; if (path.empty()) break;
            for (;;) {
                std::string name; if (!str(name)) return false; if (name.empty()) break;
                if (p + 18 > treeEnd) return false;
                uint16_t preload = U16(b,p+4), archive = U16(b,p+6);
                uint32_t offset = U32(b,p+8), length = U32(b,p+12);
                p += 18;
                std::string full = (path == " ") ? name + "." + ext : path + "/" + name + "." + ext;
                if (full != wanted) continue;
                std::vector<uint8_t> data;
                if (p + preload > b.size()) return false;
                data.insert(data.end(), b.begin()+p, b.begin()+p+preload);
                fs::path archiveFile;
                if (archive == 0x7fff) archiveFile = dirFile;
                else archiveFile = dirFile.parent_path() / (dirFile.stem().string().substr(0, dirFile.stem().string().find("_dir")) + "_" + std::to_string(archive) + ".vpk");
                std::ifstream af(archiveFile, std::ios::binary);
                if (!af) return false;
                const bool standalone = dirFile.stem().string().find("_dir") == std::string::npos;
                af.seekg(archive == 0x7fff && !standalone ? static_cast<std::streamoff>(treeEnd + offset) : static_cast<std::streamoff>(offset));
                std::vector<uint8_t> payload(length); af.read(reinterpret_cast<char*>(payload.data()), length);
                if (static_cast<size_t>(af.gcount()) != length) return false;
                data.insert(data.end(), payload.begin(), payload.end()); out = std::move(data); return true;
            }
        }
    }
    return false;
}

static bool DecodeVtex(const std::vector<uint8_t>& vtex, std::vector<uint8_t>& pixels, uint32_t& width, uint32_t& height) {
    if (vtex.size() < 32 || U16(vtex,4) != 12) return false;
    uint32_t blocks = U32(vtex,12), table = U32(vtex,8); size_t pos = 16 + table - 8;
    size_t dataOff=0, dataSize=0;
    for (uint32_t i=0; i<blocks; ++i) {
        if (pos+12 > vtex.size()) return false;
        uint32_t type=U32(vtex,pos), rel=U32(vtex,pos+4), size=U32(vtex,pos+8); size_t field=pos+4; pos+=12;
        if (type == 0x41544144) { dataOff=field+rel; dataSize=size; }
    }
    if (!dataOff || dataOff+40 > vtex.size()) return false;
    uint16_t version=U16(vtex,dataOff); (void)version;
    width=U16(vtex,dataOff+20); height=U16(vtex,dataOff+22); uint8_t format=vtex[dataOff+26];
    uint8_t mips=vtex[dataOff+27]; uint32_t extraOff=U32(vtex,dataOff+32), extraCount=U32(vtex,dataOff+36);
    if (!width || !height || mips < 1 || format != 28) return false; // BGRA8888, the CS2 overhead format
    size_t dataStart=dataOff+dataSize; if (dataStart > vtex.size()) return false;
    bool compressed=false; uint32_t compressedSize=0;
    size_t ep=dataOff+40; ep += (extraOff >= 8 ? extraOff-8 : 0);
    for (uint32_t i=0; i<extraCount; ++i) {
        if (ep+12>vtex.size()) return false; uint32_t type=U32(vtex,ep), rel=U32(vtex,ep+4), size=U32(vtex,ep+8); size_t prev=ep+12; ep=prev;
        if (type==4) { size_t q=prev + (rel>=8?rel-8:0); if(q+12>vtex.size()) return false; compressed=U32(vtex,q)==1; if(compressed) compressedSize=U32(vtex,q+8); }
    }
    size_t rawSize=size_t(width)*height*4; pixels.resize(rawSize);
    if (compressed) { if (!compressedSize || dataStart+compressedSize>vtex.size()) return false; return Lz4Decode(vtex.data()+dataStart,compressedSize,pixels.data(),rawSize); }
    if (dataStart+rawSize>vtex.size()) return false; std::memcpy(pixels.data(),vtex.data()+dataStart,rawSize); return true;
}

static bool WritePng(const fs::path& path, const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    static bool com=false; if(!com) { if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)) && !com) return false; com=true; }
    IWICImagingFactory* factory=nullptr; if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) return false;
    IWICBitmap* bitmap=nullptr; HRESULT hr=factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppBGRA,width*4,(UINT)pixels.size(),const_cast<BYTE*>(pixels.data()),&bitmap);
    IWICStream* stream=nullptr; if(SUCCEEDED(hr)) hr=factory->CreateStream(&stream); if(SUCCEEDED(hr)) hr=stream->InitializeFromFilename(path.wstring().c_str(),GENERIC_WRITE);
    IWICBitmapEncoder* enc=nullptr; if(SUCCEEDED(hr)) hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&enc); if(SUCCEEDED(hr)) hr=enc->Initialize(stream,WICBitmapEncoderNoCache);
    IWICBitmapFrameEncode* frame=nullptr; if(SUCCEEDED(hr)) hr=enc->CreateNewFrame(&frame,nullptr); if(SUCCEEDED(hr)) hr=frame->Initialize(nullptr); if(SUCCEEDED(hr)) hr=frame->WriteSource(bitmap,nullptr); if(SUCCEEDED(hr)) hr=frame->Commit(); if(SUCCEEDED(hr)) hr=enc->Commit();
    if(frame)frame->Release(); if(enc)enc->Release(); if(stream)stream->Release(); if(bitmap)bitmap->Release(); factory->Release(); return SUCCEEDED(hr);
}
}

bool EnsureSource2OverheadMap(const std::string& mapName, const std::string& webRoot) {
    if (mapName.empty() || mapName == "de_unknown") return false;
    fs::path output=fs::path(webRoot)/"maps"/(mapName+".png"); std::error_code ec; if(fs::exists(output,ec) && fs::file_size(output,ec)>64) return true;
    const std::array<std::string,4> wantedPaths{
        "panorama/images/overheadmaps/"+mapName+"_radar_psd.vtex_c",
        "maps/"+mapName+"/overheadmaps/"+mapName+"_radar_psd.vtex_c",
        "maps/"+mapName+"/"+mapName+"_radar_psd.vtex_c",
        "maps/"+mapName+"/radar/"+mapName+"_radar_psd.vtex_c"
    };
    std::vector<uint8_t> vtex; bool found=false;
    for(const auto& root:CandidateGameRoots()) {
        std::array<fs::path,2> archives{root/"pak01_dir.vpk", root/"maps"/(mapName+".vpk")};
        for (const auto& dir : archives) {
            if (!fs::exists(dir)) continue;
            for (const auto& wanted : wantedPaths) {
                if(ReadVpkEntry(dir,wanted,vtex)) { found=true; break; }
            }
            if (found) break;
        }
        if (found) break;
    }
    if(!found) return false;
    std::vector<uint8_t> pixels; uint32_t w=0,h=0; if(!DecodeVtex(vtex,pixels,w,h)) return false;
    fs::create_directories(output.parent_path(),ec); if(!WritePng(output,pixels,w,h)) return false;
    std::cout << "[MapAssets] Decoded " << mapName << " overhead map in collector" << std::endl; return true;
}
