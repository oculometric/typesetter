#include "editor.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <clipboardXX/clipboardxx.hpp>
#include <portable-file-dialogs/portable-file-dialogs.h>

using namespace STRN;
using namespace std;

bool isRenderable(int c)
{
    if (c >= ' ' && c <= '~')
        return true;
    return false;
}

void EditorDrawable::textEvent(unsigned int chr)
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

void EditorDrawable::keyEvent(KeyEvent& evt)
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

void EditorDrawable::handleHotkeyFollowup(STRN::KeyEvent& evt)
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

void EditorDrawable::triggerSave()
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

void EditorDrawable::runFileOpenDialog()
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
