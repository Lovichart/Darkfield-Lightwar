#include "Common.h"

std::filesystem::path exeDirectory() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4]{};
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (n > 0 && n < std::size(buf)) {
        return std::filesystem::path(buf).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path assetPath(const std::string& relative) {
    const std::filesystem::path rel = std::filesystem::u8path(relative);
    const std::filesystem::path a = exeDirectory() / rel;
    if (std::filesystem::exists(a)) return a;
    const std::filesystem::path b = std::filesystem::current_path() / rel;
    if (std::filesystem::exists(b)) return b;
    return a;
}

std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in.is_open()) return lines;
    std::string line;
    while (std::getline(in, line)) {
        // 容忍 UTF-8 BOM：names/*.txt 用不同编辑器保存时首行可能带 BOM。
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        const auto last = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(first, last - first + 1);
        if (!trimmed.empty()) lines.push_back(std::move(trimmed));
    }
    return lines;
}
