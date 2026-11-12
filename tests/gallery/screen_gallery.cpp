// Screen gallery (#14, ADR 0001): renders every epaper_ui screen and overlay from sample
// states into the panel's own 800x480 1-bpp framebuffer, writes each as a portrait PNG, and
// checks a CRC-32 of each framebuffer against tests/gallery/golden.txt.
//
//   screen_gallery --out DIR [--golden FILE] [--update]
//
// Without --update, a missing or changed hash fails (exit 1) and names the screen; the PNGs
// in DIR show what it looks like now. With --update, the golden file is rewritten. All sample
// text is original.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "book_layout/book_layout.h"
#include "epaper_ui/book_list_page.h"
#include "epaper_ui/book_page.h"
#include "epaper_ui/book_reader_page.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/card_modal.h"
#include "epaper_ui/dashboard_page.h"
#include "epaper_ui/details_page.h"
#include "epaper_ui/follow_up_page.h"
#include "epaper_ui/keyboard.h"
#include "epaper_ui/lock_screen.h"
#include "epaper_ui/mascots.h"
#include "greeting/special_days.h"
#include "epaper_ui/notes_page.h"
#include "epaper_ui/onboarding_page.h"
#include "epaper_ui/select_modal.h"
#include "epaper_ui/settings_about_page.h"
#include "epaper_ui/settings_page.h"
#include "epaper_ui/settings_storage_page.h"
#include "epaper_ui/settings_todos_page.h"
#include "epaper_ui/settings_topics_page.h"
#include "epaper_ui/sticky_note.h"
#include "epaper_ui/time_page.h"
#include "epaper_ui/toast.h"
#include "epaper_ui/todos_page.h"
#include "epaper_ui/topic_entries_page.h"
#include "epaper_ui/topic_summary_page.h"
#include "epaper_ui/topics_browse_page.h"
#include "epaper_ui/wifi_page.h"
#include "epub_engine/inflate.h"
#include "epub_engine/xhtml_parser.h"
#include "project_assets.h"

namespace {

constexpr int kRawWidth = 800;
constexpr int kRawHeight = 480;
constexpr int kPortraitWidth = 480;
constexpr int kPortraitHeight = 800;
constexpr size_t kFramebufferBytes = kRawWidth * kRawHeight / 8;

using Framebuffer = std::vector<uint8_t>;
using Renderer = std::function<void(uint8_t*)>;

// ---------- PNG (1-bit grayscale, stored deflate: no zlib needed) ----------

void PutBe32(std::string* out, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        out->push_back(static_cast<char>((value >> shift) & 0xFF));
    }
}

void PutChunk(std::string* png, const char* type, const std::string& data)
{
    PutBe32(png, static_cast<uint32_t>(data.size()));
    std::string body(type, 4);
    body += data;
    png->append(body);
    PutBe32(png, epub::Crc32(body.data(), body.size()));
}

bool WritePng(const Framebuffer& fb, const std::string& path)
{
    // Portrait rows of 1-bit pixels (1 = white, as on the panel), each led by filter byte 0.
    const size_t row_bytes = kPortraitWidth / 8;
    std::string raw;
    raw.reserve((row_bytes + 1) * kPortraitHeight);
    for (int y = 0; y < kPortraitHeight; ++y) {
        raw.push_back('\0');
        for (size_t byte = 0; byte < row_bytes; ++byte) {
            uint8_t value = 0;
            for (int bit = 0; bit < 8; ++bit) {
                const int x = static_cast<int>(byte) * 8 + bit;
                const int raw_x = y;
                const int raw_y = kRawHeight - 1 - x;  // portrait x runs along the gate line
                const bool white =
                    fb[static_cast<size_t>(raw_y) * (kRawWidth / 8) + raw_x / 8] & (0x80 >> (raw_x % 8));
                value = static_cast<uint8_t>((value << 1) | (white ? 1 : 0));
            }
            raw.push_back(static_cast<char>(value));
        }
    }
    std::string zlib = {'\x78', '\x01'};
    for (size_t pos = 0; pos < raw.size();) {
        const size_t length = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + length == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<char>(length & 0xFF));
        zlib.push_back(static_cast<char>(length >> 8));
        zlib.push_back(static_cast<char>(~length & 0xFF));
        zlib.push_back(static_cast<char>((~length >> 8) & 0xFF));
        zlib.append(raw, pos, length);
        pos += length;
    }
    uint32_t a = 1;
    uint32_t b = 0;
    for (const char c : raw) {
        a = (a + static_cast<uint8_t>(c)) % 65521;
        b = (b + a) % 65521;
    }
    PutBe32(&zlib, (b << 16) | a);

    std::string png = "\x89PNG\r\n\x1a\n";
    std::string header;
    PutBe32(&header, kPortraitWidth);
    PutBe32(&header, kPortraitHeight);
    header += std::string("\x01\x00\x00\x00\x00", 5);  // bit depth 1, grayscale
    PutChunk(&png, "IHDR", header);
    PutChunk(&png, "IDAT", zlib);
    PutChunk(&png, "IEND", "");
    std::ofstream file(path, std::ios::binary);
    file.write(png.data(), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(file);
}

// ---------- Sample states ----------

epaper_ui::StatusBarState StatusBar()
{
    epaper_ui::StatusBarState status = {};
    status.battery.percent = 76;
    status.wifi = epaper_ui::WifiStatus::kConnected;
    status.time_text = "9:41";
    return status;
}

epaper_ui::GlobalFooterState Footer()
{
    epaper_ui::GlobalFooterState footer = {};
    footer.visible = true;
    return footer;
}

epaper_ui::ListItemState Item(const char* time, const char* tag, const char* body, bool selected = false)
{
    epaper_ui::ListItemState item = {};
    item.header.time_text = time;
    item.header.tag_text = tag;
    item.body_text = body;
    item.selected = selected;
    return item;
}

epaper_ui::TimelineListState Timeline(const char* plural)
{
    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = plural;
    epaper_ui::TimelineGroupState today = {};
    today.label_text = "Today";
    today.items = {Item("9:12", "Note", "The library opens late on Thursdays, worth remembering.", true),
                   Item("8:40", "Idea", "A tiny reading nook by the window, with the striped cushion.")};
    epaper_ui::TimelineGroupState yesterday = {};
    yesterday.label_text = "Yesterday";
    yesterday.items = {Item("18:05", "To-do", "Return the borrowed umbrella.")};
    timeline.groups = {today, yesterday};
    timeline.visible_group_index = 0;
    timeline.focused_group_index = 0;
    return timeline;
}

std::map<std::string, Renderer> Screens()
{
    using namespace epaper_ui;
    std::map<std::string, Renderer> screens;
    const auto page = [](auto draw) {
        return [draw](uint8_t* fb) { draw(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight); };
    };

    screens["home"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        DashboardPageState state = {};
        state.navigation_focus_index = 0;
        state.welcome_message.title_text = "Good morning";
        state.welcome_message.current_date = {"Wednesday", "November 11"};
        DrawDashboardPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["lock_screen"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        LockScreenState state = {};
        state.weekday_text = "Wednesday";
        state.date_text = "November 11";
        state.pending_todo_titles = {"Water the fern", "Call about the book club",
                                     "Pick up oat milk and a new notebook on the way home"};
        state.pending_todo_count = 3;
        DrawLockScreen(fb, rw, rh, pw, ph, state, StatusBar());
    });
    screens["lock_screen_shutdown"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        LockScreenState state = {};
        state.weekday_text = "Wednesday";
        state.date_text = "November 11";
        state.for_shutdown = true;
        DrawLockScreen(fb, rw, rh, pw, ph, state, StatusBar());
    });
    screens["onboarding"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        OnboardingPageState state = {};
        state.carousel.slide_count = 6;
        state.carousel.show_close = false;
        state.carousel.next_selected = true;
        state.slide_title = "Welcome to Keepsake";
        state.slide_body = "Your pocket voice notebook. Capture thoughts out loud and let "
                           "Keepsake keep them organized.";
        state.slide_image = project_assets::GetImage(EmbeddedImageId::kSlide1);
        DrawOnboardingPage(fb, rw, rh, pw, ph, state, StatusBar());
    });
    screens["notes"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        NotesPageState state = {};
        state.timeline = Timeline("Notes");
        DrawNotesPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["notes_empty"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        NotesPageState state = {};
        state.timeline.empty_state_text = "No notes yet. Hold Record to capture one.";
        DrawNotesPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["todos"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        TodosPageState state = {};
        state.timeline = Timeline("Todos");
        DrawTodosPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["follow_up"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        FollowUpPageState state = {};
        DrawFollowUpPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["details"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        DetailsPageState state = {};
        DrawDetailsPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsPageState state = {};
        DrawSettingsPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings_storage"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsStoragePageState state = {};
        DrawSettingsStoragePage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings_about"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsAboutPageState state = {};
        state.version_text = "Version 1.3.0";
        state.update_status_text = "You're up to date.";
        state.update = {.label_text = "Check for updates", .selected = true};
        state.back = {.label_text = "Back"};
        DrawSettingsAboutPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings_about_update_ready"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsAboutPageState state = {};
        state.version_text = "Version 1.3.0";
        state.update_status_text = "Version 1.4.0 is ready. Restart to finish.";
        state.update = {.label_text = "Restart to update", .selected = true};
        state.back = {.label_text = "Back"};
        DrawSettingsAboutPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings_todos"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsTodosPageState state = {};
        DrawSettingsTodosPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["settings_topics"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        SettingsTopicsPageState state = {};
        DrawSettingsTopicsPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["wifi"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        WifiPageState state = {};
        DrawWifiPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["time"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        TimePageState state = {};
        DrawTimePage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["topics_browse"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        TopicsBrowsePageState state = {};
        DrawTopicsBrowsePage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["topic_entries"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        TopicEntriesPageState state = {};
        DrawTopicEntriesPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["topic_summary"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        TopicSummaryPageState state = {};
        DrawTopicSummaryPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["book_list"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        BookListPageState state = {};
        // As the shelf shows them: titles, progress, the book being read first.
        for (const char* name : {"A Rainy Day Sampler - 42%", "The Lighthouse Keeper's Cat - 7%",
                                 "Garden notes", "Winter recipes"}) {
            SelectItemState item = {};
            item.label_text = name;
            state.books_list.items.push_back(item);
        }
        state.books_list.focused = true;
        state.books_list.selected_item_index = 0;
        state.navigation_focus_index = 0;
        DrawBookListPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["book_list_continue"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        BookListPageState state = {};
        state.continue_reading.visible = true;
        state.continue_reading.title_text = "A Rainy Day Sampler";
        state.continue_reading.author_text = "Test Author";
        state.continue_reading.progress_percent = 42;
        // A made-up cover (dark sky, a pale moon, a gradient hill), dithered like a real one.
        std::vector<uint8_t> gray(static_cast<size_t>(kContinueCoverWidth) * kContinueCoverHeight);
        for (int y = 0; y < kContinueCoverHeight; ++y) {
            for (int x = 0; x < kContinueCoverWidth; ++x) {
                const int dx = x - 56;
                const int dy = y - 34;
                uint8_t v = static_cast<uint8_t>(40 + y);  // sky lightening downwards
                if (dx * dx + dy * dy < 14 * 14) {
                    v = 235;  // moon
                }
                if (y > 90 - (x / 6)) {
                    v = static_cast<uint8_t>(110 + (x * 100) / kContinueCoverWidth);  // hill
                }
                gray[static_cast<size_t>(y) * kContinueCoverWidth + static_cast<size_t>(x)] = v;
            }
        }
        state.continue_reading.cover =
            cover_image::Dither(gray, kContinueCoverWidth, kContinueCoverHeight);
        for (const char* name : {"A Rainy Day Sampler - 42%", "The Lighthouse Keeper's Cat - 7%",
                                 "Garden notes"}) {
            SelectItemState item = {};
            item.label_text = name;
            state.books_list.items.push_back(item);
        }
        state.books_list.focused = true;
        state.books_list.selected_item_index = 0;
        state.navigation_focus_index = 0;
        DrawBookListPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["book_reader_txt"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        BookReaderPageState state = {};
        state.title_text = "garden_notes.txt";
        state.page_number = 3;
        state.page_lines =
            PaginateBookText(pw, ph,
                             "The tomatoes came up crooked this year, leaning toward the fence as "
                             "if they had somewhere to be. The basil, on the other hand, behaved "
                             "beautifully.\n\nNext spring: more marigolds, fewer promises.")
                .lines;
        DrawBookReaderPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    screens["book_reader_bookmarked"] = page([](uint8_t* fb, int rw, int rh, int pw, int ph) {
        BookReaderPageState state = {};
        state.title_text = "The Lighthouse Keeper's Very Long Almanac";
        state.bookmarked = true;
        state.page_number = 12;
        state.page_lines =
            PaginateBookText(pw, ph, "Bookmarked: the ribbon hangs from the top corner, and "
                                     "the title makes room for it.")
                .lines;
        DrawBookReaderPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
    });
    // The same chapter at each text size (#28).
    const auto epub_reader = [&page](BookTextSize size) {
        return page([size](uint8_t* fb, int rw, int rh, int pw, int ph) {
            SetBookTextSize(size);
            const epub::Chapter chapter = epub::ParseXhtml(
                "<h1>Chapter One</h1>"
                "<p>The kettle began to sing just as the rain arrived, and <b>Mira</b> decided the "
                "day was <i>officially</i> a reading day.</p>"
                "<p>She wrote three things on a card:<br/>tea<br/>blanket<br/>book</p>"
                "<ul><li>Find the lamp</li><li>Move the cat gently</li></ul><hr/>"
                "<h2>A Second Part</h2><p>Caf\xC3\xA9 plans and a \xE2\x80\x9Cquick\xE2\x80\x9D nap.</p>",
                "a.xhtml");
            const UiRect content = BookReaderContentBounds(pw, ph);
            book_layout::LayoutOptions options;
            options.width = content.width;
            options.height = content.height;
            const auto pages = book_layout::PaginateChapter(chapter, EpaperTextMeasurer(), options);
            BookReaderPageState state = {};
            state.title_text = "A Rainy Day Sampler";
            state.use_layout_page = true;
            state.layout_page = pages.front();
            state.page_label = "Chapter One - 1/" + std::to_string(pages.size()) + " - 4%";
            DrawBookReaderPage(fb, rw, rh, pw, ph, state, StatusBar(), Footer());
            SetBookTextSize(BookTextSize::kMedium);
        });
    };
    screens["book_reader_epub"] = epub_reader(BookTextSize::kMedium);
    screens["book_reader_epub_small"] = epub_reader(BookTextSize::kSmall);
    screens["book_reader_epub_large"] = epub_reader(BookTextSize::kLarge);

    // Overlays, drawn over Home the way the app composites them.
    const auto over_home = [&screens](auto draw_overlay) {
        return [&screens, draw_overlay](uint8_t* fb) {
            screens["home"](fb);
            draw_overlay(fb);
        };
    };
    screens["overlay_toast"] = over_home([](uint8_t* fb) {
        ToastState state = {};
        state.visible = true;
        state.body_text = "Saved to Notes";
        DrawToast(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
    });
    screens["overlay_select_modal"] = over_home([](uint8_t* fb) {
        SelectModalState state = {};
        state.visible = true;
        state.title_text = "Tag this recording";
        for (const char* label : {"Note", "To-do", "Idea", "Topic", "Discard"}) {
            SelectModalItemState item = {};
            item.label_text = label;
            state.items.push_back(item);
        }
        DrawSelectModal(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
    });
    // Settings -> Saved Wi-Fi (#43): the list, then the confirmation.
    screens["saved_wifi"] = over_home([](uint8_t* fb) {
        SelectModalState state = {};
        state.visible = true;
        state.title_text = "Saved Wi-Fi";
        for (const char* label : {"Home Network (connected)", "Office", "Grandma's", "Close"}) {
            SelectModalItemState item = {};
            item.label_text = label;
            state.items.push_back(item);
        }
        DrawSelectModal(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
    });
    screens["saved_wifi_forget"] = over_home([](uint8_t* fb) {
        CardModalState state = {};
        state.visible = true;
        state.title_text = "Forget this Wi-Fi?";
        state.body_text =
            "Keepsake won't join Office again until it's set up again from Settings > Network.";
        state.action_labels = {"Cancel", "Forget"};
        DrawCardModal(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
    });
    screens["overlay_card_modal"] = over_home([](uint8_t* fb) {
        CardModalState state = {};
        state.visible = true;
        state.title_text = "Shut down?";
        state.body_text = "Your device will power off. Do you want to continue?";
        state.action_labels = {"Cancel", "Shut down"};
        DrawCardModal(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
    });
    // Special days (#85): the card that greets the day, with its mascot (#20).
    {
        greeting::People people;
        people.name = "Sam";
        people.birthday = "07-14";
        people.anniversary = "2021-09-03";
        people.first_date = "2019-05-20";
        people.custom_days = {{"03-17", "Happy Gotcha Day, Biscuit!", "Treats for the dog today."}};
        const std::pair<const char*, greeting::Date> days[] = {
            {"day_birthday", {2026, 7, 14}},   {"day_christmas", {2026, 12, 25}},
            {"day_new_year", {2027, 1, 1}},    {"day_valentines", {2027, 2, 14}},
            {"day_anniversary", {2026, 9, 3}}, {"day_first_date", {2026, 5, 20}},
            {"day_halloween", {2026, 10, 31}}, {"day_custom", {2027, 3, 17}},
        };
        for (const auto& [name, date] : days) {
            const auto occasions = greeting::OccasionsOn(people, date);
            if (occasions.empty()) {
                continue;
            }
            const greeting::Occasion occasion = occasions.front();
            screens[name] = over_home([occasion](uint8_t* fb) {
                CardModalState state = {};
                state.visible = true;
                state.title_text = occasion.title;
                state.body_text = occasion.message;
                state.action_labels = {"Close"};
                state.image = OccasionMascot(occasion.mascot);
                DrawCardModal(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state);
            });
        }
    }
    screens["overlay_keyboard"] = over_home([](uint8_t* fb) {
        KeyboardState state = {};
        state.visible = true;
        state.title_text = "Wi-Fi password";
        state.input.label_text = "Password";
        state.input.value_text = "cozy";
        state.input.focused = true;
        DrawKeyboard(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state, KeyboardStyle{});
    });
    screens["overlay_sticky_note"] = over_home([](uint8_t* fb) {
        StickyNoteState state = {};
        state.visible = true;
        state.sticky_count = 2;
        state.date_text = "Today";
        state.header.time_text = "9:12";
        state.header.tag_text = "To-do";
        state.body_text = "Return the borrowed umbrella before the weekend.";
        DrawStickyNote(fb, kRawWidth, kRawHeight, kPortraitWidth, kPortraitHeight, state,
                       StickyNoteStyle{});
    });
    return screens;
}

std::string Hex(uint32_t value)
{
    char buffer[9];
    std::snprintf(buffer, sizeof(buffer), "%08x", value);
    return buffer;
}

std::map<std::string, std::string> ReadGolden(const std::string& path)
{
    std::map<std::string, std::string> golden;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream in(line);
        std::string name;
        std::string hash;
        if (in >> name >> hash) {
            golden[name] = hash;
        }
    }
    return golden;
}

}  // namespace

int main(int argc, char** argv)
{
    std::string out_dir = ".";
    std::string golden_path;
    bool update = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            out_dir = argv[++i];
        } else if (arg == "--golden" && i + 1 < argc) {
            golden_path = argv[++i];
        } else if (arg == "--update") {
            update = true;
        } else {
            std::fprintf(stderr, "usage: %s --out DIR [--golden FILE] [--update]\n", argv[0]);
            return 2;
        }
    }

    const std::map<std::string, std::string> golden =
        golden_path.empty() ? std::map<std::string, std::string>{} : ReadGolden(golden_path);
    std::map<std::string, std::string> actual;
    int failures = 0;
    for (const auto& [name, render] : Screens()) {
        Framebuffer fb(kFramebufferBytes, 0xFF);
        render(fb.data());
        const std::string hash = Hex(epub::Crc32(fb.data(), fb.size()));
        actual[name] = hash;
        const std::string png = out_dir + "/" + name + ".png";
        if (!WritePng(fb, png)) {
            std::fprintf(stderr, "could not write %s\n", png.c_str());
            return 2;
        }
        if (!golden_path.empty() && !update) {
            const auto it = golden.find(name);
            if (it == golden.end()) {
                std::fprintf(stderr, "NEW      %-24s %s (not in golden file)\n", name.c_str(), hash.c_str());
                ++failures;
            } else if (it->second != hash) {
                std::fprintf(stderr, "CHANGED  %-24s %s -> %s\n", name.c_str(), it->second.c_str(),
                             hash.c_str());
                ++failures;
            } else {
                std::printf("ok       %s\n", name.c_str());
            }
        }
    }
    for (const auto& [name, hash] : golden) {
        if (!update && actual.find(name) == actual.end()) {
            std::fprintf(stderr, "MISSING  %s (in golden file, no longer rendered)\n", name.c_str());
            ++failures;
        }
    }

    if (update && !golden_path.empty()) {
        std::ofstream file(golden_path);
        file << "# Screen gallery golden hashes (CRC-32 of each 800x480 framebuffer).\n"
             << "# Regenerate after an intended UI change, and look at the PNGs first:\n"
             << "#   build/tests/screen_gallery --out build/tests/gallery --golden "
                "tests/gallery/golden.txt --update\n";
        for (const auto& [name, hash] : actual) {
            file << name << ' ' << hash << '\n';
        }
        std::printf("updated %s (%zu screens)\n", golden_path.c_str(), actual.size());
        return 0;
    }
    if (failures > 0) {
        std::fprintf(stderr,
                     "%d screen(s) differ from tests/gallery/golden.txt. Look at the PNGs in %s; "
                     "if the change is intended, rerun with --update.\n",
                     failures, out_dir.c_str());
        return 1;
    }
    std::printf("%zu screens rendered to %s\n", actual.size(), out_dir.c_str());
    return 0;
}
