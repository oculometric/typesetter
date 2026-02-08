#include "editor.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <clipboardXX/clipboardxx.hpp>
#include <portable-file-dialogs/portable-file-dialogs.h>

using namespace STRN;
using namespace std;

void EditorDrawable::textEvent(const unsigned int chr)
{
    if (popup_state != INACTIVE)
        return;
    if (input_state == REJECT_NEXT_INPUT)
    {
        input_state = NORMAL_INPUT;
        return;
    }
    else if (input_state != NORMAL_INPUT)
        return;

    if (chr != '\\')
        insertReplace(static_cast<char>(chr));
    updateLines();
}

void EditorDrawable::keyEvent(KeyEvent& evt)
{
    if (evt.pressed)
    {
        if (popup_state == ACTIVE)
        {
            if (evt.key == 256)
            {
                stopPopup(false);
                setStatusText("ready.");
            }
            switch (popup_index)
            {
            case SPLASH:
                stopPopup();
                setStatusText("ready.");
                break;
            case UNSAVED_CONFIRM:
                keyEventPopupUnsavedConfirm(evt);
                break;
            case INSERT_FIGURE:
                keyEventPopupFigure(evt);
                break;
            case SETTINGS:
                keyEventPopupSettings(evt);
                break;
            default: break;
            }
            return;
        }

        if (input_state == REJECT_NEXT_INPUT)
            input_state = NORMAL_INPUT;
        else if (input_state == WAITING_FOR_HOTKEY)
        {
            handleHotkeyFollowup(evt);
            return;
        }
        
        if (evt.modifiers & KeyEvent::CTRL)
            handleCtrlShortcut(evt);
           
        switch (evt.key)
        {
        case '\\': // hotkey trigger
            input_state = WAITING_FOR_HOTKEY;
            setStatusText("waiting for hotkey...");
            break;
        case 259: // backspace
            if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                selection_end_index = findPrevWord(cursor_index);
            if (selection_end_index != cursor_index)
                eraseSelection();
            else if (cursor_index > 0)
            {
                erase(cursor_index - 1);
                --cursor_index;
                clearSelection();
            }
            break;
        case 261: // delete
            if ((evt.modifiers & ~KeyEvent::SHIFT) == KeyEvent::CTRL)
                cursor_index = findNextWord(cursor_index);
            if (selection_end_index != cursor_index)
                eraseSelection();
            else
                erase(cursor_index);
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
                if (scroll + 1 < static_cast<int>(lines.size()))
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
        default: break;
        }
        updateLines();
    }
}

void EditorDrawable::handleCtrlShortcut(const KeyEvent& evt)
{
    switch (evt.key)
    {
    case ',':
        startPopup(SETTINGS);
        break;
    case 'S':
        triggerSave();
        break;
    case 'O':
        if (has_unsaved_changes)
        {
            startPopup(UNSAVED_CONFIRM);
            popup_option_index = 1;
        }
        else
            runFileOpenDialog();
        break;
    case 'Z':
        popUndoHistory();
        if ((evt.modifiers & ~KeyEvent::CTRL) == KeyEvent::SHIFT)
            popRedoHistory();
        flagUnsaved();
        break;
    case 'H':
        startPopup(HELP);
        setStatusText("showing help.");
        break;
    case 'A':
        selection_end_index = 0;
        cursor_index = text_content.size();
        break;
    case 'C':
        {
        const string clipboard = getSelection();
        const clipboardxx::clipboard c;
        c.copy(clipboard);
        setStatusText("copied " + to_string(clipboard.size()) + " characters.");
        }
        break;
    case 'X':
        {
        const string clipboard = getSelection();
        const clipboardxx::clipboard c;
        c.copy(clipboard);
        if (selection_end_index != cursor_index)
        {
            checkUndoHistoryState(CHANGE_BLOCK);
            eraseSelection();
        }
        clearSelection();
        flagUnsaved();
        setStatusText("cut " + to_string(clipboard.size()) + " characters.");
        }
        break;
    case 'V':
        {
        const clipboardxx::clipboard c;
        string clipboard = c.paste();
        fixRN(clipboard);
        insertReplace(clipboard);
        setStatusText("pasted " + to_string(clipboard.size()) + " characters.");
        }
        break;
    default: break;
    }
}

void EditorDrawable::handleHotkeyFollowup(const KeyEvent& evt)
{
    setStatusText("ready.");
    switch (evt.key)
    {
    case 'C':
        startPopup(INSERT_CITATION);
        setStatusText("showing citation selector.");

        //checkUndoHistoryState(2);
        //text_content.insert(cursor_index, "%cite{}");
        //cursor_index += strlen("%cite{}"); // TODO: citation
        //flagUnsaved();
        break;
    case 'B':
        surroundSelection('*');
        break;
    case 'I':
        surroundSelection('_');
        break;
    case 'F':
        if (needs_save_as)
        {
            setStatusText("document must be saved before figures can be inserted.");
            break;
        }
        startPopup(INSERT_FIGURE);
        setStatusText("showing figure selector.");
        break;
    case 'M':
        insertReplace("%math{}");
        --cursor_index;
        clearSelection();
        break;
    case 'X':
        insertReplace("%code{}");
        --cursor_index;
        clearSelection();
        break;
    case 'S':
        insertReplace("%section{id=\"\"}");
        cursor_index -= 2;
        clearSelection();
        break;
    case 'R':
        insertReplace("%sectref{id=\"\"}");
        cursor_index -= 2;
        clearSelection();
        break;
    case '\\':
        insertReplace('\\');
        break;
    default:
        setStatusText("unrecognised hotkey.");
        break;
    }
    input_state = REJECT_NEXT_INPUT;
    updateLines();
}

void EditorDrawable::triggerSave()
{
    if (!needs_save_as)
    {
        ofstream file_stream(file_path);
        file_stream.write(text_content.data(), static_cast<streamsize>(text_content.size()));
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
        file_stream.write(text_content.data(), static_cast<streamsize>(text_content.size()));
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
        const string& file = result[0];
        if (filesystem::is_regular_file(file))
        {
            ifstream file_stream(file, ios::ate);
            cursor_index = 0;
            clearSelection();
            text_content.clear();
            text_content.resize(file_stream.tellg());
            fixRN(text_content);
            file_stream.seekg(ios::beg);
            file_stream.read(text_content.data(), static_cast<streamsize>(text_content.size()));
            undo_history.clear();
            redo_history.clear();
            pushUndoHistory();
            file_path = file;
            has_unsaved_changes = false;
            needs_save_as = false;
        }
        else
            setStatusText("file is not a regular text file.");
    }
}
