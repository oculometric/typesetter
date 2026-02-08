#include "editor.h"

using namespace STRN;
using namespace std;

void EditorDrawable::updateLines()
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

pair<size_t, size_t> EditorDrawable::getSelectionStartLength() const
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

void EditorDrawable::insertReplace(const string& str)
{
    if (selection_end_index != cursor_index)
        eraseSelection();
    else
        checkUndoHistoryState(CHANGE_BLOCK);
    text_content.insert(cursor_index, str);
    cursor_index += str.size();
    clearSelection();
    flagUnsaved();
}

void EditorDrawable::insertReplace(char c)
{
    if (selection_end_index != cursor_index)
        eraseSelection();
    else
        checkUndoHistoryState(CHANGE_REGULAR);
    insert(cursor_index, c);
    ++cursor_index;
    clearSelection();
    flagUnsaved();
}

void EditorDrawable::insert(size_t offset, char c)
{
    text_content.insert(text_content.begin() + offset, c);
    checkUndoHistoryState(CHANGE_REGULAR);
    flagUnsaved();
}

void EditorDrawable::erase(size_t offset)
{
    text_content.erase(text_content.begin() + offset);
    checkUndoHistoryState(CHANGE_DELETE);
    flagUnsaved();
}

void EditorDrawable::eraseSelection()
{
    if (selection_end_index == cursor_index)
        return;

    auto [min_index, length] = getSelectionStartLength();
    cursor_index = min_index;
    text_content.erase(min_index, length);
    checkUndoHistoryState(CHANGE_BLOCK);
    clearSelection();
    flagUnsaved();
}

string EditorDrawable::getSelection() const
{
    if (selection_end_index == cursor_index)
        return "";

    auto [min_index, length] = getSelectionStartLength();

    return text_content.substr(min_index, length);
}

void EditorDrawable::clearSelection()
{
    selection_end_index = cursor_index;
    selection_end_position = cursor_position;
}

void EditorDrawable::surroundSelection(char c)
{
    checkUndoHistoryState(CHANGE_BLOCK);
    if (selection_end_index != cursor_index)
    {
        auto [start, length] = getSelectionStartLength();
        insert(start, c);
        insert(min(start + length + 1, text_content.size()), c);
        cursor_index = start + length;
    }
    else
    {
        insert(cursor_index, c);
        insert(cursor_index, c);
        ++cursor_index;
    }
    clearSelection();
    flagUnsaved();
}

Vec2 EditorDrawable::calculatePosition(size_t index) const
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

void EditorDrawable::cursorAdvanceLine()
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

void EditorDrawable::cursorRecedeLine()
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

void EditorDrawable::fixScroll()
{
    while (cursor_position.y - scroll >= transform.size.y - 5)
        ++scroll;
    while (cursor_position.y - scroll < 0 && scroll > 0)
        --scroll;
}

void EditorDrawable::checkUndoHistoryState(ChangeType change_type)
{
    // FIXME: this is broken rn
    return;
    if (change_type != CHANGE_CHECK)
        redo_history.clear();

    if (change_type >= CHANGE_REGULAR)
        ++changes_since_push;

    // 2 = cut/paste/delete block
    if (change_type == CHANGE_BLOCK)
        pushUndoHistory();
    else if ((change_type != CHANGE_CHECK) && (change_type != last_change_type) && (changes_since_push > 5))
        pushUndoHistory();
    else if (changes_since_push > 10)
        pushUndoHistory();
    else if (changes_since_push == 0)
        last_push = chrono::steady_clock::now();
    else
    {
        chrono::duration<float> time = chrono::steady_clock::now() - last_push;
        if (time.count() > 60.0f)
            pushUndoHistory();
    }
}

void EditorDrawable::pushUndoHistory()
{
    changes_since_push = 0;
    last_push = chrono::steady_clock::now();
    undo_history.push_back(text_content);
}

void EditorDrawable::popUndoHistory()
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

void EditorDrawable::popRedoHistory()
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

int EditorDrawable::getCharacterType(size_t index) const
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

size_t EditorDrawable::findNextWord(size_t current) const
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

size_t EditorDrawable::findPrevWord(size_t current) const
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

size_t EditorDrawable::findEndOfLine(size_t current) const
{
    while (current < text_content.size() && text_content[current] != '\n')
        ++current;
    return current;
}

size_t EditorDrawable::findStartOfLine(size_t current) const
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

size_t EditorDrawable::countWords()
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

void EditorDrawable::fixRN(string& str)
{
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        if (*it == '\r')
        {
            if (it + 1 == str.end() || *(it + 1) != '\n')
                *it = '\n';
            else
            {
                ++it;
                str.erase(it - 1);
                --it;
            }
        }
    }
}
