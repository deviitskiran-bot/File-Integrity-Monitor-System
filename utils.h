#pragma once
#include <string>
#include <ctime>
#include <cstdint>

struct FileRecord {
    std::string sha256_hex;
    std::uintmax_t size;
    std::time_t mtime;
};

std::string sha256_of_file(const std::string &path);
std::string time_to_string(std::time_t t);
