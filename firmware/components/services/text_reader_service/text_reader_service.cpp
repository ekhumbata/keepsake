#include "text_reader_service.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "epub_engine/epub_book.h"
#include "esp_log.h"
#include "library/library.h"
#include "sdkconfig.h"
#include "storage_service.h"

namespace text_reader_service {
namespace {

constexpr const char* kTag = "TextReaderService";
constexpr const char* kPositionSuffix = ".pos";
constexpr const char* kBookmarksSuffix = ".bm";
constexpr size_t kMaxBookmarksBytes = 64u * 1024u;
constexpr const char* kIndexFileName = ".library";
// Cover thumbnails (#27), one small file per book and size; hidden, so the scan skips it.
constexpr const char* kCoverCacheFolder = ".cache";
constexpr size_t kMaxCoverImageBytes = 4u * 1024u * 1024u;
constexpr size_t kMaxCoverCacheBytes = 64u * 1024u;
constexpr int kMaxFolderDepth = 4;
constexpr size_t kMaxBooks = 2000;
constexpr size_t kMaxIndexBytes = 512u * 1024u;

std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == '/') {
        return left + right;
    }
    return left + "/" + right;
}

std::string FolderPath()
{
    return JoinPath(storage_service::MountPoint(), CONFIG_CHRONINKLE_TEXT_READER_FOLDER);
}

std::string BookPath(const std::string& filename)
{
    return JoinPath(FolderPath(), filename);
}

std::string PositionSidecarPath(const std::string& filename)
{
    return BookPath(filename) + kPositionSuffix;
}

std::string BookmarksSidecarPath(const std::string& filename)
{
    return BookPath(filename) + kBookmarksSuffix;
}

bool HasExtension(const std::string& name, const char* wanted)
{
    const size_t length = std::strlen(wanted);
    if (name.size() <= length) {
        return false;
    }
    std::string ext = name.substr(name.size() - length);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == wanted;
}

bool HasBookExtension(const std::string& name)
{
    return HasExtension(name, ".txt") || HasExtension(name, ".epub");
}

// Filters out dotfiles: macOS in particular litters any folder it writes to over OTG/USB
// mass storage with ".DS_Store" and, for every real file it copies onto a non-HFS+ volume
// like this SD card's FAT32, a same-named "._<file>" AppleDouble resource-fork sidecar --
// which would otherwise pass HasBookExtension outright (e.g. "._Dracula.txt").
bool IsHiddenName(const std::string& name)
{
    return !name.empty() && name.front() == '.';
}

}  // namespace

esp_err_t Init()
{
    const std::string folder = FolderPath();
    errno = 0;
    if (mkdir(folder.c_str(), 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGW(kTag, "Create folder failed: path=%s errno=%d (%s)", folder.c_str(), errno,
             std::strerror(errno));
    return ESP_FAIL;
}

namespace {

void ScanFolder(const std::string& root, const std::string& relative, int depth,
                std::vector<library::BookRecord>* found)
{
    const std::string folder = relative.empty() ? root : JoinPath(root, relative);
    errno = 0;
    DIR* dir = opendir(folder.c_str());
    if (dir == nullptr) {
        if (errno != ENOENT) {
            ESP_LOGW(kTag, "opendir(%s) failed: errno=%d", folder.c_str(), errno);
        }
        return;
    }
    while (found->size() < kMaxBooks) {
        errno = 0;
        struct dirent* entry = readdir(dir);
        if (entry == nullptr) {
            if (errno != 0) {
                ESP_LOGW(kTag, "readdir(%s) failed: errno=%d", folder.c_str(), errno);
            }
            break;
        }
        const std::string name = entry->d_name;
        if (IsHiddenName(name)) {
            continue;
        }
        const std::string child = relative.empty() ? name : JoinPath(relative, name);
        struct stat st = {};
        if (stat(JoinPath(root, child).c_str(), &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (depth < kMaxFolderDepth) {
                ScanFolder(root, child, depth + 1, found);
            }
            continue;
        }
        if (!HasBookExtension(name)) {
            continue;
        }
        library::BookRecord record;
        record.path = child;
        record.size_bytes = static_cast<uint64_t>(st.st_size);
        record.modified_time = static_cast<int64_t>(st.st_mtime);
        found->push_back(std::move(record));
    }
    closedir(dir);
}

std::string IndexPath()
{
    return JoinPath(FolderPath(), kIndexFileName);
}

std::vector<library::BookRecord> LoadIndex()
{
    FILE* file = std::fopen(IndexPath().c_str(), "rb");
    if (file == nullptr) {
        return {};
    }
    std::string text;
    char buffer[1024];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0 && text.size() < kMaxIndexBytes) {
        text.append(buffer, read);
    }
    std::fclose(file);
    return library::ParseIndex(text);
}

void SaveIndex(const std::vector<library::BookRecord>& records)
{
    const std::string text = library::SerializeIndex(records);
    FILE* file = std::fopen(IndexPath().c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Saving the library index failed");
        return;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
}

// Title/author for a book the index doesn't know yet (or that changed on disk).
void FillMetadata(library::BookRecord* record)
{
    record->title = library::TitleFromPath(record->path);
    record->author.clear();
    if (library::Extension(record->path) != "epub") {
        return;
    }
    epub::OpenError error = epub::OpenError::kNone;
    const auto book = epub::EpubBook::Open(
        std::make_unique<epub::FileByteSource>(BookPath(record->path)), &error);
    if (book == nullptr) {
        return;  // locked/damaged: keep the file-name title; the reader explains when opened
    }
    if (!book->metadata().title.empty()) {
        record->title = book->metadata().title;
    }
    record->author = book->metadata().author;
}

std::vector<library::BookRecord> RefreshIndex()
{
    std::vector<library::BookRecord> found;
    ScanFolder(FolderPath(), "", 0, &found);
    std::map<std::string, library::BookRecord> cached;
    for (library::BookRecord& record : LoadIndex()) {
        cached[record.path] = std::move(record);
    }
    bool changed = found.size() != cached.size();
    for (library::BookRecord& record : found) {
        const auto it = cached.find(record.path);
        if (it != cached.end() && it->second.size_bytes == record.size_bytes &&
            it->second.modified_time == record.modified_time) {
            record = it->second;  // unchanged: keep title, author, progress
            continue;
        }
        if (it != cached.end()) {
            record.last_opened = it->second.last_opened;  // replaced file: keep its place
        }
        FillMetadata(&record);
        changed = true;
    }
    if (changed) {
        SaveIndex(found);
    }
    return found;
}

}  // namespace

std::vector<BookEntry> ListBooks()
{
    std::vector<library::BookRecord> records = RefreshIndex();
    library::SortForShelf(&records);
    std::vector<BookEntry> books;
    books.reserve(records.size());
    for (library::BookRecord& record : records) {
        books.push_back({.filename = std::move(record.path),
                         .size_bytes = record.size_bytes,
                         .title = std::move(record.title),
                         .author = std::move(record.author),
                         .progress_percent = record.progress_percent,
                         .last_opened = record.last_opened});
    }
    return books;
}

void RecordOpened(const std::string& filename, int progress_percent)
{
    std::vector<library::BookRecord> records = LoadIndex();
    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    for (library::BookRecord& record : records) {
        if (record.path == filename) {
            // A clock that was never set reads as 1970; still order by "most recent" sensibly.
            record.last_opened = std::max<int64_t>(now, record.last_opened + 1);
            record.progress_percent = std::clamp(progress_percent, -1, 100);
            SaveIndex(records);
            return;
        }
    }
    // Not indexed yet (opened before the list was ever built): rebuild, then record.
    records = RefreshIndex();
    for (library::BookRecord& record : records) {
        if (record.path == filename) {
            record.last_opened = std::max<int64_t>(now, 1);
            record.progress_percent = std::clamp(progress_percent, -1, 100);
            SaveIndex(records);
            return;
        }
    }
}

size_t ReadChunk(const std::string& filename, size_t offset, char* buffer, size_t length)
{
    if (buffer == nullptr || length == 0) {
        return 0;
    }

    FILE* file = std::fopen(BookPath(filename).c_str(), "rb");
    if (file == nullptr) {
        return 0;
    }

    size_t bytes_read = 0;
    if (std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0) {
        bytes_read = std::fread(buffer, 1, length, file);
    }
    std::fclose(file);
    return bytes_read;
}

uint64_t GetFileSize(const std::string& filename)
{
    struct stat st = {};
    if (stat(BookPath(filename).c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(st.st_size);
}

size_t LoadPosition(const std::string& filename)
{
    FILE* file = std::fopen(PositionSidecarPath(filename).c_str(), "rb");
    if (file == nullptr) {
        return 0;
    }
    char text[32] = {};
    const size_t bytes_read = std::fread(text, 1, sizeof(text) - 1, file);
    std::fclose(file);
    text[bytes_read] = '\0';

    const long parsed = std::strtol(text, nullptr, 10);
    if (parsed <= 0) {
        return 0;
    }

    // Clamp against the file's current size: covers the book having been replaced or
    // truncated externally since the position was last saved. The reader re-paginates
    // from offset 0 in that case rather than trying to resume mid-word or past EOF.
    const uint64_t size_bytes = GetFileSize(filename);
    const size_t offset = static_cast<size_t>(parsed);
    return offset < size_bytes ? offset : 0;
}

bool IsEpub(const std::string& filename)
{
    return HasExtension(filename, ".epub");
}

std::string FullPath(const std::string& filename)
{
    return BookPath(filename);
}

bool LoadEpubPosition(const std::string& filename, EpubPosition* out)
{
    if (out == nullptr) {
        return false;
    }
    FILE* file = std::fopen(PositionSidecarPath(filename).c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    char text[64] = {};
    const size_t bytes_read = std::fread(text, 1, sizeof(text) - 1, file);
    std::fclose(file);
    text[bytes_read] = '\0';
    unsigned long spine = 0;
    unsigned long block = 0;
    unsigned long offset = 0;
    if (std::sscanf(text, "epub %lu %lu %lu", &spine, &block, &offset) != 3) {
        return false;
    }
    out->spine = static_cast<uint32_t>(spine);
    out->block = static_cast<uint32_t>(block);
    out->offset = static_cast<uint32_t>(offset);
    return true;
}

void SaveEpubPosition(const std::string& filename, const EpubPosition& position)
{
    FILE* file = std::fopen(PositionSidecarPath(filename).c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Save position failed to open sidecar for %s", filename.c_str());
        return;
    }
    std::fprintf(file, "epub %lu %lu %lu\n", static_cast<unsigned long>(position.spine),
                 static_cast<unsigned long>(position.block),
                 static_cast<unsigned long>(position.offset));
    std::fclose(file);
}

void SavePosition(const std::string& filename, size_t offset)
{
    FILE* file = std::fopen(PositionSidecarPath(filename).c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Save position failed to open sidecar for %s", filename.c_str());
        return;
    }
    std::fprintf(file, "%zu\n", offset);
    std::fclose(file);
}

namespace {

bool ReadSmallFile(const std::string& path, size_t max_bytes, std::string* out)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    out->clear();
    char buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0 && out->size() <= max_bytes) {
        out->append(buffer, read);
    }
    std::fclose(file);
    return true;
}

void WriteSmallFile(const std::string& path, const std::string& data)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Writing %s failed", path.c_str());
        return;
    }
    std::fwrite(data.data(), 1, data.size(), file);
    std::fclose(file);
}

}  // namespace

bool LoadCover(const std::string& filename, int box_width, int box_height,
               cover_image::Thumbnail* out)
{
    *out = cover_image::Thumbnail{};
    if (!IsEpub(filename)) {
        return false;
    }
    struct stat st = {};
    if (stat(BookPath(filename).c_str(), &st) != 0) {
        return false;
    }
    const std::string folder = JoinPath(FolderPath(), kCoverCacheFolder);
    const std::string cache_path = JoinPath(
        folder, library::CoverCacheName(filename, static_cast<uint64_t>(st.st_size),
                                        static_cast<int64_t>(st.st_mtime), box_width, box_height));

    std::string cached;
    if (ReadSmallFile(cache_path, kMaxCoverCacheBytes, &cached)) {
        // An empty file records "this book has no usable cover", so it isn't tried every time.
        return !cached.empty() && cover_image::Parse(cached, out);
    }

    // First time for this book: decode it (slow, once) and cache the result either way.
    std::string encoded;
    epub::OpenError error = epub::OpenError::kNone;
    const auto book = epub::EpubBook::Open(
        std::make_unique<epub::FileByteSource>(BookPath(filename)), &error);
    if (book != nullptr && !book->metadata().cover_path.empty()) {
        (void)book->ReadFile(book->metadata().cover_path, kMaxCoverImageBytes, &encoded);
    }
    const cover_image::Result result =
        encoded.empty() ? cover_image::Result::kEmpty
                        : cover_image::MakeThumbnail(encoded, box_width, box_height, out);
    if (result != cover_image::Result::kOk) {
        *out = cover_image::Thumbnail{};
    }
    if (mkdir(folder.c_str(), 0775) != 0 && errno != EEXIST) {
        ESP_LOGW(kTag, "Create cover cache folder failed: errno=%d", errno);
        return result == cover_image::Result::kOk;
    }
    WriteSmallFile(cache_path,
                   result == cover_image::Result::kOk ? cover_image::Serialize(*out) : std::string());
    ESP_LOGI(kTag, "Cover for %s: %s", filename.c_str(),
             result == cover_image::Result::kOk ? "cached" : "none");
    return result == cover_image::Result::kOk;
}

std::vector<library::Bookmark> LoadBookmarks(const std::string& filename)
{
    FILE* file = std::fopen(BookmarksSidecarPath(filename).c_str(), "rb");
    if (file == nullptr) {
        return {};
    }
    std::string text;
    char buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0 &&
           text.size() < kMaxBookmarksBytes) {
        text.append(buffer, read);
    }
    std::fclose(file);
    return library::ParseBookmarks(text);
}

void SaveBookmarks(const std::string& filename, const std::vector<library::Bookmark>& bookmarks)
{
    const std::string path = BookmarksSidecarPath(filename);
    if (bookmarks.empty()) {
        (void)unlink(path.c_str());  // no file rather than an empty one
        return;
    }
    const std::string text = library::SerializeBookmarks(bookmarks);
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Saving bookmarks for %s failed", filename.c_str());
        return;
    }
    std::fwrite(text.data(), 1, text.size(), file);
    std::fclose(file);
}

bool DeleteBook(const std::string& filename)
{
    errno = 0;
    if (unlink(BookmarksSidecarPath(filename).c_str()) != 0 && errno != ENOENT) {
        ESP_LOGW(kTag, "Delete bookmarks for %s failed: errno=%d (%s)", filename.c_str(), errno,
                 std::strerror(errno));
    }
    // Sidecar first: if the process is interrupted between the two removals, leaving a
    // stray .pos behind is harmless (ListBooks only ever looks at .txt/.epub files), whereas
    // leaving the book behind with its position already gone is a worse partial state.
    errno = 0;
    if (unlink(PositionSidecarPath(filename).c_str()) != 0 && errno != ENOENT) {
        ESP_LOGW(kTag, "Delete position sidecar for %s failed: errno=%d (%s)", filename.c_str(),
                 errno, std::strerror(errno));
    }

    errno = 0;
    if (unlink(BookPath(filename).c_str()) != 0 && errno != ENOENT) {
        ESP_LOGW(kTag, "Delete book %s failed: errno=%d (%s)", filename.c_str(), errno,
                 std::strerror(errno));
        return false;
    }
    return true;
}

}  // namespace text_reader_service
