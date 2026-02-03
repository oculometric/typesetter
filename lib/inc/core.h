#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <set>
#include <queue>

#include "util.h"

struct GLFWwindow;

namespace STRN
{

enum Colour : uint8_t
{
    FG_BLACK        = 0b0000,
    FG_DARK_RED     = 0b0001,
    FG_DARK_GREEN   = 0b0010,
    FG_DARK_YELLOW  = 0b0011,
    FG_DARK_BLUE    = 0b0100,
    FG_DARK_MAGENTA = 0b0101,
    FG_DARK_CYAN    = 0b0110,
    FG_LIGHT_GREY   = 0b0111,
    FG_DARK_GREY    = 0b1000,
    FG_RED          = 0b1001,
    FG_GREEN        = 0b1010,
    FG_YELLOW       = 0b1011,
    FG_BLUE         = 0b1100,
    FG_MAGENTA      = 0b1101,
    FG_CYAN         = 0b1110,
    FG_WHITE        = 0b1111,
    
    BG_BLACK        = FG_BLACK << 4,
    BG_DARK_RED     = FG_DARK_RED << 4,
    BG_DARK_GREEN   = FG_DARK_GREEN << 4,
    BG_DARK_YELLOW  = FG_DARK_YELLOW << 4,
    BG_DARK_BLUE    = FG_DARK_BLUE << 4,
    BG_DARK_MAGENTA = FG_DARK_MAGENTA << 4,
    BG_DARK_CYAN    = FG_DARK_CYAN << 4,
    BG_LIGHT_GREY   = FG_LIGHT_GREY << 4,
    BG_DARK_GREY    = FG_DARK_GREY << 4,
    BG_RED          = FG_RED << 4,
    BG_GREEN        = FG_GREEN << 4,
    BG_YELLOW       = FG_YELLOW << 4,
    BG_BLUE         = FG_BLUE << 4,
    BG_MAGENTA      = FG_MAGENTA << 4,
    BG_CYAN         = FG_CYAN << 4,
    BG_WHITE        = FG_WHITE << 4
};

inline Colour operator|(const Colour a, const Colour b)
{ return static_cast<Colour>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }

inline Colour foreground(const Colour c)
{ return static_cast<Colour>(c & 0b00001111); }

inline Colour background(const Colour c)
{ return static_cast<Colour>(c & 0b11110000); }

inline Colour invert(const Colour c)
{ return static_cast<Colour>((foreground(c) << 4) | (background(c) >> 4)); }

#define DEFAULT_COLOUR (BG_BLACK | FG_WHITE)
#define DEFAULT_INVERTED (FG_BLACK | BG_WHITE)

struct Char
{
public:
    unsigned char value = ' ';
    Colour colour_bits = DEFAULT_COLOUR;
    
public:
    Char() = default;
    Char(const unsigned char c) : value(c) { }
    Char(const int c) { value = static_cast<char>(c); }
    Char(const unsigned char chr, const Colour col) : value(chr), colour_bits(col) { }
};

struct Palette
{
public:
    Colour standard_colour = DEFAULT_COLOUR;
    Colour inverted_colour = DEFAULT_INVERTED;
    Colour tertiary_colour = (BG_BLACK | FG_RED);
    
public:
    Colour operator[](int i) const
    {
        switch (i)
        {
        case 0: return standard_colour;
        case 1: return inverted_colour;
        case 2: return tertiary_colour;
        default: return standard_colour;
        }
    }
};

class Context
{
private:
    std::vector<Char> backing;
    int pitch = 0;
    std::vector<Box2> bounds_stack;
    Box2 permitted_bounds;
    Palette base_palette;
    std::vector<Palette> palette_stack;
    
public:
    Context(Vec2 size, Char fill_value);
    Context() : Context({1, 1}, ' ') { }
    Context(const Context& other) = delete;
    Context(Context&& other) = delete;
    void operator=(const Context& other) = delete;
    void operator=(Context&& other) = delete;
    ~Context() = default;
    
    Vec2 getSize() const { return permitted_bounds.size(); }
    void draw(Vec2 position, Char value);
    void draw(Vec2 position, unsigned char value, int palette_colour = 0);
    void drawBox(Vec2 start, Vec2 size, int palette_colour = 0);
    void fill(Vec2 start, Vec2 size, Char value);
    void fill(Vec2 start, Vec2 size, unsigned char value, int palette_colour = 0);
    void drawText(Vec2 start, const std::string& text, Colour colour, size_t text_offset = 0, size_t max_length = -1);
    void drawText(Vec2 start, const std::string& text, int palette_colour = 0, size_t text_offset = 0, size_t max_length = -1);
    
    std::vector<Char>::const_iterator begin() const;
    std::vector<Char>::const_iterator end() const;
    
    void pushBounds(const Vec2& min, const Vec2& max);
    void popBounds();
    void pushPalette(const Palette& p);
    void popPalette();
    Palette getBasePalette() const { return base_palette; }
    void setBasePalette(const Palette& p);
    Colour getPaletteColour(int c) const;
    
    void clear(Char fill_value);
    void resize(Vec2 new_size, Char fill_value);
};

class Drawable
{
protected:
    Transform2 transform;
    
public:
    Drawable() = default;
    virtual ~Drawable() = default;
    
    Transform2 getTransform() const { return transform; }
    Vec2 getSize() const { return transform.size; }
    void setSize(Vec2 value);
    Vec2 getPosition() const { return transform.position; }
    void setPosition(Vec2 value);
    
    virtual void render(Context& ctx) { }
};

struct KeyEvent
{
    enum Modifier
    {
        NONE = 0,
        SHIFT = 1,
        CTRL = 2,
        ALT = 4,
        SUPER = 8,
        CAPS = 16,
        NUM = 32
    };

    int key = 0;
    bool pressed = false;
    Modifier modifiers = NONE;
};

class Rasteriser
{
private:
    Context context;
    Vec2 size;
    std::set<Drawable*> drawables;
    Char clear_value = { ' ' };
    
public:
    Rasteriser(const Rasteriser& other) = delete;
    void operator=(const Rasteriser& other) = delete;
    virtual ~Rasteriser() = default;
    
    void render();
    virtual bool update() { return false; }
    virtual KeyEvent getKeyEvent() { return { }; }
    virtual void present() { }
    Vec2 getSize() const { return size; }
    void setSize(Vec2 new_size);
    bool insertDrawable(Drawable* d);
    bool eraseDrawable(Drawable* d);
    void setClearValue(Char value);
    Palette getPalette() const { return context.getBasePalette(); }
    void setPalette(const Palette& p);
    
protected:
    Rasteriser() { }
    
    const Context& getContext() const { return context; }
    void clearContext();
};

class TerminalRasteriser : public Rasteriser
{
public:
    TerminalRasteriser();
    TerminalRasteriser(const TerminalRasteriser& other) = delete;
    void operator=(const TerminalRasteriser& other) = delete;
    ~TerminalRasteriser() override = default;
    
    static Vec2 getScreenSize();
    static void setCursorVisible(bool visible);
    static void setCursorPosition(Vec2 position);
    
    bool update() override;
    // TODO: handle input
    void present() override;
};

class NativeRasteriser : public Rasteriser
{
private:
    struct Vertex
    {
        float vertex_position[3];
        float vertex_colour_a[3];
        float vertex_colour_b[3];
        float vertex_uv[2];
    };

private:
    GLFWwindow* window = nullptr;
    int width = 0;
    int height = 0;
    size_t scale_factor = 2;
    size_t pitch = 1;

    unsigned int vertex_buffer;
    unsigned int index_buffer;
    unsigned int vertex_array_object;
    unsigned int shader_program;
    int transform_var;
    unsigned int font_texture;
    int mesh_index_count;

    std::queue<KeyEvent> pending_keys;
    std::queue<unsigned int> pending_chars;

public:
    NativeRasteriser();
    NativeRasteriser(const NativeRasteriser& other) = delete;
    void operator=(const NativeRasteriser& other) = delete;
    ~NativeRasteriser() override = default;
    
    void setWindowSize(int w, int h) const;
    void setScaleFactor(size_t value);
    
    bool update() override;
    KeyEvent getKeyEvent() override;
    unsigned int getCharEvent();
    void present() override;
    
private:
    Vec2 calculateSize() const;
    static void keyFunction(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charFunction(GLFWwindow* window, unsigned int chr);
};

}