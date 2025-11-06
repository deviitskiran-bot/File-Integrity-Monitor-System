#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <iomanip>
#include <fstream>
#include <thread>
#include "utils.h"

namespace fs = std::filesystem;

// Extended FileRecord to include permissions
struct FileRecordExtended : public FileRecord {
    fs::perms permissions;
};

// Log function
void log_change(const std::string &message) {
    std::ofstream log("fim_log.txt", std::ios::app);
    if (log) {
        log << time_to_string(std::time(nullptr)) << " - " << message << "\n";
    }
}

// Print table header
void print_header() {
    std::cout << std::left << std::setw(10) << "STATUS" 
              << std::setw(60) << "FILE PATH" 
              << std::setw(20) << "MOD TIME" 
              << std::setw(10) << "SIZE" 
              << std::setw(12) << "PERMISSIONS" << "\n";
    std::cout << std::string(120, '-') << "\n";
}

// Convert permissions to string
std::string perms_to_string(fs::perms p) {
    std::string s;
    s += ((p & fs::perms::owner_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::owner_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::owner_exec) != fs::perms::none ? "x" : "-");
    s += ((p & fs::perms::group_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::group_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::group_exec) != fs::perms::none ? "x" : "-");
    s += ((p & fs::perms::others_read) != fs::perms::none ? "r" : "-");
    s += ((p & fs::perms::others_write) != fs::perms::none ? "w" : "-");
    s += ((p & fs::perms::others_exec) != fs::perms::none ? "x" : "-");
    return s;
}

int main() {
    std::unordered_map<std::string, FileRecordExtended> baseline;
    std::string dir = ".";
    
    std::cout << "Enter directory to monitor: ";
    std::getline(std::cin, dir);

    // Build baseline
    for (auto &entry : fs::recursive_directory_iterator(dir)) {
        if (fs::is_regular_file(entry)) {
            FileRecordExtended rec;
            rec.size = fs::file_size(entry);
            rec.mtime = std::chrono::system_clock::to_time_t(
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fs::last_write_time(entry) - fs::file_time_type::clock::now() 
                    + std::chrono::system_clock::now())
            );
            rec.permissions = fs::status(entry).permissions();
            rec.sha256_hex = sha256_of_file(entry.path().string());

            baseline[entry.path().string()] = rec;
        }
    }

    std::cout << "\nBaseline created for " << baseline.size() << " files.\n";
    print_header();
    for (auto &[path, rec] : baseline) {
        std::cout << std::setw(10) << "BASELINE"
                  << std::setw(60) << path
                  << std::setw(20) << time_to_string(rec.mtime)
                  << std::setw(10) << rec.size
                  << std::setw(12) << perms_to_string(rec.permissions) << "\n";
    }

    std::cout << "\nNow modify, create, or delete a file in that directory and press Enter to rescan.\n";
    std::cin.get();

    // Rescan
    std::unordered_map<std::string, FileRecordExtended> current;
    for (auto &entry : fs::recursive_directory_iterator(dir)) {
        if (fs::is_regular_file(entry)) {
            FileRecordExtended rec;
            rec.size = fs::file_size(entry);
            rec.mtime = std::chrono::system_clock::to_time_t(
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fs::last_write_time(entry) - fs::file_time_type::clock::now() 
                    + std::chrono::system_clock::now())
            );
            rec.permissions = fs::status(entry).permissions();

            // Only hash if size or mtime changed
            if (baseline.find(entry.path().string()) == baseline.end() ||
                baseline[entry.path().string()].size != rec.size ||
                baseline[entry.path().string()].mtime != rec.mtime) {
                rec.sha256_hex = sha256_of_file(entry.path().string());
            } else {
                rec.sha256_hex = baseline[entry.path().string()].sha256_hex;
            }

            current[entry.path().string()] = rec;
        }
    }

    std::cout << "\nChanges detected:\n";
    print_header();

    // Compare baseline vs current
    for (auto &[path, oldrec] : baseline) {
        if (current.find(path) == current.end()) {
            std::cout << std::setw(10) << "[DELETED]" 
                      << std::setw(60) << path 
                      << std::setw(20) << time_to_string(oldrec.mtime)
                      << std::setw(10) << oldrec.size
                      << std::setw(12) << perms_to_string(oldrec.permissions) << "\n";
            log_change("[DELETED] " + path);
        } else if (current[path].sha256_hex != oldrec.sha256_hex ||
                   current[path].permissions != oldrec.permissions) {
            std::cout << std::setw(10) << "[MODIFIED]" 
                      << std::setw(60) << path 
                      << std::setw(20) << time_to_string(current[path].mtime)
                      << std::setw(10) << current[path].size
                      << std::setw(12) << perms_to_string(current[path].permissions) << "\n";
            log_change("[MODIFIED] " + path);
        }
    }
    for (auto &[path, newrec] : current) {
        if (baseline.find(path) == baseline.end()) {
            std::cout << std::setw(10) << "[CREATED]" 
                      << std::setw(60) << path 
                      << std::setw(20) << time_to_string(newrec.mtime)
                      << std::setw(10) << newrec.size
                      << std::setw(12) << perms_to_string(newrec.permissions) << "\n";
            log_change("[CREATED] " + path);
        }
    }

    std::cout << "\nScan complete. All changes are logged to fim_log.txt.\n";
    return 0;
}
