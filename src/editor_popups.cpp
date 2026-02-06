#include "editor.h"

#include <fstream>
#include <filesystem>
#include <portable-file-dialogs/portable-file-dialogs.h>

using namespace STRN;
using namespace std;

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

void EditorDrawable::keyEventPopupFigure(KeyEvent& evt)
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

void EditorDrawable::drawPopupCitation(Context& ctx)
{
    pushTitlePalette(ctx);
    ctx.drawText(Vec2{ 2, 0 }, "[ CITATION SELECTOR ]");
    ctx.popPalette();
}

void EditorDrawable::drawPopupUnsavedConfirm(Context& ctx)
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

void EditorDrawable::keyEventPopupUnsavedConfirm(KeyEvent& evt)
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
