#include "editor.h"

#include <fstream>
#include <filesystem>
#include <portable-file-dialogs/portable-file-dialogs.h>
#include <math.h>

using namespace STRN;
using namespace std;

static string getMemorySize(size_t bytes)
{
    if (bytes >= 2048ull * 1024 * 1024)
        return format("{0:.1f} GiB", static_cast<float>(bytes) / (1024.0f * 1024 * 1024));
    else if (bytes >= 2048ull * 1024)
        return format("{0:.1f} MiB", static_cast<float>(bytes) / (1024.0f * 1024));
    else if (bytes >= 2048ull)
        return format("{0:.1f} KiB", static_cast<float>(bytes) / 1024.0f);
    else
        return format("{0} B", bytes);
}

void EditorDrawable::render(Context& ctx)
{
    checkUndoHistoryState(CHANGE_CHECK);

    int text_box_left = 1;
    if (!show_line_checker)
        --text_box_left;
    const int text_box_right = ctx.getSize().x - 1;
    constexpr int text_box_top = 1;
    int text_box_bottom = ctx.getSize().y - 2;
    if (!show_hints)
        ++text_box_bottom;
    const Vec2 text_box_size = { text_box_right - text_box_left, text_box_bottom - text_box_top };

    const int text_content_height = text_box_size.y - 2;
    const int text_left = text_box_left + 1;
    constexpr int text_top = text_box_top + 1;
    const int text_content_width = text_box_size.x - 2;

    const Vec2 scrollbar_start = Vec2{ text_box_right, 1 };
    const int scrollbar_height = text_box_bottom - 1;

    pushTextPalette(ctx);

    if (popup_state != INACTIVE && popup_index != FIND)
        pushSubtextPalette(ctx);

    // header
    static const string unsaved_editing = "[ IAPETUS ] (*) editing ";
    static const string saved_editing = "[ IAPETUS ] - editing ";
    ctx.drawText({ 1, 0 }, (has_unsaved_changes ? unsaved_editing : saved_editing) + filesystem::path(file_path).filename().string());
    const string file_size = getMemorySize(text_content.size());
    ctx.drawText(Vec2{ static_cast<int>(ctx.getSize().x - (file_size.size() + 2)), 0 }, file_size);
    const chrono::duration<float> since_last_edit = chrono::steady_clock::now() - last_change;
    const chrono::duration<float> since_epoch = chrono::steady_clock::now().time_since_epoch();
    if (since_last_edit.count() < 1.0f && enable_animations)
        ctx.draw({ ctx.getSize().x - 1, 0 }, 0x04, static_cast<int>(since_epoch.count() * 4) % 2);
    else
        ctx.draw({ ctx.getSize().x - 1, 0 }, 0x03);
    ctx.drawBox({ text_box_left, text_box_top }, text_box_size);
        
    // text content
    int actual_line = scroll + 1;
    for (int i = scroll; i < static_cast<int>(lines.size()); ++i)
    {
        if (i - scroll > text_content_height - 1)
            continue;
        ctx.drawText(Vec2{ text_left, i + text_top - scroll }, lines[i].first);
        if (show_line_checker)
            ctx.draw(Vec2{ 0, i + text_top - scroll }, (actual_line % 2) ? 0xB0 : 0xB2, 2);
        if (lines[i].second)
            ++actual_line;
    }

    // cursor and selection
    if (popup_state == INACTIVE || popup_index == FIND)
    {
        if (doc.parsing_error_position != (size_t)-1)
        {
            ctx.drawColour(calculatePosition(doc.parsing_error_position) + Vec2{ text_left, text_top - scroll }, BG_RED | FG_BLACK);
        }
        
        if (selection_end_index != cursor_index)
        {
            Vec2 selection_pos = selection_end_position;
            Vec2 end_pos = cursor_position;
            if (cursor_index < selection_end_index)
            {
                selection_pos = cursor_position;
                end_pos = selection_end_position;
            }
            while (selection_pos.y != end_pos.y)
            {
                ctx.fillColour(selection_pos + Vec2{ text_left, text_top - scroll }, Vec2{ text_content_width - selection_pos.x, 1 }, 2);
                ++selection_pos.y;
                selection_pos.x = 0;
            }
            ctx.fillColour(selection_pos + Vec2{ text_left, text_top - scroll }, Vec2{ end_pos.x - selection_pos.x + 1, 1 }, 2);
        }
        ctx.drawColour(cursor_position + Vec2{ text_left, text_top - scroll }, 1);
    }

    // scrollbar
    pushSubtextPalette(ctx);
    ctx.draw(scrollbar_start, 0xC2);
    ctx.fill(scrollbar_start + Vec2{ 0, 1 }, Vec2{ 1, scrollbar_height - 2 }, 0xB3);
    ctx.draw(scrollbar_start + Vec2{ 0, scrollbar_height - 1 }, 0xC1);
    const float start_fraction = static_cast<float>(scroll) / static_cast<float>(lines.size());
    const float end_fraction = min(static_cast<float>(scroll + ctx.getSize().y - 5) / static_cast<float>(lines.size()), 1.0f);
    const int start_y = static_cast<int>(ceil(start_fraction * static_cast<float>(scrollbar_height)));
    int end_y = static_cast<int>(floor((end_fraction - start_fraction) * static_cast<float>(scrollbar_height))) + start_y;
    if (end_y >= scrollbar_height && end_fraction < 1.0f)
        end_y = scrollbar_height - 1;
    ctx.fill(scrollbar_start + Vec2{ 0, start_y }, Vec2{ 1, min(end_y - start_y, scrollbar_height) }, 0xDB);
    if (start_y == 0)
        ctx.draw(scrollbar_start + Vec2{ 0, start_y }, 0xDC);
    if (end_y == scrollbar_height)
        ctx.draw(scrollbar_start + Vec2{ 0, end_y - 1 }, 0xDF);
    ctx.popPalette();

    // help/status bar
    pushSubtextPalette(ctx);
    ctx.drawText({ 1, text_box_bottom }, info_text, 0, 0, enable_animations ? info_text_limit : -1);
    if (enable_animations)
        info_text_limit = min(info_text_limit + 8, info_text.size());
    const string words_count = to_string(countWords()) + " words.";
    ctx.drawText(Vec2{ ctx.getSize().x - static_cast<int>(words_count.size() + 1), text_box_bottom }, words_count);
    if (show_hints)
        ctx.drawText({ 1, text_box_bottom + 1 }, "(Ctrl + H)elp  (F)igure  (C)itation  (B)old  (I)talic  (M)ath  (X)code  (S)ection  (R)eference section");
    ctx.popPalette();
    
    if (popup_state != INACTIVE && popup_index != FIND)
        ctx.popPalette();

    if (popup_state != INACTIVE && popup_state != ACTIVE && !enable_animations)
    {
        popup_state = (popup_state == OPENING) ? ACTIVE : INACTIVE;
        popup_timer = 0.0f;
    }
    
    // popup
    if (popup_state != INACTIVE)
    {
        Vec2 size = ctx.getSize() - Vec2{ 12, 6 };
        switch (popup_index)
        {
        case UNSAVED_CONFIRM:
            size.y = 8;
            break;
        case FIND:
            size.y = 5;
            break;
        case SPLASH:
            size.y = 19;
            break;
        case INSERT_FIGURE:
            size.y = 10;
            break;
        }
        
        if (popup_state != ACTIVE)
        {
            popup_timer = max(popup_timer - 0.33f, 0.0f);
            if (popup_state == OPENING)
                size.y = static_cast<int>((3.0f * popup_timer) + (static_cast<float>(size.y) * (1.0f - popup_timer)));
            else if (popup_state == CLOSING)
                size.y = static_cast<int>((static_cast<float>(size.y) * popup_timer) + (3.0f * (1.0f - popup_timer)));
            if (popup_timer <= 0.0f)
            {
                if (popup_state == OPENING)
                    popup_state = ACTIVE;
                else if (popup_state == CLOSING)
                {
                    if (!stacked_popups.empty())
                    {
                        popup_state = ACTIVE;
                        popup_index = *(stacked_popups.end() - 1);
                        stacked_popups.pop_back();
                        keyEvent(KeyEvent{ -1, true });
                    }
                    else
                        popup_state = INACTIVE;
                }
            }
        }
        Vec2 position = ((ctx.getSize() - size) / 2);
        if (popup_index == FIND)
            position.y = 2;
        
        ctx.pushBounds(position, position + size);
        ctx.drawBox(Vec2{ 0, 0 }, ctx.getSize());
        ctx.fill(Vec2{ 1, 1 }, ctx.getSize() - Vec2{ 2, 2 }, ' ');

        if (popup_state == ACTIVE)
        {
            switch (popup_index)
            {
            case SPLASH: drawPopupHello(ctx); break;
            case HELP: drawPopupHelp(ctx); break;
            case INSERT_FIGURE: drawPopupFigure(ctx); break;
            case INSERT_CITATION: drawPopupCitation(ctx); break;
            case UNSAVED_CONFIRM: drawPopupUnsavedConfirm(ctx); break;
            case SETTINGS: drawPopupSettings(ctx); break;
            case FIND: drawPopupFind(ctx); break;
            case PICKER: drawPopupPicker(ctx); break;
            default: break;
            }
            pushButtonPalette(ctx);
            ctx.drawText(Vec2{ ctx.getSize().x - 18, ctx.getSize().y - 1 }, "[ ESC to close ]");
            ctx.popPalette();
        }

        ctx.popBounds();
    }

    ctx.popPalette();
}

void EditorDrawable::pushTitlePalette(Context& ctx)
{
    ctx.pushPalette(Palette{
        BG_BLACK | FG_DARK_RED,
        FG_BLACK | BG_DARK_RED,
        FG_BLACK | BG_DARK_GREY
    });
}

void EditorDrawable::pushTextPalette(Context& ctx)
{
    ctx.pushPalette(Palette{
        BG_BLACK | FG_DARK_YELLOW,
        FG_BLACK | BG_DARK_YELLOW,
        FG_BLACK | BG_DARK_GREY
    });
}

void EditorDrawable::pushSubtextPalette(Context& ctx)
{
    ctx.pushPalette(Palette{
        BG_BLACK | FG_DARK_GREY,
        FG_BLACK | BG_DARK_GREY,
        FG_BLACK | BG_DARK_GREY
    });
}

void EditorDrawable::pushButtonPalette(Context& ctx)
{
    ctx.pushPalette(Palette{
        BG_BLACK | FG_DARK_MAGENTA,
        FG_BLACK | BG_DARK_MAGENTA,
        FG_BLACK | BG_DARK_GREY
    });
}
