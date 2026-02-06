#pragma once

#include <chrono>
#include <string>
#include <vector>
#include <strn.h>

#include "document.h"

class EditorDrawable : public STRN::Drawable
{
private:
    std::string text_content = "%title{document}\n%config{columns=2;citations=harvard}\n\nLorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n\n%bib{}";
    std::vector<std::pair<std::string, bool>> lines;
    size_t cursor_index = 0;
    STRN::Vec2 cursor_position = { 0, 0 };
    size_t selection_end_index = 0;
    STRN::Vec2 selection_end_position = { 0, 0 };
    int scroll = 0;

    std::string info_text = "ready.";
    int listening_for_hotkey = 0;
    bool is_popup_active = true;
    int popup_index = -1;
    int popup_option_index = 0;

    std::vector<std::string> undo_history;
    std::vector<std::string> redo_history;
    int changes_since_push = 10000000;
    int last_change_type = 0; // 0 = regular character, 1 = delete
    std::chrono::steady_clock::time_point last_push;

    size_t last_counted_words = 0;
    std::chrono::steady_clock::time_point last_word_count;

    std::string file_path = "untitled.tmd";
    bool has_unsaved_changes = true;
    bool needs_save_as = true;

    bool show_line_checker = true;
    bool show_hints = true;
    float distortion = 0.03f;
    
    Document doc;

    // TODO: citation popup and list/bibliography [120]
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

    void textEvent(unsigned int chr);

    void keyEvent(STRN::KeyEvent& evt);

    void render(STRN::Context& ctx) override;

    void updateLines();

private:
    std::pair<size_t, size_t> getSelectionStartLength() const;
    void eraseSelection();
    std::string getSelection() const;
    void clearSelection();
    void surroundSelection(char c);

    STRN::Vec2 calculatePosition(size_t index) const;
    void cursorAdvanceLine();
    void cursorRecedeLine();
    void fixScroll();

    void handleHotkeyFollowup(STRN::KeyEvent& evt);

    void checkUndoHistoryState(int change_type);
    void pushUndoHistory();
    void popUndoHistory();
    void popRedoHistory();

    static void pushTitlePalette(STRN::Context& ctx);
    static void pushTextPalette(STRN::Context& ctx);
    static void pushSubtextPalette(STRN::Context& ctx);
    static void pushButtonPalette(STRN::Context& ctx);

    void drawPopupHello(STRN::Context& ctx);
    void drawPopupHelp(STRN::Context& ctx);
    void drawPopupFigure(STRN::Context& ctx) const;
    void keyEventPopupFigure(STRN::KeyEvent& evt);
    void drawPopupCitation(STRN::Context& ctx);
    void drawPopupUnsavedConfirm(STRN::Context& ctx);
    void keyEventPopupUnsavedConfirm(STRN::KeyEvent& evt);

    int getCharacterType(size_t index) const;

    size_t findNextWord(size_t current) const;
    size_t findPrevWord(size_t current) const;
    size_t findEndOfLine(size_t current) const;
    size_t findStartOfLine(size_t current) const;

    size_t countWords();

    void flagUnsaved() { has_unsaved_changes = true; }
    void triggerSave();
    void runFileOpenDialog();
};