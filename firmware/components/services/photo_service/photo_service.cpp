#include "photo_service.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "chroninkle_task_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "storage_service.h"
#include "tjpgd_decode.h"

namespace photo_service {
namespace {

constexpr const char* kTag = "PhotoService";
constexpr const char* kFolder = "/photos";
constexpr const char* kCacheFolder = "/.keepsake/photo-cache";
constexpr const char* kNvsNamespace = "photos";
constexpr const char* kKeyLast = "last";
constexpr size_t kMaxPhotos = 500;
constexpr int kMaxDepth = 2;
constexpr uint64_t kMaxFileBytes = 40ull * 1024 * 1024;
// Files up to this size go through stb_image (all formats, progressive JPEG too); bigger
// JPEGs through the ROM's scaled decoder.
constexpr uint64_t kStbMaxBytes = 4ull * 1024 * 1024;
constexpr uint32_t kWorkerStackBytes = 12 * 1024;

struct Item {
    std::string path;  // relative to /photos
    uint64_t size = 0;
    int64_t modified = 0;
    photo::Caption caption;
};

std::mutex s_mutex;          // s_items, s_last_path
std::mutex s_prepare_mutex;  // one photo prepared at a time (memory)
std::vector<Item> s_items;
std::string s_last_path;
QueueHandle_t s_queue = nullptr;

std::string Root()
{
    return std::string(storage_service::MountPoint()) + kFolder;
}

std::string CacheRoot()
{
    return std::string(storage_service::MountPoint()) + kCacheFolder;
}

void EnsureCacheFolder()
{
    const std::string keepsake = std::string(storage_service::MountPoint()) + "/.keepsake";
    mkdir(keepsake.c_str(), 0775);
    mkdir(CacheRoot().c_str(), 0775);
}

bool ReadFile(const std::string& path, uint64_t limit, std::string* out)
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    bool ok = size >= 0 && static_cast<uint64_t>(size) <= limit;
    if (ok) {
        out->resize(static_cast<size_t>(size));
        ok = std::fread(out->data(), 1, out->size(), file) == out->size();
    }
    std::fclose(file);
    return ok;
}

void Scan(const std::string& folder, const std::string& relative, int depth, std::vector<Item>* items)
{
    DIR* dir = opendir(folder.c_str());
    if (dir == nullptr) {
        return;
    }
    while (dirent* entry = readdir(dir)) {
        if (items->size() >= kMaxPhotos) {
            break;
        }
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') {
            continue;
        }
        const std::string full = folder + "/" + name;
        const std::string rel = relative.empty() ? name : relative + "/" + name;
        struct stat info = {};
        if (stat(full.c_str(), &info) != 0) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            if (depth < kMaxDepth) {
                Scan(full, rel, depth + 1, items);
            }
            continue;
        }
        if (!photo::IsPhotoName(name)) {
            continue;
        }
        Item item;
        item.path = rel;
        item.size = static_cast<uint64_t>(info.st_size);
        item.modified = static_cast<int64_t>(info.st_mtime);
        // The EXIF date only matters when the name has none: read just the start of the file.
        std::string exif_date;
        if (!photo::CaptionFor(name, "").date.valid()) {
            if (FILE* file = std::fopen(full.c_str(), "rb")) {
                std::string head(64 * 1024, '\0');
                head.resize(std::fread(head.data(), 1, head.size(), file));
                std::fclose(file);
                exif_date = photo::ReadExif(head).date_time;
            }
        }
        item.caption = photo::CaptionFor(rel, exif_date);
        items->push_back(std::move(item));
    }
    closedir(dir);
}

Item ItemAt(int index, bool* found)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    *found = index >= 0 && index < static_cast<int>(s_items.size());
    return *found ? s_items[static_cast<size_t>(index)] : Item{};
}

std::string CachePath(const Item& item)
{
    return CacheRoot() + "/" + photo::CacheName(item.path, item.size, item.modified);
}

// Decodes and prepares a photo from the card (no cache).
Loaded Prepare(const Item& item)
{
    Loaded loaded;
    const std::string full = Root() + "/" + item.path;
    if (item.size == 0) {
        loaded.result = photo::Result::kEmpty;
        return loaded;
    }
    if (item.size > kMaxFileBytes) {
        loaded.result = photo::Result::kTooBig;
        return loaded;
    }
    FILE* file = std::fopen(full.c_str(), "rb");
    if (file == nullptr) {
        loaded.result = photo::Result::kUndecodable;
        return loaded;
    }
    std::string head(64 * 1024, '\0');
    head.resize(std::fread(head.data(), 1, head.size(), file));
    loaded.format = photo::FormatName(head);
    if (!photo::CanDecode(loaded.format)) {
        std::fclose(file);
        loaded.result = loaded.format.empty() ? photo::Result::kUndecodable
                                              : photo::Result::kUnsupported;
        return loaded;
    }

    cover_image::Thumbnail bitmap;
    if (loaded.format == "JPEG" && item.size > kStbMaxBytes) {
        photo::Gray gray;
        const bool decoded = DecodeJpegScaled(file, &gray);
        std::fclose(file);
        if (!decoded) {
            loaded.result = photo::Result::kTooBig;  // e.g. a huge progressive JPEG
            return loaded;
        }
        bitmap = photo::Prepare(gray, photo::ReadExif(head).orientation);
    } else {
        std::fclose(file);
        std::string encoded;
        if (!ReadFile(full, kStbMaxBytes, &encoded)) {
            loaded.result = photo::Result::kUndecodable;
            return loaded;
        }
        loaded.result = photo::PrepareEncoded(encoded, &bitmap);
        if (loaded.result != photo::Result::kOk) {
            return loaded;
        }
    }
    if (bitmap.width <= 0) {
        loaded.result = photo::Result::kUndecodable;
        return loaded;
    }
    loaded.result = photo::Result::kOk;
    loaded.bitmap = std::make_shared<const cover_image::Thumbnail>(std::move(bitmap));
    return loaded;
}

Loaded LoadItem(const Item& item)
{
    Loaded loaded;
    std::string cached;
    cover_image::Thumbnail bitmap;
    const std::string cache_path = CachePath(item);
    if (ReadFile(cache_path, 256 * 1024, &cached) && cover_image::Parse(cached, &bitmap)) {
        loaded.result = photo::Result::kOk;
        loaded.bitmap = std::make_shared<const cover_image::Thumbnail>(std::move(bitmap));
        return loaded;
    }
    std::lock_guard<std::mutex> lock(s_prepare_mutex);
    // The worker may have just finished it.
    if (ReadFile(cache_path, 256 * 1024, &cached) && cover_image::Parse(cached, &bitmap)) {
        loaded.result = photo::Result::kOk;
        loaded.bitmap = std::make_shared<const cover_image::Thumbnail>(std::move(bitmap));
        return loaded;
    }
    loaded = Prepare(item);
    if (loaded.result == photo::Result::kOk) {
        EnsureCacheFolder();
        if (FILE* file = std::fopen(cache_path.c_str(), "wb")) {
            const std::string data = cover_image::Serialize(*loaded.bitmap);
            std::fwrite(data.data(), 1, data.size(), file);
            std::fclose(file);
        }
    }
    ESP_LOGI(kTag, "prepared %s: %d", item.path.c_str(), static_cast<int>(loaded.result));
    return loaded;
}

void Worker(void*)
{
    int index = 0;
    while (true) {
        if (xQueueReceive(s_queue, &index, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        bool found = false;
        const Item item = ItemAt(index, &found);
        if (!found || !storage_service::IsMounted()) {
            continue;
        }
        struct stat info = {};
        if (stat(CachePath(item).c_str(), &info) == 0) {
            continue;  // already prepared
        }
        (void)LoadItem(item);
    }
}

}  // namespace

esp_err_t Init()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) == ESP_OK) {
        size_t length = 0;
        if (nvs_get_str(handle, kKeyLast, nullptr, &length) == ESP_OK && length > 0 && length < 512) {
            std::string value(length, '\0');
            if (nvs_get_str(handle, kKeyLast, value.data(), &length) == ESP_OK) {
                value.resize(length - 1);
                std::lock_guard<std::mutex> lock(s_mutex);
                s_last_path = value;
            }
        }
        nvs_close(handle);
    }
    s_queue = xQueueCreate(8, sizeof(int));
    if (s_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreatePinnedToCore(Worker, "photo_prep", kWorkerStackBytes, nullptr,
                                chroninkle_task_config::kPriorityPhotoPrepare, nullptr,
                                chroninkle_task_config::kAppCore) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

int Rescan()
{
    std::vector<Item> items;
    if (storage_service::IsMounted()) {
        Scan(Root(), "", 0, &items);
    }
    std::vector<photo::Entry> entries;
    entries.reserve(items.size());
    for (const Item& item : items) {
        entries.push_back({item.path, item.caption});
    }
    photo::SortOldestFirst(&entries);
    std::vector<Item> sorted;
    sorted.reserve(items.size());
    for (const photo::Entry& entry : entries) {
        const auto it = std::find_if(items.begin(), items.end(),
                                     [&entry](const Item& item) { return item.path == entry.path; });
        if (it != items.end()) {
            sorted.push_back(*it);
        }
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    s_items = std::move(sorted);
    ESP_LOGI(kTag, "%u photo(s) in %s", static_cast<unsigned>(s_items.size()), kFolder);
    return static_cast<int>(s_items.size());
}

int Count()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return static_cast<int>(s_items.size());
}

photo::Caption CaptionAt(int index)
{
    bool found = false;
    return ItemAt(index, &found).caption;
}

Loaded Load(int index)
{
    bool found = false;
    const Item item = ItemAt(index, &found);
    if (!found || !storage_service::IsMounted()) {
        return {};
    }
    return LoadItem(item);
}

void Prefetch(int index)
{
    if (s_queue != nullptr && index >= 0 && index < Count()) {
        (void)xQueueSend(s_queue, &index, 0);
    }
}

int LastIndex()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    for (size_t i = 0; i < s_items.size(); ++i) {
        if (s_items[i].path == s_last_path) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

void RememberIndex(int index)
{
    bool found = false;
    const Item item = ItemAt(index, &found);
    if (!found) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_last_path == item.path) {
            return;
        }
        s_last_path = item.path;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, kKeyLast, item.path.c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }
}

}  // namespace photo_service
