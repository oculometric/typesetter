#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <vector>

#include <strn.h>

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

class EditorDrawable : public Drawable
{
private:
    string text_content = "%title{document}\n\nLorem ipsum dolor sit amet.\n\n%bib{}";
    vector<pair<string, bool>> lines;
    size_t cursor_index = 0;
    Vec2 cursor_position = { 0, 0 };
    size_t selection_end_index = 0;
    Vec2 selection_end_position = { 0, 0 };
    int scroll = 0;

    string info_text = "ready.";
    int listening_for_hotkey = 0;
    bool is_popup_active = false;
    int popup_index = 0;

    // TODO: copy/cut/paste
    
    // TODO: cursor navigation (ctrl + right/left -> walk word, alt + right/left -> start/end line)
    // TODO: figure popup, citation popup and list/bibliography
    // TODO: load/save files
    // TODO: load popup on launch

    // TODO: more colours, custom themes
    // TODO: pdf generation
    // TODO: concrete specification

public:
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
                eraseSelection();
            text_content.insert(text_content.begin() + cursor_index, (char)chr);
            ++cursor_index;
        }
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
                // TODO: pass input to popup
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
            case 'H':
                if (evt.modifiers == KeyEvent::CTRL)
                {
                    is_popup_active = true;
                    popup_index = 0;
                    listening_for_hotkey = 0;
                    info_text = "showing help.";
                }
                break;
            case '\\': // hotkey trigger
                listening_for_hotkey = 2;
                info_text = "waiting for hotkey...";
                break;
            case 259: // backspace
                if (selection_end_index != cursor_index)
                    eraseSelection();
                else if (cursor_index > 0)
                {
                    text_content.erase(text_content.begin() + cursor_index - 1);
                    --cursor_index;
                }
                clearSelection();
                break;
            case 261: // delete
                if (selection_end_index != cursor_index)
                    eraseSelection();
                else
                    text_content.erase(text_content.begin() + cursor_index);
                clearSelection();
                break;
            case 263: // left arrow
                if (cursor_index > 0)
                    --cursor_index;
                if (!(evt.modifiers & KeyEvent::SHIFT))
                    clearSelection();
                break;
            case 262: // right arrow
                if (cursor_index < text_content.size())
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
        // header
        ctx.drawText({ 1, 0 }, "[ TYPESETTER ] - editing <filename>");
        ctx.drawBox({ 1, 1 }, ctx.getSize() - Vec2{ 2, 3 });
        
        // text content
        int actual_line = scroll + 1;
        bool show_next_line = true;
        for (int i = scroll; i < lines.size(); ++i)
        {
            if (i - scroll > ctx.getSize().y - 6)
                continue;

            ctx.drawText(Vec2{ 2, i + 2 - scroll }, lines[i].first);
            if (show_next_line)
                ctx.drawText(Vec2{ 0, i + 2 - scroll }, to_string(actual_line));
            if (lines[i].second)
            {
                ++actual_line;
                show_next_line = true;
            }
            else
                show_next_line = false;
        }

        // cursor and selection
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
                ctx.fillColour(selection_pos + Vec2{ 2, 2 - scroll }, Vec2{ ctx.getSize().x - 4, 1 }, 2);
                ++selection_pos.y;
                selection_pos.x = 0;
            }
            ctx.fillColour(selection_pos + Vec2{ 2, 2 - scroll }, Vec2{ end_pos.x - selection_pos.x + 1, 1 }, 2);
        }
        ctx.drawColour(cursor_position + Vec2{ 2, 2 - scroll }, 1);

        // scrollbar
        int scrollbar_height = ctx.getSize().y - 3;
        // FIXME: improve choice of chars for scrollbar
        ctx.draw(Vec2{ ctx.getSize().x - 1, 1 }, 0xD2);
        ctx.fill(Vec2{ ctx.getSize().x - 1, 2 }, Vec2{ 1, scrollbar_height - 2 }, 0xBA);
        ctx.draw(Vec2{ ctx.getSize().x - 1, scrollbar_height }, 0xD0);
        float start_fraction = static_cast<float>(scroll) / static_cast<float>(lines.size());
        float end_fraction = min(static_cast<float>(scroll + ctx.getSize().y - 5) / static_cast<float>(lines.size()), 1.0f);
        int start_y = static_cast<int>(ceil(start_fraction * scrollbar_height));
        int end_y = static_cast<int>(floor((end_fraction - start_fraction) * scrollbar_height)) + start_y;
        if (end_y >= scrollbar_height && end_fraction < 1.0f)
            end_y = scrollbar_height - 1;
        ctx.fill(Vec2{ ctx.getSize().x - 1, start_y + 1 }, Vec2{ 1, min(end_y - start_y, scrollbar_height) }, 0xDB);

        // help/status bar
        ctx.drawText({ 1, ctx.getSize().y - 2 }, info_text);
        ctx.drawText({ 1, ctx.getSize().y - 1 }, "help (Ctrl + H)  figure (F)  citation (C)  bold (B)  italic (I)  math (M)  code (X)");
    
        // popup
        if (is_popup_active)
        {
            ctx.pushBounds(Vec2{ 6, 3 }, ctx.getSize() - Vec2{ 6, 3 });
            ctx.drawBox(Vec2{ 0, 0 }, ctx.getSize());
            ctx.fill(Vec2{ 1, 1 }, ctx.getSize() - Vec2{ 2, 2 }, ' ');
            
            switch (popup_index)
            {
            case 0: drawPopupHelp(ctx); break;
            case 1: drawPopupFigure(ctx); break;
            case 2: drawPopupCitation(ctx); break;
            }
            ctx.drawText(Vec2{ ctx.getSize().x - 18, ctx.getSize().y - 1 }, "[ ESC to close ]");

            ctx.popBounds();
        }
    }

    void updateLines()
    {
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
    void eraseSelection()
    {
        if (selection_end_index == cursor_index)
            return;

        // inclusive!
        size_t min_index = selection_end_index;
        size_t max_index = cursor_index - 1;
        if (selection_end_index > cursor_index)
        {
            min_index = cursor_index + 1;
            max_index = selection_end_index;
        }
        else
            cursor_index = selection_end_index;

        text_content.erase(min_index, (max_index - min_index) + 1);
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

            //text_content.insert(cursor_index, "%cite{}");
            //cursor_index += strlen("%cite{}"); // TODO: citation
            break;
        case 'B':
            break; // TODO: bold, possibly selection
        case 'F':
            is_popup_active = true;
            popup_index = 1;
            listening_for_hotkey = 0;
            info_text = "showing figure selector.";

            //text_content.insert(cursor_index, "%fig{}");
            //cursor_index += strlen("%fig{}");
            break; // TODO: figure
        case 'I':
            break; // TODO: italic
        case 'M':
            text_content.insert(cursor_index, "%math{}");
            cursor_index += strlen("%math{}");
            break; // TODO: math
        case 'X':
            text_content.insert(cursor_index, "%code{}");
            cursor_index += strlen("%code{}");
            break; // TODO: code block
        case '\\':
            if (selection_end_index != cursor_index)
                eraseSelection();
            text_content.insert(text_content.begin() + cursor_index, '\\');
            ++cursor_index;
            clearSelection();
            break;
        default:
            info_text = "unrecognised hotkey.";
            break;
        }
        listening_for_hotkey = 1;
        updateLines();
    }

    void clearSelection()
    {
        selection_end_index = cursor_index;
        selection_end_position = cursor_position;
    }

    void drawPopupHelp(Context& ctx)
    {
        ctx.drawText(Vec2{ 2, 0 }, "[ HELP ]");
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
        ctx.drawText(Vec2{ 2, 0 }, "[ FIGURE SELECTOR ]");
    }

    void drawPopupCitation(Context& ctx)
    {
        ctx.drawText(Vec2{ 2, 0 }, "[ CITATION SELECTOR ]");
    }
};

int main()
{
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