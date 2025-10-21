#include <iostream>
#include <string>
#include <fstream>
#include <windows.h>
#include <direct.h>
#include <vector>
#include <map>

// Resource ID for embedded icon
#define IDR_ICON 101

// TOML Configuration Structure
struct TomlConfig {
    std::string name = "unnamed_project";
    std::string version = "0.1.0";
    std::string platform = "win";
    std::string type = "bin";
    std::string icon = "icon.ico";
    
    struct Profile {
        int optLevel = 0;
        bool debug = true;
        int codegenUnits = 1;
        std::string lto = "off";
    };
    
    Profile dev;
    Profile release;
};

std::string getHostPlatform() {
    #ifdef _WIN32
        return "win";
    #elif __linux__
        return "lin";
    #elif __APPLE__
        return "mac";
    #else
        return "unknown";
    #endif
}

bool createDirectory(const std::string& path) {
    return CreateDirectoryA(path.c_str(), NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool directoryExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool fileExists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string removeQuotes(const std::string& str) {
    std::string result = trim(str);
    if (result.length() >= 2 && result.front() == '"' && result.back() == '"') {
        return result.substr(1, result.length() - 2);
    }
    return result;
}

std::string removeComments(const std::string& line) {
    size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
        return line.substr(0, commentPos);
    }
    return line;
}

TomlConfig parseToml(const std::string& filename) {
    TomlConfig config;
    
    // Set defaults
    config.dev.optLevel = 0;
    config.dev.debug = true;
    config.dev.codegenUnits = 4;
    config.dev.lto = "off";
    
    config.release.optLevel = 3;
    config.release.debug = false;
    config.release.codegenUnits = 1;
    config.release.lto = "fat";
    
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Warning: Could not open cclank.toml, using defaults" << std::endl;
        return config;
    }
    
    std::string currentSection = "";
    std::string line;
    
    while (std::getline(file, line)) {
        line = removeComments(line);
        line = trim(line);
        
        if (line.empty()) continue;
        
        // Parse section headers [section] or [section.subsection]
        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key = value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = trim(line.substr(0, eqPos));
            std::string value = trim(line.substr(eqPos + 1));
            value = removeQuotes(value);
            
            // Package section
            if (currentSection == "package") {
                if (key == "name") config.name = value;
                else if (key == "version") config.version = value;
                else if (key == "platform") config.platform = value;
                else if (key == "type") config.type = value;
                else if (key == "icon") config.icon = value;
            }
            // Dev profile
            else if (currentSection == "profile.dev") {
                if (key == "opt-level") config.dev.optLevel = std::stoi(value);
                else if (key == "debug") config.dev.debug = (value == "true");
                else if (key == "codegen-units") config.dev.codegenUnits = std::stoi(value);
                else if (key == "lto") config.dev.lto = value;
            }
            // Release profile
            else if (currentSection == "profile.release") {
                if (key == "opt-level") config.release.optLevel = std::stoi(value);
                else if (key == "debug") config.release.debug = (value == "true");
                else if (key == "codegen-units") config.release.codegenUnits = std::stoi(value);
                else if (key == "lto") config.release.lto = value;
            }
        }
    }
    
    file.close();
    return config;
}

std::string getOutputFilename(const std::string& name, const std::string& type, const std::string& platform) {
    if (type == "bin") {
        if (platform == "win") return name + ".exe";
        return name;  // Linux/Mac have no extension
    }
    else if (type == "lib") {
        if (platform == "win") return name + ".lib";
        return "lib" + name + ".a";  // Linux/Mac static library with lib prefix
    }
    else if (type == "dylib" || type == "dll" || type == "so") {
        if (platform == "win") return name + ".dll";
        if (platform == "mac") return "lib" + name + ".dylib";
        return "lib" + name + ".so";  // Linux shared library
    }
    return name + ".exe";  // Default
}

std::vector<std::string> findCppFiles(const std::string& directory) {
    std::vector<std::string> files;
    WIN32_FIND_DATAA findData;
    std::string searchPath = directory + "\\*.cpp";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(directory + "\\" + findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }
    
    return files;
}

std::string buildCommand(const TomlConfig& config, bool isRelease, const std::vector<std::string>& sourceFiles) {
    const TomlConfig::Profile& profile = isRelease ? config.release : config.dev;
    std::string profileName = isRelease ? "release" : "debug";
    
    std::string cmd = "g++";
    
    // Optimization level
    if (profile.optLevel == 0) cmd += " -O0";
    else if (profile.optLevel == 1) cmd += " -O1";
    else if (profile.optLevel == 2) cmd += " -O2";
    else if (profile.optLevel == 3) cmd += " -O3";
    
    // Debug info
    if (profile.debug) {
        cmd += " -g";
    }
    
    // LTO
    if (profile.lto == "fat") {
        cmd += " -flto";
    }
    
    // Static library needs compile-only flag
    if (config.type == "lib") {
        cmd += " -c";
    }
    
    // Dynamic library needs shared flag and PIC for non-Windows
    if (config.type == "dylib" || config.type == "dll" || config.type == "so") {
        cmd += " -shared";
        if (config.platform != "win") {
            cmd += " -fPIC";
        }
    }
    
    // Source files
    for (const auto& file : sourceFiles) {
        cmd += " " + file;
    }
    
    // Icon resource (only for Windows binaries)
    if (config.platform == "win" && config.type == "bin" && fileExists("icon.ico")) {
        cmd += " resource.o";
    }
    
    // Output file
    std::string outputFilename = getOutputFilename(config.name, config.type, config.platform);
    
    // For static libraries, output object files to build directory
    if (config.type == "lib") {
        cmd += " -o build/" + profileName + "/";
        // We'll handle ar separately
    } else {
        cmd += " -o build/" + profileName + "/" + outputFilename;
    }
    
    // Platform-specific linking flags (not for libraries being created)
    if (config.platform == "win" && config.type == "bin") {
        cmd += " -static -lshlwapi";
    }
    
    return cmd;
}

bool extractEmbeddedIcon(const std::string& outputPath) {
    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(IDR_ICON), RT_RCDATA);
    
    if (!hResource) {
        std::cerr << "Error: Could not find embedded icon resource" << std::endl;
        return false;
    }
    
    HGLOBAL hLoadedResource = LoadResource(hModule, hResource);
    if (!hLoadedResource) {
        std::cerr << "Error: Could not load icon resource" << std::endl;
        return false;
    }
    
    void* pResourceData = LockResource(hLoadedResource);
    DWORD dwResourceSize = SizeofResource(hModule, hResource);
    
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Could not create icon file: " << outputPath << std::endl;
        return false;
    }
    
    outFile.write(static_cast<const char*>(pResourceData), dwResourceSize);
    outFile.close();
    
    return true;
}

void createDefaultToml(const std::string& projectPath, const std::string& projectName) {
    std::string tomlPath = projectPath + "/cclank.toml";
    std::ofstream toml(tomlPath);
    
    if (!toml) {
        std::cerr << "Error: Could not create cclank.toml" << std::endl;
        return;
    }
    
    toml << "[package]\n";
    toml << "name = \"" << projectName << "\"\n";
    toml << "version = \"0.1.0\"\n";
    toml << "platform = \"win\"\n";
    toml << "type = \"bin\"\n";
    toml << "icon = \"icon.ico\"\n";
    toml << "\n";
    toml << "[features]\n";
    toml << "\n";
    toml << "[profile.dev]\n";
    toml << "opt-level = 0\n";
    toml << "debug = true\n";
    toml << "codegen-units = 4\n";
    toml << "\n";
    toml << "[profile.release]\n";
    toml << "opt-level = 3\n";
    toml << "debug = false\n";
    toml << "lto = \"fat\"\n";
    toml << "codegen-units = 1\n";
    
    toml.close();
    std::cout << "Created cclank.toml" << std::endl;
}

void createMainCpp(const std::string& srcPath, const std::string& projectName) {
    std::string mainPath = srcPath + "/main.cpp";
    std::ofstream main(mainPath);
    
    if (!main) {
        std::cerr << "Error: Could not create main.cpp" << std::endl;
        return;
    }
    
    main << "#include <iostream>\n";
    main << "\n";
    main << "int main() {\n";
    main << "    std::cout << \"Hello from " << projectName << "!\" << std::endl;\n";
    main << "    return 0;\n";
    main << "}\n";
    
    main.close();
    std::cout << "Created src/main.cpp" << std::endl;
}

void createNewProject(const std::string& projectName) {
    // Check if directory already exists
    if (directoryExists(projectName)) {
        std::cerr << "Error: Directory '" << projectName << "' already exists" << std::endl;
        return;
    }
    
    // Create project directory
    if (!createDirectory(projectName)) {
        std::cerr << "Error: Could not create directory '" << projectName << "'" << std::endl;
        return;
    }
    std::cout << "Created project directory: " << projectName << std::endl;
    
    // Create src directory
    std::string srcPath = projectName + "/src";
    if (!createDirectory(srcPath)) {
        std::cerr << "Error: Could not create src/ directory" << std::endl;
        return;
    }
    std::cout << "Created src/ directory" << std::endl;
    
    // Extract and save embedded icon
    std::string iconPath = projectName + "/icon.ico";
    if (extractEmbeddedIcon(iconPath)) {
        std::cout << "Created icon.ico" << std::endl;
    }
    
    // Create cclank.toml
    createDefaultToml(projectName, projectName);
    
    // Create main.cpp
    createMainCpp(srcPath, projectName);
    
    std::cout << "\nProject '" << projectName << "' created successfully!" << std::endl;
    std::cout << "Next steps:" << std::endl;
    std::cout << "  cd " << projectName << std::endl;
    std::cout << "  cclank build" << std::endl;
}

void buildProject(bool isRelease) {
    // Check if cclank.toml exists
    if (!fileExists("cclank.toml")) {
        std::cerr << "Error: cclank.toml not found. Are you in a cclank project directory?" << std::endl;
        return;
    }
    
    // Parse configuration
    TomlConfig config = parseToml("cclank.toml");
    std::string profileName = isRelease ? "release" : "debug";
    std::string hostPlatform = getHostPlatform();
    
    std::cout << "Building " << config.name << " (" << profileName << " profile, " 
              << config.type << " for " << config.platform << ")..." << std::endl;
    
    // Cross-compilation warning for binaries and dynamic libraries
    if (config.platform != hostPlatform) {
        if (config.type == "bin") {
            std::cerr << "\nWarning: Cross-compilation is not available for binaries!" << std::endl;
            std::cerr << "Project platform: " << config.platform << std::endl;
            std::cerr << "Host platform: " << hostPlatform << std::endl;
            std::cerr << "The resulting binary will only run on " << hostPlatform << ", not " << config.platform << std::endl;
            std::cerr << "To build for " << config.platform << ", use a " << config.platform << " system.\n" << std::endl;
        }
        else if (config.type == "dylib" || config.type == "dll" || config.type == "so") {
            std::cerr << "\nWarning: Cross-compilation is not available for dynamic libraries!" << std::endl;
            std::cerr << "Project platform: " << config.platform << std::endl;
            std::cerr << "Host platform: " << hostPlatform << std::endl;
            std::cerr << "The resulting library will only work on " << hostPlatform << ", not " << config.platform << std::endl;
            std::cerr << "To build for " << config.platform << ", use a " << config.platform << " system.\n" << std::endl;
        }
        else if (config.type == "lib") {
            std::cout << "\nNote: Building static library with platform set to " << config.platform << std::endl;
            std::cout << "Static libraries (.a, .lib) may be platform-specific depending on code." << std::endl;
            std::cout << "For true cross-platform compatibility, build on the target platform.\n" << std::endl;
        }
    }
    
    // Find all .cpp files in src/
    std::vector<std::string> sourceFiles = findCppFiles("src");
    if (sourceFiles.empty()) {
        std::cerr << "Error: No .cpp files found in src/ directory" << std::endl;
        return;
    }
    
    std::cout << "Found " << sourceFiles.size() << " source file(s)" << std::endl;
    
    // Create build directories
    if (!createDirectory("build")) {
        std::cerr << "Error: Could not create build directory" << std::endl;
        return;
    }
    
    std::string buildPath = "build/" + profileName;
    if (!createDirectory(buildPath)) {
        std::cerr << "Error: Could not create " << buildPath << " directory" << std::endl;
        return;
    }
    
    // Generate resource file if icon exists and type is bin
    if (config.type == "bin" && config.platform == "win" && fileExists(config.icon)) {
        std::cout << "Compiling icon resource..." << std::endl;
        
        // Create resource.rc
        std::ofstream rc("resource.rc");
        rc << "#include <windows.h>\n";
        rc << "IDI_ICON1 ICON \"" << config.icon << "\"\n";
        rc.close();
        
        // Compile resource
        std::string rcCmd = "windres resource.rc -O coff -o resource.o";
        int rcResult = system(rcCmd.c_str());
        if (rcResult != 0) {
            std::cerr << "Warning: Icon resource compilation failed" << std::endl;
        }
    }
    
    // Build the project
    if (config.type == "lib") {
        // Static library: compile to object files, then use ar
        std::cout << "Compiling object files..." << std::endl;
        
        std::vector<std::string> objectFiles;
        bool compileFailed = false;
        
        for (const auto& srcFile : sourceFiles) {
            // Extract filename without path
            size_t lastSlash = srcFile.find_last_of("\\/");
            std::string filename = (lastSlash != std::string::npos) ? srcFile.substr(lastSlash + 1) : srcFile;
            std::string objName = filename.substr(0, filename.find_last_of('.')) + ".o";
            std::string objPath = buildPath + "/" + objName;
            
            std::string compileCmd = buildCommand(config, isRelease, {srcFile}) + objName;
            std::cout << "  Compiling " << filename << "..." << std::endl;
            
            int result = system(compileCmd.c_str());
            if (result != 0) {
                std::cerr << "Error: Compilation failed for " << filename << std::endl;
                compileFailed = true;
                break;
            }
            
            objectFiles.push_back(objPath);
        }
        
        if (!compileFailed && !objectFiles.empty()) {
            // Create static library with ar
            std::string outputFilename = getOutputFilename(config.name, config.type, config.platform);
            std::string libPath = buildPath + "/" + outputFilename;
            
            std::string arCmd = "ar rcs " + libPath;
            for (const auto& obj : objectFiles) {
                arCmd += " " + obj;
            }
            
            std::cout << "Creating static library..." << std::endl;
            std::cout << "Running: " << arCmd << std::endl;
            
            int result = system(arCmd.c_str());
            if (result == 0) {
                std::cout << "Build successful!" << std::endl;
                std::cout << "Output: " << libPath << std::endl;
            } else {
                std::cerr << "Build failed!" << std::endl;
            }
        }
    } else {
        // Binary or dynamic library: compile and link in one step
        std::string cmd = buildCommand(config, isRelease, sourceFiles);
        std::cout << "Running: " << cmd << std::endl;
        
        int result = system(cmd.c_str());
        
        if (result == 0) {
            std::cout << "Build successful!" << std::endl;
            std::string outputFilename = getOutputFilename(config.name, config.type, config.platform);
            std::cout << "Output: build/" << profileName << "/" << outputFilename << std::endl;
        } else {
            std::cerr << "Build failed!" << std::endl;
        }
    }
    
    // Clean up temporary files
    if (fileExists("resource.rc")) {
        DeleteFileA("resource.rc");
    }
    if (fileExists("resource.o")) {
        DeleteFileA("resource.o");
    }
}

void runProject(bool isRelease) {
    // Check if cclank.toml exists to get config
    if (!fileExists("cclank.toml")) {
        std::cerr << "Error: cclank.toml not found. Are you in a cclank project directory?" << std::endl;
        return;
    }
    
    TomlConfig config = parseToml("cclank.toml");
    
    // Only binaries can be run
    if (config.type != "bin") {
        std::cerr << "Error: Cannot run non-binary project (type = " << config.type << ")" << std::endl;
        std::cerr << "Only projects with type = \"bin\" can be executed" << std::endl;
        return;
    }
    
    // Check if platform matches host
    std::string hostPlatform = getHostPlatform();
    if (config.platform != hostPlatform) {
        std::cerr << "Error: Cannot use 'run' when the platform doesn't match the host platform" << std::endl;
        std::cerr << "Project platform: " << config.platform << std::endl;
        std::cerr << "Host platform: " << hostPlatform << std::endl;
        std::cerr << "Change platform to \"" << hostPlatform << "\" in cclank.toml to run on this system" << std::endl;
        return;
    }
    
    std::string profileName = isRelease ? "release" : "debug";
    std::string outputFilename = getOutputFilename(config.name, config.type, config.platform);
    std::string exePath = "build\\" + profileName + "\\" + outputFilename;
    
    // Check if the executable exists, if not build it first
    if (!fileExists(exePath)) {
        std::cout << "Executable not found, building first..." << std::endl;
        buildProject(isRelease);
        
        // Check again after build
        if (!fileExists(exePath)) {
            std::cerr << "Error: Build failed, executable not found" << std::endl;
            return;
        }
    }
    
    // Run the executable in the same terminal using quotes
    std::cout << "Running " << exePath << "...\n" << std::endl;
    std::string runCmd = "\"" + exePath + "\"";
    system(runCmd.c_str());
}

void cleanProject() {
    if (!directoryExists("build")) {
        std::cout << "Nothing to clean (build directory doesn't exist)" << std::endl;
        return;
    }
    
    std::cout << "Cleaning build directory..." << std::endl;
    
    // Remove build directory recursively
    std::string cmd = "rmdir /s /q build";
    int result = system(cmd.c_str());
    
    if (result == 0) {
        std::cout << "Clean successful!" << std::endl;
    } else {
        std::cerr << "Clean failed!" << std::endl;
    }
}

void printUsage() {
    std::cout << "cclank - A C++ build and project manager inspired by Rusts build system.\n" << std::endl;
    std::cout << "Usage:" << std::endl;
    std::cout << "  cclank new <n>        Create new project with default structure" << std::endl;
    std::cout << "  cclank build             Build using dev profile" << std::endl;
    std::cout << "  cclank build --release   Build using release profile" << std::endl;
    std::cout << "  cclank run               Build and run (dev profile)" << std::endl;
    std::cout << "  cclank run --release     Build and run (release profile)" << std::endl;
    std::cout << "  cclank clean             Remove build directory" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "new") {
        if (argc < 3) {
            std::cerr << "Error: Project name required" << std::endl;
            std::cout << "Usage: cclank new <n>" << std::endl;
            return 1;
        }
        std::string projectName = argv[2];
        createNewProject(projectName);
    }
    else if (command == "build") {
        bool isRelease = (argc > 2 && std::string(argv[2]) == "--release");
        buildProject(isRelease);
    }
    else if (command == "run") {
        bool isRelease = (argc > 2 && std::string(argv[2]) == "--release");
        runProject(isRelease);
    }
    else if (command == "clean") {
        cleanProject();
    }
    else {
        std::cerr << "Error: Unknown command '" << command << "'" << std::endl;
        printUsage();
        return 1;
    }
    
    return 0;
}