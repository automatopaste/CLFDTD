#pragma once

#include <cstdint>
#include <fstream>
#include <vector>

template <typename T>
inline static void writeT(std::ofstream& file, T& i) {
    file.write((char*)&i, sizeof(T));
}
template <typename T>
inline static T readT(std::ifstream& file) {
    T i;
    file.read((char*)&i, sizeof(T));
    return i;
}

template <typename T>
inline static void writeTbuf(std::ofstream& file, std::vector<T>& buf) {
    uint32_t size = static_cast<uint32_t>(buf.size()) * sizeof(T);
    writeT<uint32_t>(file, size);
    file.write((char*)buf.data(), size);
}
template <typename T>
inline static void readTbuf(std::ifstream& file, std::vector<T>& buf) {
    uint32_t size = readT<uint32_t>(file);
    buf.resize(size / sizeof(T));
    file.read((char*)buf.data(), size);
}

inline static void writeStr(std::ofstream& file, std::string& string) {
    uint32_t len = static_cast<uint32_t>(string.length());
    writeT<uint32_t>(file, len);
    file.write(string.c_str(), len);
}

inline static void readStr(std::ifstream& file, std::string& dest) {
    uint32_t size = readT<uint32_t>(file);
    if (size == 0) return;
    dest.resize(size, '\0');
    file.read(&dest[0], size);
}