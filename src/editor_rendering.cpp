#include "editor.h"

#include <fstream>
#include <filesystem>
#include <portable-file-dialogs/portable-file-dialogs.h>

using namespace STRN;
using namespace std;

static string getMemorySize(size_t bytes)
{
    if (bytes >= 2048ull * 1024 * 1024)
        return format("{0:.1f} GiB", (float)bytes / (1024ull * 1024 * 1024));
    else if (bytes >= 2048ull * 1024)
        return format("{0:.1f} MiB", (float)bytes / (1024ull * 1024));
    else if (bytes >= 2048ull)
        return format("{0:.1f} KiB", (float)bytes / 1024);
    else
        return format("{0} B", bytes);
}

void EditorDrawable::render(Context& ctx)
{
    checkUndoHistoryState(-1);

    int text_box_left = 1;
    if (!show_line_checker)
        --text_box_left;
    int text_box_right = ctx.getSize().x - 1;
    int text_box_top = 1;
    int text_box_bottom = ctx.getSize().y - 2;
    if (!show_hints)
        ++text_box_bottom;
    Vec2 text_box_size = { text_box_right - text_box_left, text_box_bottom - text_box_top };

    int text_content_height = text_box_size.y - 2;
    int text_left = text_box_left + 1;
    int text_top = text_box_top + 1;
    int text_content_width = text_box_size.x - 2;

    Vec2 scrollbar_start = Vec2{ text_box_right, 1 };
    int scrollbar_height = text_box_bottom - 1;

    pushTextPalette(ctx);

    if (is_popup_active)
        pushSubtextPalette(ctx);

    // header
    ctx.drawText({ 1, 0 }, "[ TYPECETTER ] - editing " + filesystem::path(file_path).filename().string());
    string file_size = getMemorySize(text_content.size());
    ctx.drawText(Vec2{ static_cast<int>(ctx.getSize().x - (file_size.size() + 2)), 0 }, file_size);
    ctx.draw({ ctx.getSize().x - 1, 0 }, 0x03);
    ctx.drawBox({ text_box_left, text_box_top }, text_box_size);
        
    // text content
    int actual_line = scroll + 1;
    bool show_next_line = true;
    for (int i = scroll; i < lines.size(); ++i)
    {
        if (i - scroll > text_content_height - 1)
            continue;
        ctx.drawText(Vec2{ text_left, i + text_top - scroll }, lines[i].first);
        if (show_line_checker)
            ctx.draw(Vec2{ 0, i + text_top - scroll }, (actual_line % 2) ? 0xB0 : 0xB2, 2);
        if (lines[i].second)
        {
            ++actual_line;
            show_next_line = true;
        }
        else
            show_next_line = false;
    }

    // cursor and selection
    if (!is_popup_active)
    {
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
    float start_fraction = static_cast<float>(scroll) / static_cast<float>(lines.size());
    float end_fraction = min(static_cast<float>(scroll + ctx.getSize().y - 5) / static_cast<float>(lines.size()), 1.0f);
    int start_y = static_cast<int>(ceil(start_fraction * scrollbar_height));
    int end_y = static_cast<int>(floor((end_fraction - start_fraction) * scrollbar_height)) + start_y;
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
    ctx.drawText({ 1, text_box_bottom }, info_text);
    string words_count = to_string(countWords()) + " words.";
    ctx.drawText({ 1, text_box_bottom }, info_text);
    ctx.drawText(Vec2{ ctx.getSize().x - static_cast<int>((words_count.size() + 1)), text_box_bottom }, words_count);
    if (show_hints)
        ctx.drawText({ 1, text_box_bottom + 1 }, "(Ctrl + H)elp  (F)igure  (C)itation  (B)old  (I)talic  (M)ath  (X)code");
    ctx.popPalette();
    
    if (is_popup_active)
    {
        ctx.popPalette();
    }

    // popup
    if (is_popup_active)
    {
        ctx.pushBounds(Vec2{ 6, 3 }, ctx.getSize() - Vec2{ 6, 3 });
        ctx.drawBox(Vec2{ 0, 0 }, ctx.getSize());
        ctx.fill(Vec2{ 1, 1 }, ctx.getSize() - Vec2{ 2, 2 }, ' ');

        switch (popup_index)
        {
        case -1: drawPopupHello(ctx); break;
        case 0: drawPopupHelp(ctx); break;
        case 1: drawPopupFigure(ctx); break;
        case 2: drawPopupCitation(ctx); break;
        case 3: drawPopupUnsavedConfirm(ctx); break;
        }
        pushButtonPalette(ctx);
        ctx.drawText(Vec2{ ctx.getSize().x - 18, ctx.getSize().y - 1 }, "[ ESC to close ]");
        ctx.popPalette();

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
