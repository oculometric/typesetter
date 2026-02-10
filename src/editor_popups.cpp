#include "editor.h"

#include <fstream>
#include <filesystem>
#include <portable-file-dialogs/portable-file-dialogs.h>

using namespace STRN;
using namespace std;

static const char* saturn_ascii = 
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

void EditorDrawable::startPopup(PopupIndex i)
{
    if (popup_state == OPENING || popup_state == CLOSING)
        return;
    if (popup_state == ACTIVE)
        stacked_popups.push_back(popup_index);
    popup_state = OPENING;
    popup_timer = 1.0f;
    popup_index = i;
}

void EditorDrawable::stopPopup(const bool reject_next_input)
{
    if (popup_state == OPENING || popup_state == CLOSING)
        return;
    popup_state = CLOSING;
    popup_timer = 1.0f;
    if (reject_next_input)
        input_state = REJECT_NEXT_INPUT;
}

void EditorDrawable::drawPopupHello(Context& ctx)
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

void EditorDrawable::drawPopupHelp(Context& ctx)
{
    pushTitlePalette(ctx);
    ctx.drawText(Vec2{ 2, 0 }, "[ HELP ]");
    ctx.popPalette();

    ctx.drawText(Vec2{ 3,  2 }, "Ctrl + S         : save document");
    ctx.drawText(Vec2{ 3,  3 }, "Ctrl + O         : open document");
    ctx.drawText(Vec2{ 3,  4 }, "Ctrl + C         : copy");
    ctx.drawText(Vec2{ 3,  5 }, "Ctrl + V         : paste");
    ctx.drawText(Vec2{ 3,  6 }, "Ctrl + X         : cut");
    ctx.drawText(Vec2{ 3,  7 }, "Ctrl + Z         : undo");
    ctx.drawText(Vec2{ 3,  8 }, "Ctrl + Shift + Z : redo");
    ctx.drawText(Vec2{ 3,  9 }, "Ctrl + F         : find in text");
    ctx.drawText(Vec2{ 3, 10 }, "Ctrl + E         : show export popup");
    ctx.drawText(Vec2{ 3, 11 }, "Ctrl + H         : show help popup");

    ctx.drawText(Vec2{ 3, 13 }, "\\, F             : show figure dialog");
    ctx.drawText(Vec2{ 3, 14 }, "\\, C             : show citation dialog");
    ctx.drawText(Vec2{ 3, 15 }, "\\, B             : bold selection");
    ctx.drawText(Vec2{ 3, 16 }, "\\, I             : italic selection");
    ctx.drawText(Vec2{ 3, 17 }, "\\, M             : insert math block");
    ctx.drawText(Vec2{ 3, 18 }, "\\, X             : insert code block");
    ctx.drawText(Vec2{ 3, 19 }, "\\, S             : insert section marker");
    ctx.drawText(Vec2{ 3, 20 }, "\\, R             : insert section reference");
}

void EditorDrawable::drawPopupFigure(Context& ctx) const
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

    string summary_text;
    switch (popup_option_index)
    {
    case 0:
        summary_text = "select a new image file to insert.";
        break;
    case 1:
        summary_text = "reference the next figure after the cursor";
        break;
    case 2:
        summary_text = "reference the last figure before the cursor.";
        break;
    case 3:
        summary_text = "select an existing figure to reference.";
        break;
    default: break;
    }
    ctx.drawTextWrapped(Vec2{ 26, 3 }, summary_text, 0, ctx.getSize().x - 29);
}

void EditorDrawable::keyEventPopupFigure(const KeyEvent& evt)
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
                stopPopup();
                setStatusText("nothing to insert.");
                return;
            }
            const auto path = filesystem::path(result[0]);
            const auto doc_path = filesystem::path(file_path);
            inserted_text = "%fig{image=\"" + filesystem::relative(path, doc_path.parent_path()).string() + "\";id=" + doc.getUniqueID(result[0]);
        }
        else if (popup_option_index == 1)
        {
            inserted_text = "%figref{id=";
            for (auto it = doc.figures.rbegin(); it != doc.figures.rend(); ++it)
            {
                if (it->start_offset < cursor_index)
                {
                    if (it == doc.figures.rbegin())
                    {
                        stopPopup();
                        setStatusText("nothing to insert.");
                        return;
                    }
                    inserted_text += (it - 1)->identifier;
                }
            }
            if (inserted_text == "%figref{id=")
            {
                const auto first = doc.figures.begin();
                if (first->start_offset > cursor_index)
                    inserted_text += first->identifier;
                else
                {
                    stopPopup();
                    setStatusText("nothing to insert.");
                    return;
                }
            }
        }
        else if (popup_option_index == 2)
        {
            inserted_text = "%figref{id=";
            for (auto it = doc.figures.begin(); it != doc.figures.end(); ++it)
            {
                if (it->start_offset > cursor_index)
                {
                    if (it == doc.figures.begin())
                    {
                        stopPopup();
                        setStatusText("nothing to insert.");
                        return;
                    }
                    inserted_text += (it - 1)->identifier;
                }
            }
            if (inserted_text == "%figref{id=")
            {
                const auto last = doc.figures.end() - 1;
                if (!doc.figures.empty() && last->start_offset < cursor_index)
                    inserted_text += last->identifier;
                else
                {
                    stopPopup();
                    setStatusText("nothing to insert.");
                    return;
                }
            }
        }
        else if (popup_option_index == 3)
        {
            sub_popup_passthrough = 0;
            popup_option_index = 0;
            startPopup(PICKER);
            return;
        }
        else
            return;
        inserted_text =  inserted_text + "}";
        insertReplace(inserted_text);
        updateLines();
        stopPopup();
        setStatusText("ready.");
    }
    else if (evt.key == -1)
    {
        if (sub_popup_passthrough == -1 || sub_popup_passthrough >= doc.figures.size())
            return;
        insertReplace("%figref{id=" + doc.figures[sub_popup_passthrough].identifier + "}");
        updateLines();
        stopPopup();
        setStatusText("ready.");
    }
}

void EditorDrawable::drawPopupCitation(Context& ctx)
{
    pushTitlePalette(ctx);
    ctx.drawText(Vec2{ 2, 0 }, "[ CITATION SELECTOR ]");
    ctx.popPalette();
}

void EditorDrawable::drawPopupUnsavedConfirm(Context& ctx) const
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

void EditorDrawable::keyEventPopupUnsavedConfirm(const KeyEvent& evt)
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
        stopPopup();
        setStatusText("ready.");
        updateLines();
    }
}

void EditorDrawable::drawPopupSettings(Context& ctx) const
{
    pushTitlePalette(ctx);
    ctx.drawText(Vec2{ 2, 0 }, "[ SETTINGS ]");
    ctx.popPalette();
    
    pushButtonPalette(ctx);
    static const string enabled = "-\xFE  [ ON  ]";
    static const string disabled = "\xFE-  [ OFF ]";
    ctx.drawText(Vec2{ 3, 3 }, (show_line_checker ? enabled : disabled) + " - line checker", (popup_option_index == 0) ? 1 : 0);
    ctx.drawText(Vec2{ 3, 4 }, (show_hints ? enabled : disabled) + " - hotkey hints", (popup_option_index == 1) ? 1 : 0);
    ctx.drawText(Vec2{ 3, 5 }, (enable_animations ? enabled : disabled) + " - UI animations", (popup_option_index == 2) ? 1 : 0);
    
#if defined(GUI)
    string distortion_str(5, '\xC4');
    distortion_str[distortion] = '\xFE';
    distortion_str += format("  [ {:.2f} ]", distortion_options[distortion]);
    ctx.drawText(Vec2{ 3, 7 }, distortion_str + " - screen distortion", (popup_option_index == 3) ? 1 : 0);
#endif
    ctx.popPalette();
}

void EditorDrawable::keyEventPopupSettings(const KeyEvent& evt)
{
    if (evt.key == 265)
        popup_option_index = max(0, popup_option_index - 1);
    else if (evt.key == 264)
#if defined(GUI)
        popup_option_index = min(3, popup_option_index + 1);
#else
        popup_option_index = min(2, popup_option_index + 1);
#endif
    else if (evt.key == 263 || evt.key == 262 || evt.key == 257)
    {
        bool* setting = nullptr;
        switch (popup_option_index)
        {
        case 0: setting = &show_line_checker; break;
        case 1: setting = &show_hints; break;
        case 2: setting = &enable_animations; break;
        case 3: 
            if (evt.key == 262)
                distortion = min(4, distortion + 1);
            else if (evt.key == 263)
                distortion = max(0, distortion - 1);
            break;
        default: break;
        }
        if (setting == nullptr)
            return;
        if (evt.key == 262)
            *setting = true;
        else if (evt.key == 263)
            *setting = false;
        else if (evt.key == 257)
            *setting = !(*setting);
       updateLines();
    }
}

void EditorDrawable::drawPopupFind(Context& ctx) const
{
    pushTitlePalette(ctx);
    ctx.drawText(Vec2{ 2, 0 }, "[ FIND ]");
    ctx.popPalette();
    
    ctx.drawText(Vec2{ 3, 2 }, "> " + find_str, 0, 0, ctx.getSize().x - 4);
    ctx.draw(Vec2{ 5 + static_cast<int>(find_str.size()), 2 }, ' ', 1);
    
    pushButtonPalette(ctx);
    ctx.drawText(Vec2{ 2, ctx.getSize().y - 1 }, "[ ENTER/SHIFT+ENTER TO ADVANCE ]");
    ctx.popPalette();
}

void EditorDrawable::textEventPopupFind(unsigned int chr)
{
    find_str.push_back(chr);
}

void EditorDrawable::keyEventPopupFind(const KeyEvent& evt)
{
    if (evt.key == 257)
    {
        if (find_str.empty())
            return;
        
        size_t offset;
        if (evt.modifiers == KeyEvent::SHIFT)
        {
            if (cursor_index > 0)
                offset = text_content.rfind(find_str, cursor_index - 1);
            else
                offset = string::npos;
        }
        else
            offset = text_content.find(find_str, cursor_index + 1);
        
        if (offset == string::npos)
            setStatusText("nothing more to find.");
        else
        {
            cursor_index = offset;
            cursor_position = calculatePosition(cursor_index);
            clearSelection();
        }
    }
    else if (evt.key == 259)
    {
        if (!find_str.empty())
            find_str.pop_back();
    }
}

void EditorDrawable::drawPopupPicker(Context& ctx) const
{
    pushTitlePalette(ctx);
    string title = "[ SELECTOR ]";
    switch (sub_popup_passthrough)
    {
    case 0: title = "[ FIGURE SELECTOR ]"; break;
    case 1: title = "[ SECTION SELECTOR ]"; break;
    }
    ctx.drawText(Vec2{ 2, 0 }, title);
    ctx.popPalette();
    
    if (sub_popup_passthrough == 0)
    {
        pushButtonPalette(ctx);
        int y = 3;
        for (size_t i = popup_option_index; i < doc.figures.size(); ++i)
        {
            if (y >= ctx.getSize().y - 4)
                break;
            ctx.drawText(Vec2{ 3, y }, "[ " + doc.figures[i].target_path + " ]", i == popup_option_index);
            ++y;
        }
        ctx.popPalette();
        pushSubtextPalette(ctx);
        ctx.drawText(Vec2{ 3, y }, "end of list");
        ctx.popPalette();
    }
    else if (sub_popup_passthrough == 1)
    {
        pushButtonPalette(ctx);
        int y = 3;
        for (size_t i = popup_option_index; i < doc.sections.size(); ++i)
        {
            if (y >= ctx.getSize().y - 4)
                break;
            ctx.drawText(Vec2{ 3, y }, "[ " + doc.sections[i].identifier + " ]", i == popup_option_index);
            ++y;
        }
        ctx.popPalette();
        pushSubtextPalette(ctx);
        ctx.drawText(Vec2{ 3, y }, "end of list");
        ctx.popPalette();
    }
}

void EditorDrawable::keyEventPopupPicker(const KeyEvent& evt)
{
    size_t array_size = 0;
    switch (sub_popup_passthrough)
    {
    case 0: array_size = doc.figures.size(); break;
    case 1: array_size = doc.sections.size(); break;
    }
    
    if (evt.key == 265)
        popup_option_index = max(0, popup_option_index - 1);
    else if (evt.key == 264)
        popup_option_index = min((array_size == 0) ? 0 : static_cast<int>(array_size) - 1, popup_option_index + 1);
    else if (evt.key == 257)
    {
        if (sub_popup_passthrough == 1)
        {
            insertReplace("%sectref{id=" + doc.sections[popup_option_index].identifier + "}");
            updateLines();
        }
        sub_popup_passthrough = popup_option_index;
        stopPopup();
    }
}
