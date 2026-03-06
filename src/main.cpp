#include "ncmcrypt.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include "platform.h"
#endif

#include "color.h"
#include "version.h"
#include "cxxopts.hpp"

namespace fs = std::filesystem;

static std::string toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool isSupportedRemoveFile(const fs::path &path)
{
    if (!path.has_extension())
    {
        return false;
    }

    std::string extension = toLowerAscii(path.extension().u8string());
    return extension == ".ncm" || extension == ".wav" || extension == ".flac" || extension == ".mp3";
}

static bool isNcmPath(const fs::path &path)
{
    return path.has_extension() && toLowerAscii(path.extension().u8string()) == ".ncm";
}

static bool shouldProcessPath(const fs::path &path, bool isRequiredRemoved)
{
    if (isRequiredRemoved)
    {
        return isSupportedRemoveFile(path);
    }

    return isNcmPath(path);
}

static bool clearBinaryFileContents(const fs::path &path)
{
    if (!fs::is_regular_file(path) || !isSupportedRemoveFile(path))
    {
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file)
    {
        return false;
    }

    file.close();
    return !file.fail();
}

static bool isPathWithin(const fs::path &path, const fs::path &base)
{
    std::error_code ec;
    fs::path relativePath = fs::relative(path, base, ec);
    if (ec)
    {
        return false;
    }

    return !relativePath.empty() && !relativePath.is_absolute() && *relativePath.begin() != "..";
}

static std::string shellQuote(const fs::path &path)
{
    std::string quoted = "\"";
    for (char ch : path.u8string())
    {
        if (ch == '\"')
        {
            quoted += "\\\"";
        }
        else
        {
            quoted += ch;
        }
    }
    quoted += "\"";
    return quoted;
}

static bool convertFlacToWav(const fs::path &flacPath, fs::path &wavPath)
{
    wavPath = flacPath;
    wavPath.replace_extension(".wav");

    std::string command = "ffmpeg -y -loglevel error -i " + shellQuote(flacPath) + " -vn " + shellQuote(wavPath);
    int result = std::system(command.c_str());

    if (result != 0 || !fs::exists(wavPath))
    {
        return false;
    }

    std::error_code ec;
    fs::remove(flacPath, ec);
    return true;
}

static void processFile(const fs::path &filePath, const fs::path &outputFolder, bool isRequiredRemoved, bool preferLosslessFlac)
{
    if (!fs::exists(filePath))
    {
        std::cerr << BOLDRED << "[Error] " << RESET << "file '" << filePath.u8string() << "' does not exist." << std::endl;
        return;
    }

    if (!isNcmPath(filePath))
    {
        if (isRequiredRemoved && isSupportedRemoveFile(filePath))
        {
            if (clearBinaryFileContents(filePath))
            {
                std::cout << BOLDGREEN << "[Done] " << RESET << "'" << filePath.u8string()
                          << "' contents cleared as required." << std::endl;
            }
            else
            {
                std::cerr << BOLDYELLOW << "[Warn] " << RESET << "failed to clear contents of '"
                          << filePath.u8string() << "'." << std::endl;
            }
        }
        return;
    }

    try
    {
        NeteaseCrypt crypt(filePath.u8string());
        crypt.Dump(outputFolder.u8string());
        crypt.FixMetadata();
        fs::path outputPath = crypt.dumpFilepath();

        if (crypt.isLossless() && !preferLosslessFlac)
        {
            fs::path wavPath;
            if (convertFlacToWav(outputPath, wavPath))
            {
                outputPath = wavPath;
            }
            else
            {
                std::cerr << BOLDYELLOW << "[Warn] " << RESET << "failed to convert '" << outputPath.u8string()
                          << "' to wav. Falling back to flac. Make sure ffmpeg is available in PATH." << std::endl;
            }
        }

        std::cout << BOLDGREEN << "[Done] " << RESET << "'" << filePath.u8string() << "' -> '" << outputPath.u8string() << "'";

        if (isRequiredRemoved)
        {
            if (clearBinaryFileContents(filePath))
            {
                std::cout << " with source contents cleared as required.";
            }
            else
            {
                std::cout << " but failed to clear source contents.";
            }
        }
        std::cout << std::endl;
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << BOLDRED << "[Exception] " << RESET << RED << e.what() << RESET << " '" << filePath.u8string() << "'" << std::endl;
    }
    catch (...)
    {
        std::cerr << BOLDRED << "[Error] Unexpected exception while processing file: " << RESET << filePath.u8string() << std::endl;
    }
}

static void collectFilesFromDirectory(const fs::path &sourceDir, const fs::path &outputDir, bool recursive, bool isRequiredRemoved, std::vector<fs::path> &filesToProcess)
{
    std::error_code ec;
    bool outputDirIsSourceDir = fs::equivalent(outputDir, sourceDir, ec);
    bool skipOutputSubtree = !ec && !outputDirIsSourceDir && isPathWithin(outputDir, sourceDir);

    if (recursive)
    {
        for (auto it = fs::recursive_directory_iterator(sourceDir); it != fs::recursive_directory_iterator(); ++it)
        {
            const auto &entry = *it;
            fs::path currentPath = entry.path();

            if (skipOutputSubtree && entry.is_directory() && isPathWithin(currentPath, outputDir))
            {
                it.disable_recursion_pending();
                continue;
            }

            if (!entry.is_regular_file())
            {
                continue;
            }

            if (skipOutputSubtree && isPathWithin(currentPath, outputDir))
            {
                continue;
            }

            if (shouldProcessPath(currentPath, isRequiredRemoved))
            {
                filesToProcess.push_back(currentPath);
            }
        }
        return;
    }

    for (const auto &entry : fs::directory_iterator(sourceDir))
    {
        const auto path = fs::u8path(entry.path().u8string());
        if (!entry.is_regular_file())
        {
            continue;
        }

        if (skipOutputSubtree && isPathWithin(path, outputDir))
        {
            continue;
        }

        if (shouldProcessPath(path, isRequiredRemoved))
        {
            filesToProcess.push_back(path);
        }
    }
}

int main(int argc, char **argv)
{
#if defined(_WIN32)
    win32_utf8argv(&argc, &argv);
#endif

    cxxopts::Options options("ncmplus");

    options.add_options()
        ("h,help", "Print usage")
        ("d,directory", "Process files in a folder (requires folder path)", cxxopts::value<std::string>())
        ("r,recursive", "Process files recursively (requires -d option)", cxxopts::value<bool>()->default_value("false"))
        ("o,output", "Output folder (default: ./output)", cxxopts::value<std::string>())
        ("v,version", "Print version information", cxxopts::value<bool>()->default_value("false"))
        ("remove", "Clear contents of supported music files; for .ncm do it after successful processing", cxxopts::value<bool>()->default_value("false"))
        ("f,flac", "Keep dumped FLAC for lossless files (default: convert lossless output to WAV; requires ffmpeg otherwise)", cxxopts::value<bool>()->default_value("false"))
        ("filenames", "Input files", cxxopts::value<std::vector<std::string>>());

    options.positional_help("<files>");
    options.parse_positional({"filenames"});
    options.allow_unrecognised_options();

    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::parsing &)
    {
        std::cout << options.help() << std::endl;
        return 1;
    }

    if (!result.unmatched().empty())
    {
        std::cout << options.help() << std::endl;
        return 1;
    }

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    if (result.count("version"))
    {
        std::cout << "ncmplus version " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
        return 0;
    }

    if (result.count("directory") == 0 && result.count("filenames") == 0)
    {
        std::cout << options.help() << std::endl;
        return 1;
    }

    bool recursive = result["recursive"].as<bool>();
    bool removeOriginal = result["remove"].as<bool>();
    bool preferLosslessFlac = result["flac"].as<bool>();

    if (recursive && result.count("directory") == 0)
    {
        std::cerr << BOLDRED << "[Error] " << RESET << "-r option requires -d option." << std::endl;
        return 1;
    }

    fs::path outputDir = fs::u8path("./output");
    if (result.count("output"))
    {
        outputDir = fs::u8path(result["output"].as<std::string>());
    }

    if (fs::exists(outputDir) && !fs::is_directory(outputDir))
    {
        std::cerr << BOLDRED << "[Error] " << RESET << "'" << outputDir.u8string() << "' is not a valid directory." << std::endl;
        return 1;
    }
    fs::create_directories(outputDir);

    if (result.count("directory"))
    {
        fs::path sourceDir = fs::u8path(result["directory"].as<std::string>());
        if (!fs::is_directory(sourceDir))
        {
            std::cerr << BOLDRED << "[Error] " << RESET << "'" << sourceDir.u8string() << "' is not a valid directory." << std::endl;
            return 1;
        }

        sourceDir = fs::weakly_canonical(sourceDir);
        outputDir = fs::weakly_canonical(outputDir);

        std::vector<fs::path> filesToProcess;
        collectFilesFromDirectory(sourceDir, outputDir, recursive, removeOriginal, filesToProcess);

        if (filesToProcess.empty())
        {
            std::cout << BOLDYELLOW << "[Info] " << RESET << "no "
                      << (removeOriginal ? "supported music files" : ".ncm files")
                      << " found in '" << sourceDir.u8string() << "'"
                      << (recursive ? " recursively." : ".") << std::endl;
            return 0;
        }

        for (const auto &currentPath : filesToProcess)
        {
            fs::path targetFolder = outputDir;
            if (recursive)
            {
                fs::path relativePath = fs::relative(currentPath, sourceDir);
                fs::path destinationPath = outputDir / relativePath;
                fs::create_directories(destinationPath.parent_path());
                targetFolder = destinationPath.parent_path();
            }

            processFile(currentPath, targetFolder, removeOriginal, preferLosslessFlac);
        }
        return 0;
    }

    if (result.count("filenames"))
    {
        for (const auto &filePath : result["filenames"].as<std::vector<std::string>>())
        {
            fs::path filePathU8 = fs::u8path(filePath);
            if (!fs::is_regular_file(filePathU8))
            {
                std::cerr << BOLDRED << "[Error] " << RESET << "'" << filePathU8.u8string() << "' is not a valid file." << std::endl;
                continue;
            }

            processFile(filePathU8, outputDir, removeOriginal, preferLosslessFlac);
        }
    }

    return 0;
}
