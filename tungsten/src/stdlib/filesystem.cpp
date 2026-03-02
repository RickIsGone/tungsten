#include <filesystem>
#include <cstring>
#include <cstdint>

namespace fs = std::filesystem;

extern "C" {

// Utility to allocate strings (necessary because .data() is not safe)
static char* allocateString(const std::string& str) {
   char* buffer = new char[str.length() + 1];
   strcpy(buffer, str.c_str());
   return buffer;
}

void freeString(char* str) {
   delete[] str;
}

// Copy operations
void copy(const char* from, const char* to) {
   fs::copy(from, to);
}

bool copyFile(const char* from, const char* to) {
   return fs::copy_file(from, to);
}

bool copyDirectory(const char* from, const char* to, bool recursive) {
   try {
      auto options = recursive ? fs::copy_options::recursive : fs::copy_options::none;
      fs::copy(from, to, options);
      return true;
   } catch (...) {
      return false;
   }
}

bool copySymlink(const char* from, const char* to) {
   try {
      fs::copy_symlink(from, to);
      return true;
   } catch (...) {
      return false;
   }
}

// Directory creation
bool createDirectory(const char* path) {
   return fs::create_directory(path);
}

bool createDirectories(const char* path) {
   return fs::create_directories(path);
}

bool createHardLink(const char* target, const char* link) {
   try {
      fs::create_hard_link(target, link);
      return true;
   } catch (...) {
      return false;
   }
}

bool createSymlink(const char* target, const char* link) {
   try {
      fs::create_symlink(target, link);
      return true;
   } catch (...) {
      return false;
   }
}

bool createDirectorySymlink(const char* target, const char* link) {
   try {
      fs::create_directory_symlink(target, link);
      return true;
   } catch (...) {
      return false;
   }
}

// Existence and type checks
bool exists(const char* path) {
   return fs::exists(path);
}

bool isDirectory(const char* path) {
   return fs::is_directory(path);
}

bool isRegularFile(const char* path) {
   return fs::is_regular_file(path);
}

bool isSymlink(const char* path) {
   return fs::is_symlink(path);
}

bool isEmpty(const char* path) {
   return fs::is_empty(path);
}

bool isBlockFile(const char* path) {
   return fs::is_block_file(path);
}

bool isCharacterFile(const char* path) {
   return fs::is_character_file(path);
}

bool isFifo(const char* path) {
   return fs::is_fifo(path);
}

bool isSocket(const char* path) {
   return fs::is_socket(path);
}

bool isOther(const char* path) {
   return fs::is_other(path);
}

// Removal operations
bool __remove(const char* path) {
   return fs::remove(path);
}

uint64_t removeAll(const char* path) {
   return fs::remove_all(path);
}

// Rename and move
void __rename(const char* oldPath, const char* newPath) {
   fs::rename(oldPath, newPath);
}

// File information
uint64_t fileSize(const char* path) {
   return fs::file_size(path);
}

uint64_t hardLinkCount(const char* path) {
   return fs::hard_link_count(path);
}

uint64_t lastWriteTime(const char* path) {
   auto ftime = fs::last_write_time(path);
   return static_cast<uint64_t>(ftime.time_since_epoch().count());
}

bool setLastWriteTime(const char* path, uint64_t time) {
   try {
      fs::file_time_type ftime{fs::file_time_type::duration(time)};
      fs::last_write_time(path, ftime);
      return true;
   } catch (...) {
      return false;
   }
}

// File operations
bool resizeFile(const char* path, uint64_t newSize) {
   try {
      fs::resize_file(path, newSize);
      return true;
   } catch (...) {
      return false;
   }
}

bool changePermissions(const char* path, int permissions) {
   try {
      fs::permissions(path, static_cast<fs::perms>(permissions));
      return true;
   } catch (...) {
      return false;
   }
}

// Path operations
char* absolute(const char* path) {
   return allocateString(fs::absolute(path).string());
}

char* canonical(const char* path) {
   return allocateString(fs::canonical(path).string());
}

char* weaklyCanonical(const char* path) {
   return allocateString(fs::weakly_canonical(path).string());
}

char* relativePath(const char* path, const char* base) {
   return allocateString(fs::relative(path, base).string());
}

char* proximate(const char* path, const char* base) {
   return allocateString(fs::proximate(path, base).string());
}

// Path components
char* parentPath(const char* path) {
   return allocateString(fs::path(path).parent_path().string());
}

char* filename(const char* path) {
   return allocateString(fs::path(path).filename().string());
}

char* extension(const char* path) {
   return allocateString(fs::path(path).extension().string());
}

char* stem(const char* path) {
   return allocateString(fs::path(path).stem().string());
}

char* rootName(const char* path) {
   return allocateString(fs::path(path).root_name().string());
}

char* rootDirectory(const char* path) {
   return allocateString(fs::path(path).root_directory().string());
}

char* rootPath(const char* path) {
   return allocateString(fs::path(path).root_path().string());
}

char* relativePathComponent(const char* path) {
   return allocateString(fs::path(path).relative_path().string());
}

// Path property checks
bool hasRootName(const char* path) {
   return fs::path(path).has_root_name();
}

bool hasRootDirectory(const char* path) {
   return fs::path(path).has_root_directory();
}

bool hasRootPath(const char* path) {
   return fs::path(path).has_root_path();
}

bool hasRelativePath(const char* path) {
   return fs::path(path).has_relative_path();
}

bool hasParentPath(const char* path) {
   return fs::path(path).has_parent_path();
}

bool hasFilename(const char* path) {
   return fs::path(path).has_filename();
}

bool hasStem(const char* path) {
   return fs::path(path).has_stem();
}

bool hasExtension(const char* path) {
   return fs::path(path).has_extension();
}

bool isAbsolute(const char* path) {
   return fs::path(path).is_absolute();
}

bool isRelative(const char* path) {
   return fs::path(path).is_relative();
}

// Current path operations
char* currentPath() {
   return allocateString(fs::current_path().string());
}

void setCurrentPath(const char* path) {
   fs::current_path(path);
}

// Temporary directory
char* tempDirectoryPath() {
   return allocateString(fs::temp_directory_path().string());
}

// Symlink operations
char* readSymlink(const char* path) {
   return allocateString(fs::read_symlink(path).string());
}

// Disk space information
bool getSpaceInfo(const char* path, uint64_t* capacity, uint64_t* free, uint64_t* available) {
   try {
      auto space = fs::space(path);
      *capacity = static_cast<uint64_t>(space.capacity);
      *free = static_cast<uint64_t>(space.free);
      *available = static_cast<uint64_t>(space.available);
      return true;
   } catch (...) {
      return false;
   }
}

// Path equivalence
bool equivalent(const char* p1, const char* p2) {
   return fs::equivalent(p1, p2);
}
}
