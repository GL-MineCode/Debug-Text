#define NOMINMAX

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "FontBuilder.hpp"
#include "HeaderEmitter.hpp"
#include "Preview.hpp"

namespace fs = std::filesystem;

namespace {

struct CliOptions {
    std::string root = ".";
    bool all = false;
    bool preview = false;
    bool quiet = false;
    std::string ttf;
    std::string id;
    std::string out;
    std::string name;
    std::string copyright = "Copyright (c) 2022, TakWolf (https://takwolf.com), with Reserved Font Name \"Fusion Pixel\"";
    std::string license = "SIL Open Font License 1.1";
    std::string version;
    int ptsize = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int tabColumns = 4;
    uint32_t maxCodepoint = 0x10FFFFu;
    std::vector<int> sizes{ 8, 10, 12 };
};

void usage() {
    std::printf(
        "DebugText 字库生成器（TTF -> 字库 header，v2 格式）\n"
        "\n"
        "用法：\n"
        "  generator --all [--root <仓库根目录>] [--preview] [--quiet]\n"
        "  generator --ttf <字体.ttf> --id <FontID> [--out <输出.hpp>] [选项]\n"
        "\n"
        "选项：\n"
        "  --all                生成内置预设（8/10/12px mono zh_hans）\n"
        "  --ttf <路径>         指定源 TTF\n"
        "  --id <FontID>        字库 ID，例如 fusion-pixel-12px-mono-zh_hans\n"
        "  --out <路径>         输出 header 路径（默认 include/fonts/<id>.hpp）\n"
        "  --name <文本>        显示名称（默认取 TTF 的 family name）\n"
        "  --copyright <文本>   写入 header 的版权声明\n"
        "  --license <文本>     许可名称（默认 SIL Open Font License 1.1）\n"
        "  --version <文本>     源字体版本（--all 时会自动从目录名推断）\n"
        "  --ptsize <n>         点大小（默认 12）\n"
        "  --cell-w <n>         全角宽（默认 = ptsize）\n"
        "  --cell-h <n>         单元格高（默认 = ptsize）\n"
        "  --tab-cols <n>       Tab 宽度 = n 个全角宽（默认 4）\n"
        "  --max-cp <n>         扫描码点上限（默认 0x10FFFF）\n"
        "  --root <路径>        仓库根目录（默认当前目录）\n"
        "  --preview            显示 SDL 预览窗口\n"
        "  --quiet              安静模式\n"
        "  -h, --help           显示本帮助\n");
}

std::string toIdentifier(const std::string& id) {
    std::string result;
    result.reserve(id.size());
    for (char ch : id) {
        const bool ok = std::isalnum(static_cast<unsigned char>(ch)) != 0;
        result.push_back(ok ? ch : '_');
    }
    return result.empty() ? std::string("font") : result;
}

//! 从 .../fusion-pixel-font-12px-monospaced-ttf-v2026.10.25/xxx.ttf 里推断版本号
std::string detectVersion(const std::string& ttfPath) {
    const size_t pos = ttfPath.rfind("-v");
    if (pos == std::string::npos) return {};

    const size_t start = pos + 2;
    size_t end = start;
    while (end < ttfPath.size()
           && (std::isdigit(static_cast<unsigned char>(ttfPath[end])) != 0 || ttfPath[end] == '.')) {
        ++end;
    }
    if (end == start) return {};
    if (end < ttfPath.size() && ttfPath[end] != '\\' && ttfPath[end] != '/') return {};
    return ttfPath.substr(start, end - start);
}

std::string findPresetTTF(const fs::path& root, int size) {
    const std::string fileName = "fusion-pixel-" + std::to_string(size) + "px-monospaced-zh_hans.ttf";

    std::error_code code;

    // 优先使用带版本号的目录（font/fusion-pixel-font-Npx-...-vXXXX/），避免用到根目录下的旧文件
    const fs::path fontDir = root / "font";
    if (fs::is_directory(fontDir, code)) {
        for (const auto& entry : fs::recursive_directory_iterator(fontDir, code)) {
            if (code) break;
            if (!entry.is_regular_file()) continue;
            if (entry.path().filename() != fileName) continue;
            if (entry.path().parent_path() == fontDir) continue;
            return entry.path().string();
        }
    }

    const fs::path direct = fontDir / fileName;
    if (fs::is_regular_file(direct, code)) return direct.string();

    return {};
}

struct Job {
    std::string ttf;
    std::string id;
    std::string displayName;
    std::string version;
    std::string output;
    int ptsize = 0;
};

} // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    // 让中文日志在 Windows 控制台正常显示
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto takeValue = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "参数 %s 缺少值\n", name);
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--all") {
            options.all = true;
        } else if (arg == "--preview") {
            options.preview = true;
        } else if (arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "--root") {
            options.root = takeValue("--root");
        } else if (arg == "--ttf") {
            options.ttf = takeValue("--ttf");
        } else if (arg == "--id") {
            options.id = takeValue("--id");
        } else if (arg == "--out") {
            options.out = takeValue("--out");
        } else if (arg == "--name") {
            options.name = takeValue("--name");
        } else if (arg == "--copyright") {
            options.copyright = takeValue("--copyright");
        } else if (arg == "--license") {
            options.license = takeValue("--license");
        } else if (arg == "--version") {
            options.version = takeValue("--version");
        } else if (arg == "--ptsize") {
            options.ptsize = std::atoi(takeValue("--ptsize").c_str());
        } else if (arg == "--cell-w") {
            options.cellWidth = std::atoi(takeValue("--cell-w").c_str());
        } else if (arg == "--cell-h") {
            options.cellHeight = std::atoi(takeValue("--cell-h").c_str());
        } else if (arg == "--tab-cols") {
            options.tabColumns = std::atoi(takeValue("--tab-cols").c_str());
        } else if (arg == "--max-cp") {
            options.maxCodepoint = static_cast<uint32_t>(std::strtoul(takeValue("--max-cp").c_str(), nullptr, 0));
        } else if (arg == "-h" || arg == "--help") {
            usage();
            return 0;
        } else {
            std::fprintf(stderr, "未知参数：%s\n", arg.c_str());
            usage();
            return 2;
        }
    }

    const fs::path root = fs::weakly_canonical(options.root);

    // ---------------- 组装任务 ----------------
    std::vector<Job> jobs;
    if (options.all) {
        for (int size : options.sizes) {
            const std::string ttf = findPresetTTF(root, size);
            if (ttf.empty()) {
                std::fprintf(stderr, "找不到 %dpx 的预设字体（font/**/fusion-pixel-%dpx-monospaced-zh_hans.ttf）\n", size, size);
                return 1;
            }
            Job job;
            job.ttf = ttf;
            job.id = "fusion-pixel-" + std::to_string(size) + "px-mono-zh_hans";
            job.version = detectVersion(ttf);
            job.output = (root / "include" / "fonts" / (toIdentifier(job.id) + ".hpp")).string();
            job.ptsize = size;
            jobs.push_back(job);
        }
    } else {
        if (options.ttf.empty() || options.id.empty()) {
            std::fprintf(stderr, "需要同时指定 --ttf 与 --id（或使用 --all）\n");
            usage();
            return 2;
        }
        Job job;
        job.ttf = options.ttf;
        job.id = options.id;
        job.displayName = options.name;
        job.version = options.version.empty() ? detectVersion(options.ttf) : options.version;
        job.output = options.out.empty()
            ? (root / "include" / "fonts" / (toIdentifier(job.id) + ".hpp")).string()
            : options.out;
        job.ptsize = options.ptsize;
        jobs.push_back(job);
    }

    // ---------------- SDL / SDL_ttf ----------------
    const SDL_InitFlags initFlags = options.preview ? SDL_INIT_VIDEO : 0;
    if (!SDL_Init(initFlags)) {
        std::fprintf(stderr, "SDL_Init 失败：%s\n", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        std::fprintf(stderr, "TTF_Init 失败：%s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    PreviewWindow preview;
    if (options.preview) {
        std::string error;
        if (!preview.create("DebugText 字库生成器", jobs.front().ttf, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            options.preview = false;
        }
    }

    // ---------------- 逐个生成 ----------------
    bool success = true;
    for (const Job& job : jobs) {
        if (!options.quiet) std::printf("正在处理 %s\n", job.ttf.c_str());

        FontBuildOptions buildOptions;
        buildOptions.ttfPath = job.ttf;
        buildOptions.id = job.id;
        buildOptions.displayName = job.displayName;
        buildOptions.copyright = options.copyright;
        buildOptions.licenseName = options.license;
        buildOptions.sourceVersion = job.version;
        buildOptions.ptsize = job.ptsize;
        buildOptions.cellWidth = options.cellWidth;
        buildOptions.cellHeight = options.cellHeight;
        buildOptions.tabColumns = options.tabColumns;
        buildOptions.maxCodepoint = options.maxCodepoint;
        buildOptions.verbose = !options.quiet;

        if (options.preview) {
            buildOptions.onProgress = &PreviewWindow::progressCallback;
            buildOptions.progressUser = &preview;
            preview.setStatus(job.id);
        }

        PackedFont packed;
        std::string error;
        if (!buildFontFromTTF(buildOptions, packed, error)) {
            std::fprintf(stderr, "生成失败：%s\n", error.c_str());
            success = false;
            continue;
        }

        bool changed = false;
        if (!emitFontHeader(packed, job.output, error, changed)) {
            std::fprintf(stderr, "写出 header 失败：%s\n", error.c_str());
            success = false;
            continue;
        }

        if (!options.quiet) {
            printFontReport(packed);
            std::printf("  输出      : %s%s\n\n", job.output.c_str(), changed ? "" : "（内容无变化，已跳过写入）");
        }
    }

    preview.destroy();
    TTF_Quit();
    SDL_Quit();

    return success ? 0 : 1;
}
