#include "utils.hpp"
#include <fstream>
#include "zlib.h"

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    std::streamsize sz = f.tellg();
    if (sz <= 0) return false;
    out.resize(static_cast<size_t>(sz));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return f.good();
}

bool DecompressZlib(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst) {
    dst.resize(srcLen * 8);
    z_stream strm{};
    strm.next_in  = const_cast<Bytef*>(src);
    strm.avail_in = static_cast<uInt>(srcLen);
    
    if (inflateInit(&strm) != Z_OK) return false;
    
    int ret;
    do {
        strm.next_out  = dst.data() + strm.total_out;
        strm.avail_out = static_cast<uInt>(dst.size() - strm.total_out);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_BUF_ERROR || (ret == Z_OK && strm.avail_out == 0)) {
            dst.resize(dst.size() * 2);
        }
    } while (ret == Z_OK || ret == Z_BUF_ERROR);
    
    if (ret != Z_STREAM_END) {
        inflateEnd(&strm);
        return false;
    }
    
    dst.resize(strm.total_out);
    inflateEnd(&strm);
    return true;
}
