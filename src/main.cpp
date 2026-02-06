#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

#include <strn.h>
#include <clipboardXX/clipboardxx.hpp>
#include <portable-file-dialogs/portable-file-dialogs.h>

const char* saturn_ascii = 
" .             "
" .##           "
"  #  ..        "
"     #####-    "
"   #########   "
"   ###.#####.  "
"    ###-####   "
"     .### .    "
"          # -# "
"            ## ";

using namespace STRN;
using namespace std;

bool isRenderable(int c)
{
    if (c >= ' ' && c <= '~')
        return true;
    return false;
}

struct Figure
{
    size_t start_offset;
    string identifier;
    string target_path;
};

size_t contextAwareFind(const string& find_target, const string& content, size_t start, size_t end)
{
    // TODO: make this aware of brackets, curly braces, and quotes
    size_t result = content.find(find_target, start);
    if (result >= end)
        return string::npos;
    return result;
}

struct Document
{
    string content;
    
    vector<Figure> figures;

    // TODO: document parsing
    void indexFigures()
    {
        figures.clear();
        size_t index = 0;
        while (index < content.size())
        {
            if (content[index] == '%')
            {
                static const char* fig_search = "%fig{";
                if (content.compare(index, strlen(fig_search), fig_search) == 0)
                {
                    size_t start_index = index + strlen(fig_search);
                    index = content.find('}', start_index);
                    size_t image_tag = contextAwareFind("image=", content, start_index, index);
                    size_t id_tag = contextAwareFind("id=", content, start_index, index);
                    if (image_tag == string::npos || id_tag == string::npos)
                        continue;
                    size_t img_end = min(contextAwareFind(";", content, image_tag, index), index);
                    string img = content.substr(image_tag + strlen("image="), img_end - (image_tag + strlen("image=")));
                    size_t id_end = min(contextAwareFind(";", content, id_tag, index), index);
                    string id = content.substr(id_tag + strlen("id="), id_end - (id_tag + strlen("id=")));
                    figures.emplace_back(start_index, id, img);
                }
            }
            ++index;
        }
    }
    
    void parseBibliography();
};

class EditorDrawable : public Drawable
{
private:
    string text_content = "%title{document}\n%config{columns=2;citations=harvard}\n\nLorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n\n%bib{}";
    vector<pair<string, bool>> lines;
    size_t cursor_index = 0;
    Vec2 cursor_position = { 0, 0 };
    size_t selection_end_index = 0;
    Vec2 selection_end_position = { 0, 0 };
    int scroll = 0;

    string info_text = "ready.";
    int listening_for_hotkey = 0;
    bool is_popup_active = true;
    int popup_index = -1;
    int popup_option_index = 0;

    vector<string> undo_history;
    vector<string> redo_history;
    int changes_since_push = 10000000;
    int last_change_type = 0; // 0 = regular character, 1 = delete
    chrono::steady_clock::time_point last_push;

    size_t last_counted_words = 0;
    chrono::steady_clock::time_point last_word_count;

    string file_path = "untitled.tmd";
    bool has_unsaved_changes = true;
    bool needs_save_as = true;

    bool show_line_checker = true;
    bool show_hints = true;
    float distortion = 0.03f;
    
    Document doc;

    // TODO: figure popup, citation popup and list/bibliography [120]
    // TODO: user settings (toggle line numbers, toggle hints, toggle distortion)
    // TODO: find tool [60]

    // TODO: concrete specification [120]
    // TODO: pdf generation [240]
    // TODO: syntax highlighting
    // TODO: section reference tag
    // TODO: ability to add custom font

public:
    EditorDrawable()
    { pushUndoHistory(); }

    // FIXME: reorganise code

    void textEvent(unsigned int chr)
    {
        if (is_popup_active)
            return;
        if (listening_for_hotkey > 1)
            return;
        if (listening_for_hotkey == 1)
        {
            listening_for_hotkey = 0;
            return;
        }

        if (chr != '\\')
        {
            if (selection_end_index != cursor_index)
            {
                checkUndoHistoryState(2);
                eraseSelection();
            }
            else
                checkUndoHistoryState(0);
            text_content.insert(text_content.begin() + cursor_index, (char)chr);
            ++cursor_index;
        }
        flagUnsaved();
        clearSelection();
        updateLines();
    }

    void keyEvent(KeyEvent& evt)
    {
        if (evt.pressed)
        {
            if (is_popup_active)
            {
                if (evt.key == 256)
                {
                    is_popup_active = false;
                    info_text = "ready.";
                }
                if (popup_index == -1)
                {
                    is_popup_active = false;
                    listening_for_hotkey = 1;
                    info_text = "ready.";
                }
                if (popup_index == 3)
                    keyEventPopupUnsavedConfirm(evt);
                else if (popup_index == 1)
                    keyEventPopupFigure(evt);
                return;
            }

            if (listening_for_hotkey == 1)
                listening_for_hotkey = 0;
            else if (listening_for_hotkey == 2)
            {
                handleHotkeyFollowup(evt);
                return;
            }
           
            switch (evt.key)
            {
            case 'S':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    triggerSave();
                }
                break;
            case 'O':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    if (has_unsaved_changes)
                    {
                        is_popup_active = true;
                        popup_index = 3;
                        popup_option_index = 1;
                    }
                    else
                        runFileOpenDialog();
                }
                break;
            case 'Z':
                if (evt.modifiers == KeyEvent::CTRL)
                    popUndoHistory();
                else if (evt.modifiers == (KeyEvent::CTRL | KeyEvent::SHIFT))
                    popRedoHistory();
                flagUnsaved();
                break;
            case 'H':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    is_popup_active = true;
                    popup_index = 0;
                    listening_for_hotkey = 0;
                    info_text = "showing help.";
                }
                break;
            case 'A':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    selection_end_index = 0;
                    cursor_index = text_content.size();
                }
                break;
            case 'C':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    string clipboard = getSelection();
                    clipboardxx::clipboard c;
                    c << clipboard;
                    info_text = "copied " + to_string(clipboard.size()) + " characters.";
                }
                break;
            case 'X':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    string clipboard = getSelection();
                    clipboardxx::clipboard c;
                    c << clipboard;
                    if (selection_end_index != cursor_index)
                    {
                        checkUndoHistoryState(2);
                        eraseSelection();
                    }
                    clearSelection();
                    flagUnsaved();
                    info_text = "cut " + to_string(clipboard.size()) + " characters.";
                }
                break;
            case 'V':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    string clipboard;
                    clipboardxx::clipboard c;
                    c >> clipboard; // FIXME: replace \r with \n, unless \r\n, including on file load!!
                    if ((selection_end_index != cursor_index) || clipboard.size() > 0)
                        checkUndoHistoryState(2);
                    if (selection_end_index != cursor_index)
                        eraseSelection();
                    clearSelection();
                    text_content.insert(cursor_index, clipboard);
                    cursor_index += clipboard.size();
                    clearSelection();
                    flagUnsaved();
                    info_text = "pasted " + to_string(clipboard.size()) + " characters.";
                }
                break;
            case '\\': // hotkey trigger
                listening_for_hotkey = 2;
                info_text = "waiting for hotkey...";
                break;
            case 259: // backspace
                if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                    selection_end_index = findPrevWord(cursor_index);
                if (selection_end_index != cursor_index)
                {
                    checkUndoHistoryState(2);
                    eraseSelection();
                }
                else if (cursor_index > 0)
                {
                    checkUndoHistoryState(1);
                    text_content.erase(text_content.begin() + cursor_index - 1);
                    --cursor_index;
                }
                clearSelection();
                flagUnsaved();
                break;
            case 261: // delete
                if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                    cursor_index = findNextWord(cursor_index);
                if (selection_end_index != cursor_index)
                {
                    checkUndoHistoryState(2);
                    eraseSelection();
                }
                else
                {
                    checkUndoHistoryState(1);
                    text_content.erase(text_content.begin() + cursor_index);
                }
                clearSelection();
                flagUnsaved();
                break;
            case 263: // left arrow
                if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                    cursor_index = findPrevWord(cursor_index);
                else if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::ALT)
                    cursor_index = findStartOfLine(cursor_index);
                else if (cursor_index > 0)
                    --cursor_index;
                if (!(evt.modifiers & KeyEvent::SHIFT))
                    clearSelection();
                break;
            case 262: // right arrow
                if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                    cursor_index = findNextWord(cursor_index);
                else if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::ALT)
                    cursor_index = findEndOfLine(cursor_index);
                else if (cursor_index < text_content.size())
                    ++cursor_index;
                if (!(evt.modifiers & KeyEvent::SHIFT))
                    clearSelection();
                break;
            case 265: // up arrow
                if (evt.modifiers & KeyEvent::ALT)
                {
                    cursor_index = 0;
                    if (!(evt.modifiers & KeyEvent::SHIFT))
                        clearSelection();
                    break;
                }
                if (evt.modifiers & KeyEvent::CTRL)
                {
                    if (scroll > 0)
                    {
                        --scroll;
                        cursorRecedeLine();
                        clearSelection();
                    }
                    break;
                }

                cursorRecedeLine();
                if (!(evt.modifiers & KeyEvent::SHIFT))
                    clearSelection();
                break;
            case 264: // down arrow
                if (evt.modifiers & KeyEvent::ALT)
                {
                    cursor_index = text_content.size();
                    if (!(evt.modifiers & KeyEvent::SHIFT))
                        clearSelection();
                    break;
                }
                if (evt.modifiers & KeyEvent::CTRL)
                {
                    if (scroll + 1 < lines.size())
                    {
                        ++scroll;
                        cursorAdvanceLine();
                        clearSelection();
                    }
                    break;
                }

                cursorAdvanceLine();
                if (!(evt.modifiers & KeyEvent::SHIFT))
                    clearSelection();
                break;
            case 257: // enter/newline
                textEvent('\n');
                break;
            }
            updateLines();
        }
    }

    void render(Context& ctx) override
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

    void updateLines()
    {
        doc.content = text_content;
        doc.indexFigures();
        // FIXME: this will need to be way faster (skip recalculating lines where possible)

        // wrap text and calculate cursor jump info
        lines.clear();
        string line = "";
        for (char c : text_content)
        {
            if (c != '\n')
                line.push_back(c);

            if (c == '\n' || line.size() >= transform.size.x - 4)
            {
                lines.push_back({ line, c == '\n' });
                line.clear();
            }
        }
        lines.push_back({ line, true });

        // recalculate cursor position based on index
        cursor_position = calculatePosition(cursor_index);
        if (selection_end_index == cursor_index)
            selection_end_position = cursor_position;
        else
            selection_end_position = calculatePosition(selection_end_index);

        fixScroll();
    }

private:
    pair<size_t, size_t> getSelectionStartLength()
    {
        // inclusive!
        size_t min_index = selection_end_index;
        size_t max_index = cursor_index - 1;
        if (selection_end_index > cursor_index)
        {
            min_index = cursor_index;
            max_index = selection_end_index;
        }
        
        return { min_index, (max_index - min_index) + 1 };
    }

    void eraseSelection()
    {
        if (selection_end_index == cursor_index)
            return;

        auto [min_index, length] = getSelectionStartLength();

        cursor_index = min_index;

        text_content.erase(min_index, length);
    }

    string getSelection()
    {
        if (selection_end_index == cursor_index)
            return "";

        auto [min_index, length] = getSelectionStartLength();

        return text_content.substr(min_index, length);
    }

    Vec2 calculatePosition(size_t index)
    {
        Vec2 position = { 0, 0 };
        size_t i = index;
        size_t line_length = 0;
        if (position.y < lines.size())
        {
            line_length = lines[position.y].first.size();
            if (lines[position.y].second)
                ++line_length;
        }
        while (position.y < lines.size() && i >= line_length)
        {
            i -= line_length;
            ++position.y;

            if (position.y < lines.size())
            {
                line_length = lines[position.y].first.size();
                if (lines[position.y].second)
                    ++line_length;
            }
        }
        position.x = i;

        return position;
    }

    void fixScroll()
    {
        while (cursor_position.y - scroll >= transform.size.y - 5)
            ++scroll;
        while (cursor_position.y - scroll < 0 && scroll > 0)
            --scroll;
    }

    void cursorAdvanceLine()
    {
        if (cursor_position.y + 1 < lines.size())
        {
            cursor_index -= cursor_position.x;
            cursor_index += lines[cursor_position.y].first.size();
            if (lines[cursor_position.y].second)
                ++cursor_index;
            cursor_index += min(cursor_position.x, (int)lines[cursor_position.y + 1].first.size());
        }
        else
            cursor_index = text_content.size();
    }

    void cursorRecedeLine()
    {
        if (cursor_position.y > 0)
        {
            cursor_index -= cursor_position.x;
            cursor_index -= lines[cursor_position.y - 1].first.size();
            if (lines[cursor_position.y - 1].second)
                --cursor_index;
            cursor_index += min(cursor_position.x, (int)lines[cursor_position.y - 1].first.size());
        }
        else
            cursor_index = 0;
    }

    void handleHotkeyFollowup(KeyEvent& evt)
    {
        info_text = "ready.";
        switch (evt.key)
        {
        case 'C':
            is_popup_active = true;
            popup_index = 2;
            listening_for_hotkey = 0;
            info_text = "showing citation selector.";

            //checkUndoHistoryState(2);
            //text_content.insert(cursor_index, "%cite{}");
            //cursor_index += strlen("%cite{}"); // TODO: citation
            //flagUnsaved();
            break;
        case 'B':
            checkUndoHistoryState(2);
            surroundSelection('*');
            flagUnsaved();
            break;
        case 'I':
            checkUndoHistoryState(2);
            surroundSelection('_');
            flagUnsaved();
            break;
        case 'F':
            if (needs_save_as)
            {
                info_text = "document must be saved before figures can be inserted.";
                break;
            }
            
            is_popup_active = true;
            popup_index = 1;
            listening_for_hotkey = 0;
            info_text = "showing figure selector.";
            break;
        case 'M':
            checkUndoHistoryState(2);
            text_content.insert(cursor_index, "%math{}");
            cursor_index += strlen("%math{");
            clearSelection();
            flagUnsaved();
            break;
        case 'X':
            checkUndoHistoryState(2);
            text_content.insert(cursor_index, "%code{}");
            cursor_index += strlen("%code{");
            clearSelection();
            flagUnsaved();
            break;
        case '\\':
            if (selection_end_index != cursor_index)
            {
                checkUndoHistoryState(2);
                eraseSelection();
            }
            else
                checkUndoHistoryState(0);
            text_content.insert(text_content.begin() + cursor_index, '\\');
            ++cursor_index;
            clearSelection();
            flagUnsaved();
            break;
        default:
            info_text = "unrecognised hotkey.";
            break;
        }
        listening_for_hotkey = 1;
        updateLines();
    }

    void surroundSelection(char c)
    {
        if (selection_end_index != cursor_index)
        {
            auto [start, length] = getSelectionStartLength();
            text_content.insert(text_content.begin() + start, c);
            text_content.insert(text_content.begin() + min(start + length + 1, text_content.size()), c);
            cursor_index += 2;
        }
        else
        {
            text_content.insert(cursor_index, 2, c);
            ++cursor_index;
        }
        clearSelection();
    }

    void clearSelection()
    {
        selection_end_index = cursor_index;
        selection_end_position = cursor_position;
    }

    void checkUndoHistoryState(int change_type)
    {
        if (change_type != -1)
            redo_history.clear();

        if (change_type >= 0)
            ++changes_since_push;

        // 2 = cut/paste/delete block
        if (change_type == 2)
            pushUndoHistory();
        else if ((change_type != -1) && (change_type != last_change_type) && (changes_since_push > 5))
            pushUndoHistory();
        else if (changes_since_push > 10)
            pushUndoHistory();
        else if (changes_since_push == 0)
        {
            last_push = chrono::steady_clock::now();
        }
        else
        {
            chrono::duration<float> time = chrono::steady_clock::now() - last_push;
            if (time.count() > 60.0f)
                pushUndoHistory();
        }
    }

    void pushUndoHistory()
    {
        changes_since_push = 0;
        last_push = chrono::steady_clock::now();
        undo_history.push_back(text_content);
    }

    void popUndoHistory()
    {
        if (undo_history.empty())
            return;
        changes_since_push = 0;
        last_push = chrono::steady_clock::now();
        redo_history.push_back(text_content);
        text_content = *(undo_history.end() - 1);
        undo_history.pop_back();
        clearSelection();
    }

    void popRedoHistory()
    {
        if (redo_history.empty())
            return;
        changes_since_push = 0;
        last_push = chrono::steady_clock::now();
        undo_history.push_back(text_content);
        text_content = *(redo_history.end() - 1);
        redo_history.pop_back();
        clearSelection();
    }

    void pushTitlePalette(Context& ctx)
    {
        ctx.pushPalette(Palette{
                BG_BLACK | FG_DARK_RED,
                FG_BLACK | BG_DARK_RED,
                FG_BLACK | BG_DARK_GREY
            });
    }

    void pushTextPalette(Context& ctx)
    {
        ctx.pushPalette(Palette{
                BG_BLACK | FG_DARK_YELLOW,
                FG_BLACK | BG_DARK_YELLOW,
                FG_BLACK | BG_DARK_GREY
            });
    }

    void pushSubtextPalette(Context& ctx)
    {
        ctx.pushPalette(Palette{
                BG_BLACK | FG_DARK_GREY,
                FG_BLACK | BG_DARK_GREY,
                FG_BLACK | BG_DARK_GREY
            });
    }

    void pushButtonPalette(Context& ctx)
    {
        ctx.pushPalette(Palette{
                BG_BLACK | FG_DARK_MAGENTA,
                FG_BLACK | BG_DARK_MAGENTA,
                FG_BLACK | BG_DARK_GREY
            });
    }

    void drawPopupHello(Context& ctx)
    {
        pushTitlePalette(ctx);
        ctx.drawText(Vec2{ 2, 0 }, "[ SPLASH ]");
        ctx.popPalette();

        size_t offset = 0;
        for (int i = 0; i < 10; ++i)
        {
            ctx.drawText(Vec2{ 4, 4 + i }, saturn_ascii, 0, offset, 15);
            offset += 15;
        }

        ctx.drawText(Vec2{ 23,  4 }, ": typecetter ----------------------- :");
        ctx.drawText(Vec2{ 23,  5 }, ": a text-only scientific typesetting :");
        ctx.drawText(Vec2{ 23,  6 }, ": editor by cassette costen, powered :");
        ctx.drawText(Vec2{ 23,  7 }, ":                           by STRN. :");
        ctx.drawText(Vec2{ 23,  8 }, ":                                    :");
        ctx.drawText(Vec2{ 23,  9 }, ":                                    :");
        ctx.drawText(Vec2{ 23, 10 }, ":                                    :");
        ctx.drawText(Vec2{ 23, 11 }, ":                                    :");
        ctx.drawText(Vec2{ 23, 12 }, ":                                    :");
        ctx.drawText(Vec2{ 23, 13 }, ":                                    :");
        ctx.drawText(Vec2{ 23, 14 }, ":      press any key to begin.       :");
    }

    void drawPopupHelp(Context& ctx)
    {
        pushTitlePalette(ctx);
        ctx.drawText(Vec2{ 2, 0 }, "[ HELP ]");
        ctx.popPalette();

        ctx.drawText(Vec2{ 3, 2 },   "Ctrl + S         : save document");
        ctx.drawText(Vec2{ 3, 3 },   "Ctrl + O         : open document");
        ctx.drawText(Vec2{ 3, 4 },   "Ctrl + C         : copy");
        ctx.drawText(Vec2{ 3, 5 },   "Ctrl + V         : paste");
        ctx.drawText(Vec2{ 3, 6 },   "Ctrl + X         : cut");
        ctx.drawText(Vec2{ 3, 7 },   "Ctrl + Z         : undo");
        ctx.drawText(Vec2{ 3, 8 },   "Ctrl + Shift + Z : redo");
        ctx.drawText(Vec2{ 3, 9 },   "Ctrl + F         : find in text");
        ctx.drawText(Vec2{ 3, 10 },  "Ctrl + E         : show export popup");
        ctx.drawText(Vec2{ 3, 11 },  "Ctrl + H         : show help popup");

        ctx.drawText(Vec2{ 3, 13 }, "\\, F             : show figure dialog");
        ctx.drawText(Vec2{ 3, 14 }, "\\, C             : show citation dialog");
        ctx.drawText(Vec2{ 3, 15 }, "\\, B             : bold selection");
        ctx.drawText(Vec2{ 3, 16 }, "\\, I             : italic selection");
        ctx.drawText(Vec2{ 3, 17 }, "\\, M             : insert math block");
        ctx.drawText(Vec2{ 3, 18 }, "\\, X             : insert code block");
    }

    void drawPopupFigure(Context& ctx)
    {
        pushTitlePalette(ctx);
        ctx.drawText(Vec2{ 2, 0 }, "[ FIGURE SELECTOR ]");
        ctx.popPalette();

        pushButtonPalette(ctx);
        ctx.drawText(Vec2{ 3, 3 }, "[    INSERT NEW     ]", (popup_option_index == 0) ? 1 : 0);
        ctx.drawText(Vec2{ 3, 4 }, "[  REFERENCE NEXT   ]", (popup_option_index == 1) ? 1 : 0);
        ctx.drawText(Vec2{ 3, 5 }, "[  REFERENCE LAST   ]", (popup_option_index == 2) ? 1 : 0);
        ctx.drawText(Vec2{ 3, 6 }, "[ REFERENCE BY NAME ]", (popup_option_index == 3) ? 1 : 0);
        ctx.popPalette();

        string info_text;
        switch (popup_option_index)
        {
        case 0:
            info_text = "select a new image file to insert.";
            break;
        case 1:
            info_text = "reference the next figure after the cursor";
            break;
        case 2:
            info_text = "reference the last figure before the cursor.";
            break;
        case 3:
            info_text = "select an existing figure to reference.";
            break;
        }
        ctx.drawTextWrapped(Vec2{ 26, 3 }, info_text, 0, ctx.getSize().x - 29);
    }

    void keyEventPopupFigure(KeyEvent& evt)
    {
        if (evt.key == 265)
            popup_option_index = max(0, popup_option_index - 1);
        else if (evt.key == 264)
            popup_option_index = min(3, popup_option_index + 1);
        else if (evt.key == 257)
        {
            string inserted_text;
            if (popup_option_index == 0)
            {
                auto f = pfd::open_file("select figure to insert", "",
            { "Image Files (.png .jpg .jpeg .svg)", "*.png *.jpg *.jpeg *.svg",
                        "All Files", "*" },
                    pfd::opt::none);
                const auto result = f.result();
                if (result.empty())
                {
                    is_popup_active = false;
                    listening_for_hotkey = 1;
                    info_text = "nothing to insert.";
                    return;
                }
                auto path = filesystem::path(result[0]);
                auto doc_path = filesystem::path(file_path);
                inserted_text = "image=" + filesystem::relative(path, doc_path.parent_path()).string() + ";id=" + path.filename().string();
            }
            else if (popup_option_index == 1)
            {
                inserted_text = "ref_id=";
                for (auto it = doc.figures.rbegin(); it != doc.figures.rend(); ++it)
                {
                    if (it->start_offset < cursor_index)
                    {
                        if (it == doc.figures.rbegin())
                        {
                            is_popup_active = false;
                            listening_for_hotkey = 1;
                            info_text = "nothing to insert.";
                            return;
                        }
                        inserted_text += (it - 1)->identifier;
                    }
                }
                if (inserted_text == "ref_id=")
                {
                    auto first = doc.figures.begin();
                    if (first->start_offset > cursor_index)
                        inserted_text += first->identifier;
                    else
                    {
                        is_popup_active = false;
                        listening_for_hotkey = 1;
                        info_text = "nothing to insert.";
                        return;
                    }
                }
            }
            else if (popup_option_index == 2)
            {
                inserted_text = "ref_id=";
                for (auto it = doc.figures.begin(); it != doc.figures.end(); ++it)
                {
                    if (it->start_offset > cursor_index)
                    {
                        if (it == doc.figures.begin())
                        {
                            is_popup_active = false;
                            listening_for_hotkey = 1;
                            info_text = "nothing to insert.";
                            return;
                        }
                        inserted_text += (it - 1)->identifier;
                    }
                }
                if (inserted_text == "ref_id=")
                {
                    auto last = doc.figures.end() - 1;
                    if (last->start_offset < cursor_index)
                        inserted_text += last->identifier;
                    else
                    {
                        is_popup_active = false;
                        listening_for_hotkey = 1;
                        info_text = "nothing to insert.";
                        return;
                    }
                }
            }
            else if (popup_option_index == 3)
            {
                // TODO: show a list of figures to choose from
            }
            else
                return;
            inserted_text = "%fig{" + inserted_text + "}";
            text_content.insert(cursor_index, inserted_text);
            cursor_index += inserted_text.size();
            clearSelection();
            checkUndoHistoryState(2);
            flagUnsaved();
            updateLines();
            is_popup_active = false;
            listening_for_hotkey = 1;
            info_text = "ready.";
        }
    }

    void drawPopupCitation(Context& ctx)
    {
        pushTitlePalette(ctx);
        ctx.drawText(Vec2{ 2, 0 }, "[ CITATION SELECTOR ]");
        ctx.popPalette();
    }

    void drawPopupUnsavedConfirm(Context& ctx)
    {
        pushTitlePalette(ctx);
        ctx.drawText(Vec2{ 2, 0 }, "[ UNSAVED CHANGES ]");
        ctx.popPalette();

        ctx.drawText(Vec2{ 3, 3 }, "you have unsaved changes in the current document.");
        ctx.drawText(Vec2{ 3, 4 }, "do you want to save them before continuing?");

        pushButtonPalette(ctx);
        ctx.drawText(Vec2{ 2, ctx.getSize().y - 1 }, "[ DISCARD CHANGES ]", (popup_option_index == 0) ? 1 : 0);
        ctx.drawText(Vec2{ 24, ctx.getSize().y - 1 }, "[ SAVE CHANGES ]", (popup_option_index == 1) ? 1 : 0);
        ctx.popPalette();
    }

    void keyEventPopupUnsavedConfirm(KeyEvent& evt)
    {
        if (evt.key == 263)
            popup_option_index = 0;
        else if (evt.key == 262)
            popup_option_index = 1;
        else if (evt.key == 257)
        {
            if (popup_option_index == 0)
                runFileOpenDialog();
            else
            {
                triggerSave();
                runFileOpenDialog();
            }
            is_popup_active = false;
            info_text = "ready.";
            updateLines();
        }
    }

    string getMemorySize(size_t bytes)
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

    int getCharacterType(size_t index)
    {
        char c = text_content[index];
        if (c >= 'a' && c <= 'z')
            return 0;
        if (c >= 'A' && c <= 'Z')
            return 0;
        if (c >= '0' && c <= '9')
            return 0;
        if (c == ' ' || c == '\t' || c == '\n')
            return 1;
        return 2;
    }

    size_t findNextWord(size_t current)
    {
        int current_type = getCharacterType(current);
        while ((current < text_content.size()) && ((getCharacterType(current) == current_type) || (getCharacterType(current) == 1)))
        {
            if (getCharacterType(current) == 1)
                current_type = 1;
            ++current;
        }
        return current;
    }

    size_t findPrevWord(size_t current)
    {
        if (current <= 1)
            return 0;

        int current_type = getCharacterType(current);
        if (getCharacterType(current - 1) != getCharacterType(current) && getCharacterType(current - 1) != 1)
        {
            --current;
            current_type = getCharacterType(current);
        }
        else if (getCharacterType(current - 1) == 1)
            --current;
        while (current > 0)
        {
            if (getCharacterType(current - 1) != current_type && getCharacterType(current) != 1)
                break;
            --current;
        }
        return current;
    }

    size_t findEndOfLine(size_t current)
    {
        while (current < text_content.size() && text_content[current] != '\n')
            ++current;
        return current;
    }

    size_t findStartOfLine(size_t current)
    {
        if (current <= 1)
            return 0;

        if (text_content[current] == '\n')
        {
            if (text_content[current - 1] == '\n')
                return current;
            else --current;
        }

        while (current > 0 && text_content[current] != '\n')
            --current;
        if (current == 0)
            return current;
        return current + 1;
    }

    size_t countWords()
    {
        chrono::duration<float> since_last_count = chrono::steady_clock::now() - last_word_count;
        if (since_last_count.count() < 2.0f)
            return last_counted_words;

        size_t words = 0;
        size_t word_length = 0;
        size_t index = 0;
        while (index < text_content.size())
        {
            int char_type = getCharacterType(index);
            if (char_type != 0)
            {
                if (word_length > 0)
                {
                    ++words;
                    word_length = 0;
                }
            }
            else
            {
                ++word_length;
            }
            ++index;
        }
        if (word_length > 0)
            ++words;

        last_counted_words = words;
        last_word_count = chrono::steady_clock::now();
        return words;
    }

    void flagUnsaved()
    {
        has_unsaved_changes = true;
    }
    
    void runFileOpenDialog()
    {
        auto f = pfd::open_file("select file to open", "",
            { "Markdown Files (.tmd .md)", "*.tmd *.md",
              "Text Files (.txt .text)", "*.txt *.text",
              "All Files", "*" },
            pfd::opt::none);
        const auto result = f.result();
        if (!result.empty())
        {
            const string file = result[0];
            if (filesystem::is_regular_file(file))
            {
                ifstream file_stream(file, ios::ate);
                text_content.resize(file_stream.tellg());
                file_stream.seekg(ios::beg);
                file_stream.read(text_content.data(), text_content.size());
                undo_history.clear();
                redo_history.clear();
                pushUndoHistory();
                file_path = file;
                has_unsaved_changes = false;
                needs_save_as = false;
            }
            else
                info_text = "file is not a regular text file.";
        }
    }

    void triggerSave()
    {
        if (!needs_save_as)
        {
            ofstream file_stream(file_path);
            file_stream.write(text_content.data(), text_content.size());
            pushUndoHistory();
            has_unsaved_changes = false;
        }
        else
        {
            auto f = pfd::save_file("select file to save", file_path,
                { "Markdown Files (.tmd .md)", "*.tmd *.md",
                  "Text Files (.txt .text)", "*.txt *.text",
                  "All Files", "*" },
                pfd::opt::none);
            const string file = f.result();
            ofstream file_stream(file);
            file_stream.write(text_content.data(), text_content.size());
            pushUndoHistory();
            file_path = file;
            has_unsaved_changes = false;
            needs_save_as = false;
        }
    }
};

#include <Windows.h>
int main()
{
    FreeConsole();
    NativeRasteriser comp;
    comp.setPalette(Palette{
        DEFAULT_COLOUR,
        DEFAULT_INVERTED,
        FG_BLACK | BG_LIGHT_GREY
        });
    comp.setScaleFactor(2.0f);
    EditorDrawable* e = new EditorDrawable();
    comp.insertDrawable(e);

    constexpr std::chrono::duration<double> ideal_frame_time(1.0 / 24.0);
    
    while (true)
    {
        auto last = std::chrono::high_resolution_clock::now();
        if (comp.update())
            break;
        static Vec2 last_size = Vec2{ 0, 0 };
        if (comp.getSize() != last_size)
        {
            last_size = comp.getSize();
            e->setSize(last_size);
            e->setPosition({ 0, 0 });
            e->updateLines();
        }
        comp.render();
        comp.present();
        KeyEvent key = comp.getKeyEvent();
        while (key.key != 0)
        {
            e->keyEvent(key);
            key = comp.getKeyEvent();
        }
        unsigned int chr = comp.getCharEvent();
        while (chr != 0)
        {
            e->textEvent(chr);
            chr = comp.getCharEvent();
        }
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = now - last;        
        auto sleep_duration = ideal_frame_time - duration;
        std::this_thread::sleep_for(sleep_duration); // prevents us from spinning too hard
    }
}