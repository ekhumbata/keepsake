#include "book_list_page_coordinator.h"

#include <algorithm>

#include "epaper_ui/book_page.h"
#include "library/library.h"

namespace {

using page_navigation::NavigationItemRole;

}  // namespace

BookListPageCoordinator::BookListPageCoordinator() = default;

void BookListPageCoordinator::RebuildNavigationModel(int book_count)
{
    navigation_model_ = page_navigation::BuildBookListPageNavigationModel(book_count);
}

void BookListPageCoordinator::Show(int book_count)
{
    RebuildNavigationModel(book_count);
    focus_.Configure(navigation_model_.item_count, 0);
}

void BookListPageCoordinator::Sync(int book_count)
{
    const int previous_index = focus_.index();
    RebuildNavigationModel(book_count);
    focus_.Configure(navigation_model_.item_count, previous_index);
}

bool BookListPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool BookListPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool BookListPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

int BookListPageCoordinator::FocusedBookIndex() const
{
    const page_navigation::NavigationItemDescriptor* item = navigation_model_.ItemAt(focus_.index());
    if (item == nullptr || item->role != NavigationItemRole::kBookListBookRow) {
        return -1;
    }
    return item->item_index;
}

epaper_ui::BookListPageState BookListPageCoordinator::BuildState(
    const std::vector<text_reader_service::BookEntry>& books,
    const cover_image::Thumbnail& continue_cover) const
{
    epaper_ui::BookListPageState state = {};
    state.navigation_focus_index = focus_.index();

    // The shelf puts the book being read first; it also gets the card with its cover.
    if (!books.empty() && books.front().last_opened > 0 && books.front().progress_percent < 100) {
        const text_reader_service::BookEntry& reading = books.front();
        state.continue_reading.visible = true;
        state.continue_reading.title_text = epaper_ui::ToDisplayText(
            reading.title.empty() ? library::TitleFromPath(reading.filename) : reading.title);
        state.continue_reading.author_text = epaper_ui::ToDisplayText(reading.author);
        state.continue_reading.progress_percent = std::max(0, reading.progress_percent);
        state.continue_reading.cover = continue_cover;
    }

    const int focused_index = FocusedBookIndex();
    state.books_list.items.reserve(books.size());
    for (const text_reader_service::BookEntry& book : books) {
        library::BookRecord record;
        record.path = book.filename;
        record.title = book.title;
        record.progress_percent = book.progress_percent;
        // Titles are UTF-8; the bitmap fonts draw ASCII (curly quotes, accents mapped).
        state.books_list.items.push_back(
            {.label_text = epaper_ui::ToDisplayText(library::ShelfLabel(record))});
    }
    state.books_list.focused = focused_index >= 0;
    state.books_list.active = focused_index >= 0;
    state.books_list.selected_item_index =
        focused_index >= 0 ? focused_index : epaper_ui::kSelectListNoSelection;
    return state;
}
