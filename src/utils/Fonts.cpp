#include "utils/Fonts.h"

#include "utils/Logger.h"

#include <FL/Fl.H>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <unordered_map>

#ifdef FONTS_EMBEDDED
#include "fonts/100.h"
#include "fonts/100italic.h"
#include "fonts/200.h"
#include "fonts/200italic.h"
#include "fonts/300.h"
#include "fonts/300italic.h"
#include "fonts/400.h"
#include "fonts/400italic.h"
#include "fonts/500.h"
#include "fonts/500italic.h"
#include "fonts/600.h"
#include "fonts/600italic.h"
#include "fonts/700.h"
#include "fonts/700italic.h"
#include "fonts/800.h"
#include "fonts/800italic.h"
#include "fonts/900.h"
#include "fonts/900italic.h"
#endif

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#else
#include <fontconfig/fontconfig.h>
#endif

namespace FontLoader {

namespace {
std::unordered_map<std::string, Fl_Font> fontMap;
std::deque<std::string> fontNameStorage;
Fl_Font nextFontId = FL_FREE_FONT;
std::filesystem::path tempFontDirPath;
bool tempFontDirReady = false;

std::filesystem::path ensureTempFontDir() {
    if (tempFontDirReady) {
        return tempFontDirPath;
    }

    std::error_code ec;
    auto baseDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        Logger::error("Failed to get temp directory for fonts");
        return {};
    }

    tempFontDirPath = baseDir / "discove";
    std::filesystem::create_directories(tempFontDirPath, ec);
    if (ec || !std::filesystem::exists(tempFontDirPath, ec)) {
        Logger::warn("Failed to create temp font directory, using base temp directory");
        tempFontDirPath = baseDir;
        ec.clear();
    } else {
        for (const auto &entry : std::filesystem::directory_iterator(tempFontDirPath, ec)) {
            if (ec) {
                break;
            }
            std::error_code removeError;
            std::filesystem::remove_all(entry.path(), removeError);
        }
    }

    tempFontDirReady = true;
    return tempFontDirPath;
}

bool endsWith(const std::string &value, const std::string &suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool shouldLoadAllFonts() {
    const char *value = std::getenv("DISCOVE_LOAD_ALL_FONTS");
    return value && *value != '\0';
}

#ifdef _WIN32
std::string makeFltkFontName(const std::string &fontName) {
    if (endsWith(fontName, " Italic")) {
        std::string baseName = fontName.substr(0, fontName.size() - 7);
        if (endsWith(baseName, " Bold")) {
            baseName.erase(baseName.size() - 5);
            return "P" + baseName;
        }
        return "I" + baseName;
    }
    if (endsWith(fontName, " Bold")) {
        std::string baseName = fontName.substr(0, fontName.size() - 5);
        return "B" + baseName;
    }
    return " " + fontName;
}

bool loadFontFromMemoryWindows(const unsigned char *fontData, size_t fontSize, const std::string &fontName) {
    DWORD loadedCount = 0;
    HANDLE fontHandle = AddFontMemResourceEx(const_cast<void *>(static_cast<const void *>(fontData)),
                                             static_cast<DWORD>(fontSize), nullptr, &loadedCount);
    if (fontHandle != nullptr && loadedCount > 0) {
        Logger::debug("AddFontMemResourceEx loaded " + std::to_string(loadedCount) + " font(s) for: " + fontName);
        std::string fltkName = makeFltkFontName(fontName);
        fontNameStorage.push_back(fltkName);
        const char *persistentName = fontNameStorage.back().c_str();

        Fl::set_font(nextFontId, persistentName);
        fontMap[fontName] = nextFontId;
        nextFontId++;

        Logger::info("Loaded embedded font: " + fontName);
        return true;
    }

    DWORD error = GetLastError();
    Logger::warn("AddFontMemResourceEx failed for: " + fontName + " (error: " + std::to_string(error) +
                 "), falling back to temp file");

    std::filesystem::path tempDir = ensureTempFontDir();
    if (tempDir.empty()) {
        Logger::error("Failed to get temp directory for font: " + fontName);
        return false;
    }

    std::string safeFileName = fontName;
    std::replace(safeFileName.begin(), safeFileName.end(), ' ', '_');
    static std::atomic<unsigned int> tempCounter{0};
    unsigned int counter = tempCounter.fetch_add(1, std::memory_order_relaxed);
    std::wstring fileName = L"font_" + std::wstring(safeFileName.begin(), safeFileName.end()) + L"_" +
                            std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(counter) + L".ttf";
    std::filesystem::path tempFilePath = tempDir / fileName;
    const std::wstring tempFile = tempFilePath.wstring();

    Logger::debug("Creating temp font file: " + std::string(tempFile.begin(), tempFile.end()));
    HANDLE hFile = CreateFileW(tempFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        Logger::error("Failed to create temp font file: " + fontName + " (error: " + std::to_string(error) + ")");
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(hFile, fontData, fontSize, &written, NULL) || written != fontSize) {
        CloseHandle(hFile);
        Logger::error("Failed to write font data: " + fontName + " (wrote " + std::to_string(written) + " of " +
                      std::to_string(fontSize) + " bytes)");
        return false;
    }
    CloseHandle(hFile);

    int result = AddFontResourceExW(tempFile.c_str(), FR_PRIVATE, 0);
    if (result == 0) {
        DWORD error = GetLastError();
        Logger::error("Failed to load font from temp file: " + fontName + " (error: " + std::to_string(error) + ")");
        return false;
    }

    Logger::debug("AddFontResourceEx loaded " + std::to_string(result) + " font(s) for: " + fontName);

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded embedded font: " + fontName);
    return true;
}

bool loadFontWindows(const std::string &fontPath, const std::string &fontName) {
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0);
    if (wideSize == 0) {
        Logger::error("Failed to convert font path to wide string: " + fontPath);
        return false;
    }

    std::wstring widePath(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, fontPath.c_str(), -1, &widePath[0], wideSize);

    int result = AddFontResourceExW(widePath.c_str(), FR_PRIVATE, 0);
    if (result == 0) {
        Logger::error("Failed to load font from: " + fontPath);
        return false;
    }

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded font: " + fontName + " from " + fontPath);
    return true;
}
#elif defined(__APPLE__)
std::string makeFltkFontName(const std::string &fontName) { return fontName; }

bool loadFontFromMemoryMacOS(const unsigned char *fontData, size_t fontSize, const std::string &fontName) {
    CFDataRef fontDataRef = CFDataCreate(kCFAllocatorDefault, fontData, fontSize);
    if (!fontDataRef) {
        Logger::error("Failed to create CFData for font: " + fontName);
        return false;
    }

    CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(fontDataRef);
    CGFontRef cgFont = CGFontCreateWithDataProvider(dataProvider);
    CGDataProviderRelease(dataProvider);
    CFRelease(fontDataRef);

    if (!cgFont) {
        Logger::error("Failed to create CGFont from memory: " + fontName);
        return false;
    }

    CFErrorRef error = nullptr;
    bool registered = CTFontManagerRegisterGraphicsFont(cgFont, &error);
    CGFontRelease(cgFont);

    if (!registered) {
        if (error) {
            CFRelease(error);
        }
        Logger::error("Failed to register font from memory: " + fontName);
        return false;
    }

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded embedded font: " + fontName);
    return true;
}

bool loadFontMacOS(const std::string &fontPath, const std::string &fontName) {
    CFStringRef fontPathRef = CFStringCreateWithCString(kCFAllocatorDefault, fontPath.c_str(), kCFStringEncodingUTF8);
    CFURLRef fontUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fontPathRef, kCFURLPOSIXPathStyle, false);
    CFRelease(fontPathRef);

    if (!fontUrl) {
        Logger::error("Failed to create font URL for: " + fontPath);
        return false;
    }

    CFArrayRef fontDescriptors = CTFontManagerCreateFontDescriptorsFromURL(fontUrl);
    CFRelease(fontUrl);

    if (!fontDescriptors || CFArrayGetCount(fontDescriptors) == 0) {
        Logger::error("Failed to load font descriptors from: " + fontPath);
        if (fontDescriptors)
            CFRelease(fontDescriptors);
        return false;
    }

    bool registered = CTFontManagerRegisterFontsForURL(fontUrl, kCTFontManagerScopeProcess, nullptr);
    CFRelease(fontDescriptors);

    if (!registered) {
        Logger::error("Failed to register font from: " + fontPath);
        return false;
    }

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded font: " + fontName + " from " + fontPath);
    return true;
}
#else
std::string makeFltkFontName(const std::string &fontName) {
    if (endsWith(fontName, " Italic")) {
        std::string baseName = fontName.substr(0, fontName.size() - 7);
        if (endsWith(baseName, " Bold")) {
            baseName.erase(baseName.size() - 5);
            return "P" + baseName;
        }
        return "I" + baseName;
    }
    if (endsWith(fontName, " Bold")) {
        std::string baseName = fontName.substr(0, fontName.size() - 5);
        return "B" + baseName;
    }
    return " " + fontName;
}

bool loadFontFromMemoryLinux(const unsigned char *fontData, size_t fontSize, const std::string &fontName) {
    std::filesystem::path tempDir = ensureTempFontDir();
    if (tempDir.empty()) {
        Logger::error("Failed to get temp directory for font: " + fontName);
        return false;
    }

    std::string safeFileName = fontName;
    std::replace(safeFileName.begin(), safeFileName.end(), ' ', '_');
    std::filesystem::path tempPath = tempDir / ("font_" + safeFileName + ".ttf");

    std::ofstream tempFile(tempPath.string(), std::ios::binary);
    if (!tempFile.is_open()) {
        Logger::error("Failed to create temporary font file for: " + fontName);
        return false;
    }

    tempFile.write(reinterpret_cast<const char *>(fontData), fontSize);
    tempFile.close();

    FcConfig *config = FcInitLoadConfigAndFonts();
    if (!config) {
        Logger::error("Failed to initialize fontconfig");
        return false;
    }

    FcBool result = FcConfigAppFontAddFile(config, (const FcChar8 *)tempPath.c_str());
    if (!result) {
        Logger::error("Failed to add font to fontconfig: " + fontName);
        FcConfigDestroy(config);
        return false;
    }

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded embedded font: " + fontName);
    return true;
}

bool loadFontLinux(const std::string &fontPath, const std::string &fontName) {
    FcConfig *config = FcInitLoadConfigAndFonts();
    if (!config) {
        Logger::error("Failed to initialize fontconfig");
        return false;
    }

    FcBool result = FcConfigAppFontAddFile(config, (const FcChar8 *)fontPath.c_str());
    if (!result) {
        Logger::error("Failed to add font to fontconfig: " + fontPath);
        FcConfigDestroy(config);
        return false;
    }

    std::string fltkName = makeFltkFontName(fontName);
    fontNameStorage.push_back(fltkName);
    const char *persistentName = fontNameStorage.back().c_str();

    Fl::set_font(nextFontId, persistentName);
    fontMap[fontName] = nextFontId;
    nextFontId++;

    Logger::info("Loaded font: " + fontName + " from " + fontPath);
    return true;
}
#endif

bool loadFontFromMemory(const unsigned char *fontData, size_t fontSize, const std::string &fontName) {
#ifdef _WIN32
    return loadFontFromMemoryWindows(fontData, fontSize, fontName);
#elif defined(__APPLE__)
    return loadFontFromMemoryMacOS(fontData, fontSize, fontName);
#else
    return loadFontFromMemoryLinux(fontData, fontSize, fontName);
#endif
}

bool loadFont(const std::string &fontPath, const std::string &fontName) {
#ifdef _WIN32
    return loadFontWindows(fontPath, fontName);
#elif defined(__APPLE__)
    return loadFontMacOS(fontPath, fontName);
#else
    return loadFontLinux(fontPath, fontName);
#endif
}

} // namespace

namespace Fonts {
Fl_Font INTER_THIN = FL_HELVETICA;
Fl_Font INTER_EXTRA_LIGHT = FL_HELVETICA;
Fl_Font INTER_LIGHT = FL_HELVETICA;
Fl_Font INTER_REGULAR = FL_HELVETICA;
Fl_Font INTER_MEDIUM = FL_HELVETICA;
Fl_Font INTER_SEMIBOLD = FL_HELVETICA_BOLD;
Fl_Font INTER_BOLD = FL_HELVETICA_BOLD;
Fl_Font INTER_EXTRA_BOLD = FL_HELVETICA_BOLD;
Fl_Font INTER_BLACK = FL_HELVETICA_BOLD;
Fl_Font INTER_THIN_ITALIC = FL_HELVETICA_ITALIC;
Fl_Font INTER_EXTRA_LIGHT_ITALIC = FL_HELVETICA_ITALIC;
Fl_Font INTER_LIGHT_ITALIC = FL_HELVETICA_ITALIC;
Fl_Font INTER_REGULAR_ITALIC = FL_HELVETICA_ITALIC;
Fl_Font INTER_MEDIUM_ITALIC = FL_HELVETICA_ITALIC;
Fl_Font INTER_SEMIBOLD_ITALIC = FL_HELVETICA_BOLD_ITALIC;
Fl_Font INTER_BOLD_ITALIC = FL_HELVETICA_BOLD_ITALIC;
Fl_Font INTER_EXTRA_BOLD_ITALIC = FL_HELVETICA_BOLD_ITALIC;
Fl_Font INTER_BLACK_ITALIC = FL_HELVETICA_BOLD_ITALIC;
} // namespace Fonts

bool loadFonts() {
    bool success = true;
    bool loadAllFonts = shouldLoadAllFonts();

#ifdef FONTS_EMBEDDED
    Logger::info("Loading embedded fonts...");
    if (!loadAllFonts) {
        Logger::info("Loading core font weights only (set DISCOVE_LOAD_ALL_FONTS=1 to load all weights).");
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_REGULAR_DATA, EmbeddedFonts::INTER_REGULAR_DATA_size, "Inter")) {
        Fonts::INTER_REGULAR = getFontId("Inter");
    } else {
        Logger::warn("Failed to load embedded Inter Regular");
        success = false;
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_MEDIUM_DATA, EmbeddedFonts::INTER_MEDIUM_DATA_size, "Inter Medium")) {
        Fonts::INTER_MEDIUM = getFontId("Inter Medium");
    } else {
        Logger::warn("Failed to load embedded Inter Medium");
        success = false;
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_SEMIBOLD_DATA, EmbeddedFonts::INTER_SEMIBOLD_DATA_size,
                           "Inter SemiBold")) {
        Fonts::INTER_SEMIBOLD = getFontId("Inter SemiBold");
        Logger::info("INTER_SEMIBOLD assigned font ID: " + std::to_string(Fonts::INTER_SEMIBOLD));
    } else {
        Logger::warn("Failed to load embedded Inter SemiBold");
        success = false;
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_BOLD_DATA, EmbeddedFonts::INTER_BOLD_DATA_size, "Inter Bold")) {
        Fonts::INTER_BOLD = getFontId("Inter Bold");
        Logger::info("INTER_BOLD assigned font ID: " + std::to_string(Fonts::INTER_BOLD));
    } else {
        Logger::warn("Failed to load embedded Inter Bold");
        success = false;
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_REGULAR_ITALIC_DATA, EmbeddedFonts::INTER_REGULAR_ITALIC_DATA_size,
                           "Inter Italic")) {
        Fonts::INTER_REGULAR_ITALIC = getFontId("Inter Italic");
    } else {
        Logger::warn("Failed to load embedded Inter Regular Italic");
        success = false;
    }

    if (loadFontFromMemory(EmbeddedFonts::INTER_BOLD_ITALIC_DATA, EmbeddedFonts::INTER_BOLD_ITALIC_DATA_size,
                           "Inter Bold Italic")) {
        Fonts::INTER_BOLD_ITALIC = getFontId("Inter Bold Italic");
    } else {
        Logger::warn("Failed to load embedded Inter Bold Italic");
        success = false;
    }

    if (loadAllFonts) {
        if (loadFontFromMemory(EmbeddedFonts::INTER_THIN_DATA, EmbeddedFonts::INTER_THIN_DATA_size, "Inter Thin")) {
            Fonts::INTER_THIN = getFontId("Inter Thin");
        } else {
            Logger::warn("Failed to load embedded Inter Thin");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_EXTRA_LIGHT_DATA, EmbeddedFonts::INTER_EXTRA_LIGHT_DATA_size,
                               "Inter ExtraLight")) {
            Fonts::INTER_EXTRA_LIGHT = getFontId("Inter ExtraLight");
        } else {
            Logger::warn("Failed to load embedded Inter ExtraLight");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_LIGHT_DATA, EmbeddedFonts::INTER_LIGHT_DATA_size, "Inter Light")) {
            Fonts::INTER_LIGHT = getFontId("Inter Light");
        } else {
            Logger::warn("Failed to load embedded Inter Light");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_EXTRA_BOLD_DATA, EmbeddedFonts::INTER_EXTRA_BOLD_DATA_size,
                               "Inter ExtraBold")) {
            Fonts::INTER_EXTRA_BOLD = getFontId("Inter ExtraBold");
            Logger::info("INTER_EXTRA_BOLD assigned font ID: " + std::to_string(Fonts::INTER_EXTRA_BOLD));
        } else {
            Logger::warn("Failed to load embedded Inter ExtraBold");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_BLACK_DATA, EmbeddedFonts::INTER_BLACK_DATA_size, "Inter Black")) {
            Fonts::INTER_BLACK = getFontId("Inter Black");
        } else {
            Logger::warn("Failed to load embedded Inter Black");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_THIN_ITALIC_DATA, EmbeddedFonts::INTER_THIN_ITALIC_DATA_size,
                               "Inter Thin Italic")) {
            Fonts::INTER_THIN_ITALIC = getFontId("Inter Thin Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Thin Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_EXTRA_LIGHT_ITALIC_DATA,
                               EmbeddedFonts::INTER_EXTRA_LIGHT_ITALIC_DATA_size, "Inter ExtraLight Italic")) {
            Fonts::INTER_EXTRA_LIGHT_ITALIC = getFontId("Inter ExtraLight Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Extra Light Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_LIGHT_ITALIC_DATA, EmbeddedFonts::INTER_LIGHT_ITALIC_DATA_size,
                               "Inter Light Italic")) {
            Fonts::INTER_LIGHT_ITALIC = getFontId("Inter Light Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Light Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_MEDIUM_ITALIC_DATA, EmbeddedFonts::INTER_MEDIUM_ITALIC_DATA_size,
                               "Inter Medium Italic")) {
            Fonts::INTER_MEDIUM_ITALIC = getFontId("Inter Medium Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Medium Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_SEMIBOLD_ITALIC_DATA,
                               EmbeddedFonts::INTER_SEMIBOLD_ITALIC_DATA_size, "Inter SemiBold Italic")) {
            Fonts::INTER_SEMIBOLD_ITALIC = getFontId("Inter SemiBold Italic");
        } else {
            Logger::warn("Failed to load embedded Inter SemiBold Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_EXTRA_BOLD_ITALIC_DATA,
                               EmbeddedFonts::INTER_EXTRA_BOLD_ITALIC_DATA_size, "Inter ExtraBold Italic")) {
            Fonts::INTER_EXTRA_BOLD_ITALIC = getFontId("Inter ExtraBold Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Extra Bold Italic");
            success = false;
        }

        if (loadFontFromMemory(EmbeddedFonts::INTER_BLACK_ITALIC_DATA, EmbeddedFonts::INTER_BLACK_ITALIC_DATA_size,
                               "Inter Black Italic")) {
            Fonts::INTER_BLACK_ITALIC = getFontId("Inter Black Italic");
        } else {
            Logger::warn("Failed to load embedded Inter Black Italic");
            success = false;
        }
    }

#else
    Logger::info("Loading fonts from files...");
    if (!loadAllFonts) {
        Logger::info("Loading core font weights only (set DISCOVE_LOAD_ALL_FONTS=1 to load all weights).");
    }
    std::string fontDir = "assets/fonts/inter/";

    if (loadFont(fontDir + "400.ttf", "Inter")) {
        Fonts::INTER_REGULAR = getFontId("Inter");
    } else {
        Logger::warn("Failed to load Inter Regular");
        success = false;
    }

    if (loadFont(fontDir + "500.ttf", "Inter Medium")) {
        Fonts::INTER_MEDIUM = getFontId("Inter Medium");
    } else {
        Logger::warn("Failed to load Inter Medium");
        success = false;
    }

    if (loadFont(fontDir + "600.ttf", "Inter SemiBold")) {
        Fonts::INTER_SEMIBOLD = getFontId("Inter SemiBold");
    } else {
        Logger::warn("Failed to load Inter SemiBold");
        success = false;
    }

    if (loadFont(fontDir + "700.ttf", "Inter Bold")) {
        Fonts::INTER_BOLD = getFontId("Inter Bold");
    } else {
        Logger::warn("Failed to load Inter Bold");
        success = false;
    }

    if (loadFont(fontDir + "400italic.ttf", "Inter Italic")) {
        Fonts::INTER_REGULAR_ITALIC = getFontId("Inter Italic");
    } else {
        Logger::warn("Failed to load Inter Regular Italic");
        success = false;
    }

    if (loadFont(fontDir + "700italic.ttf", "Inter Bold Italic")) {
        Fonts::INTER_BOLD_ITALIC = getFontId("Inter Bold Italic");
    } else {
        Logger::warn("Failed to load Inter Bold Italic");
        success = false;
    }

    if (loadAllFonts) {
        if (loadFont(fontDir + "100.ttf", "Inter Thin")) {
            Fonts::INTER_THIN = getFontId("Inter Thin");
        } else {
            Logger::warn("Failed to load Inter Thin");
            success = false;
        }

        if (loadFont(fontDir + "200.ttf", "Inter ExtraLight")) {
            Fonts::INTER_EXTRA_LIGHT = getFontId("Inter ExtraLight");
        } else {
            Logger::warn("Failed to load Inter ExtraLight");
            success = false;
        }

        if (loadFont(fontDir + "300.ttf", "Inter Light")) {
            Fonts::INTER_LIGHT = getFontId("Inter Light");
        } else {
            Logger::warn("Failed to load Inter Light");
            success = false;
        }

        if (loadFont(fontDir + "800.ttf", "Inter ExtraBold")) {
            Fonts::INTER_EXTRA_BOLD = getFontId("Inter ExtraBold");
        } else {
            Logger::warn("Failed to load Inter ExtraBold");
            success = false;
        }

        if (loadFont(fontDir + "900.ttf", "Inter Black")) {
            Fonts::INTER_BLACK = getFontId("Inter Black");
        } else {
            Logger::warn("Failed to load Inter Black");
            success = false;
        }

        if (loadFont(fontDir + "100italic.ttf", "Inter Thin Italic")) {
            Fonts::INTER_THIN_ITALIC = getFontId("Inter Thin Italic");
        } else {
            Logger::warn("Failed to load Inter Thin Italic");
            success = false;
        }

        if (loadFont(fontDir + "200italic.ttf", "Inter ExtraLight Italic")) {
            Fonts::INTER_EXTRA_LIGHT_ITALIC = getFontId("Inter ExtraLight Italic");
        } else {
            Logger::warn("Failed to load Inter Extra Light Italic");
            success = false;
        }

        if (loadFont(fontDir + "300italic.ttf", "Inter Light Italic")) {
            Fonts::INTER_LIGHT_ITALIC = getFontId("Inter Light Italic");
        } else {
            Logger::warn("Failed to load Inter Light Italic");
            success = false;
        }

        if (loadFont(fontDir + "500italic.ttf", "Inter Medium Italic")) {
            Fonts::INTER_MEDIUM_ITALIC = getFontId("Inter Medium Italic");
        } else {
            Logger::warn("Failed to load Inter Medium Italic");
            success = false;
        }

        if (loadFont(fontDir + "600italic.ttf", "Inter SemiBold Italic")) {
            Fonts::INTER_SEMIBOLD_ITALIC = getFontId("Inter SemiBold Italic");
        } else {
            Logger::warn("Failed to load Inter SemiBold Italic");
            success = false;
        }

        if (loadFont(fontDir + "800italic.ttf", "Inter ExtraBold Italic")) {
            Fonts::INTER_EXTRA_BOLD_ITALIC = getFontId("Inter ExtraBold Italic");
        } else {
            Logger::warn("Failed to load Inter Extra Bold Italic");
            success = false;
        }

        if (loadFont(fontDir + "900italic.ttf", "Inter Black Italic")) {
            Fonts::INTER_BLACK_ITALIC = getFontId("Inter Black Italic");
        } else {
            Logger::warn("Failed to load Inter Black Italic");
            success = false;
        }
    }

    Logger::info("Notifying Windows of font changes...");
    SendMessageTimeout(HWND_BROADCAST, WM_FONTCHANGE, 0, 0, SMTO_ABORTIFHUNG, 1000, NULL);
#endif

    return success;
}

Fl_Font getFontId(const std::string &name) {
    auto it = fontMap.find(name);
    if (it != fontMap.end()) {
        return it->second;
    }
    return FL_HELVETICA;
}

} // namespace FontLoader
