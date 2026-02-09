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

    enum InputState : uint8_t
    {
        NORMAL_INPUT,
        REJECT_NEXT_INPUT,
        WAITING_FOR_HOTKEY
    };
    
    enum PopupIndex : int8_t
    {
        SPLASH = -1,
        HELP = 0,
        INSERT_FIGURE,
        INSERT_CITATION,
        UNSAVED_CONFIRM,
        SETTINGS,
        FIND,
        PICKER,
    };
    
    enum PopupState : uint8_t
    {
        INACTIVE = 0,
        ACTIVE = 1,
        OPENING = 2,
        CLOSING = 3
    };
    
    std::string info_text = "ready.";
    size_t info_text_limit = 0;
    InputState input_state = NORMAL_INPUT;
    PopupState popup_state = ACTIVE;
    float popup_timer = 0;
    PopupIndex popup_index = SPLASH;
    std::vector<PopupIndex> stacked_popups;
    int popup_option_index = 0;
    int sub_popup_passthrough = 0;
    std::string find_str;

    enum ChangeType : int8_t
    {
        CHANGE_CHECK = -1,
        CHANGE_REGULAR = 0,
        CHANGE_DELETE = 1,
        CHANGE_BLOCK = 2
    };
    
    std::vector<std::string> undo_history;
    std::vector<std::string> redo_history;
    int changes_since_push = 10000000;
    ChangeType last_change_type = CHANGE_REGULAR;
    std::chrono::steady_clock::time_point last_push;
    std::chrono::steady_clock::time_point last_change;

    size_t last_counted_words = 0;
    std::chrono::steady_clock::time_point last_word_count;

    std::string file_path = "untitled.tmd";
    bool has_unsaved_changes = true;
    bool needs_save_as = true;

    bool show_line_checker = true;
    bool show_hints = true;
    int distortion = 2;
    static constexpr float distortion_options[5] = { 0.0f, 0.01f, 0.03f, 0.06f, 0.1f };
    bool enable_animations = true;
    
    Document doc;

    // --- mandatory todos
    // TODO: section reference should present picker
    // TODO: citation popup and list/bibliography [120]
    // TODO: concrete specification [120]
    // TODO: pdf generation [240]
    // TODO: review undo history thing (probably broken)
    // TODO: figures should have captions
    
    // --- optional todos
    // TODO: word wrapping
    // TODO: syntax highlighting
    
    // --- STRN todos
    // TODO: ability to add custom font
    // TODO: icon
    // TODO: option for CLI version
    
public:
    EditorDrawable()
    { pushUndoHistory(); }

    void textEvent(unsigned int chr);
    void keyEvent(const STRN::KeyEvent& evt);

    void render(STRN::Context& ctx) override;

    void updateLines();
    float getDistortion() const { return distortion_options[distortion]; }

private:
    std::pair<size_t, size_t> getSelectionStartLength() const;
    void insertReplace(const std::string& str);
    void insertReplace(char c);
    void insert(size_t offset, char c);
    void erase(size_t offset);
    void eraseSelection();
    std::string getSelection() const;
    void clearSelection();
    void surroundSelection(char c);

    STRN::Vec2 calculatePosition(size_t index) const;
    void cursorAdvanceLine();
    void cursorRecedeLine();
    void fixScroll();

    void handleCtrlShortcut(const STRN::KeyEvent& evt);
    void handleHotkeyFollowup(const STRN::KeyEvent& evt);

    void checkUndoHistoryState(ChangeType change_type);
    void pushUndoHistory();
    void popUndoHistory();
    void popRedoHistory();

    static void pushTitlePalette(STRN::Context& ctx);
    static void pushTextPalette(STRN::Context& ctx);
    static void pushSubtextPalette(STRN::Context& ctx);
    static void pushButtonPalette(STRN::Context& ctx);

    void startPopup(PopupIndex i);
    void stopPopup(bool reject_next_input = true);
    static void drawPopupHello(STRN::Context& ctx);
    static void drawPopupHelp(STRN::Context& ctx);
    void drawPopupFigure(STRN::Context& ctx) const;
    void keyEventPopupFigure(const STRN::KeyEvent& evt);
    static void drawPopupCitation(STRN::Context& ctx);
    void drawPopupUnsavedConfirm(STRN::Context& ctx) const;
    void keyEventPopupUnsavedConfirm(const STRN::KeyEvent& evt);
    void drawPopupSettings(STRN::Context& ctx) const;
    void keyEventPopupSettings(const STRN::KeyEvent& evt);
    void drawPopupFind(STRN::Context& ctx) const;
    void textEventPopupFind(unsigned int chr);
    void keyEventPopupFind(const STRN::KeyEvent& evt);
    void drawPopupPicker(STRN::Context& ctx) const;
    void keyEventPopupPicker(const STRN::KeyEvent& evt);

    int getCharacterType(size_t index) const;

    size_t findNextWord(size_t current) const;
    size_t findPrevWord(size_t current) const;
    size_t findEndOfLine(size_t current) const;
    size_t findStartOfLine(size_t current) const;

    size_t countWords();
    static void fixRN(std::string& str);

    void setStatusText(const std::string& text);
    void flagUnsaved() { has_unsaved_changes = true; }
    void triggerSave();
    void runFileOpenDialog();
};