#pragma once

#include <string>

#include "core.h"

namespace STRN
{

class Widget;

class Window : public Drawable
{
private:
    Widget* root = nullptr;
    
    std::string title = "window";
    bool borderless = false;
    bool is_minimised = false;
    bool allows_minimise = true;
    
    bool is_dirty = true;
    
public:
    Window(const std::string& _title, const bool _borderless, const bool _minimised) : title(_title), borderless(_borderless), is_minimised(_minimised) { }
    Window(const Window& other) = delete;
    Window(Window&& other) = delete;
    void operator=(const Window& other) = delete;
    void operator=(Window&& other) = delete;
    ~Window() override = default;
    
    std::string getTitle() const { return title; }
    void setTitle(const std::string& value);
    void setAllowsMinimise(bool value);
    void setBorderless(bool value);
    bool getMinimised() const { return is_minimised; }
    void setRoot(Widget* value);

    void render(Context& ctx) override;
    void dirty() { is_dirty = true; }
    bool getDirty() const { return is_dirty; }
};

}
