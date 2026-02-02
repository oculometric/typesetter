#pragma once

#include <vector>
#include <string>
#include <typeinfo>
#include <stdexcept>

#include "util.h"

namespace STRN
{

class Window;
class Context;

class Widget
{
protected:
    std::vector<Widget*> children;
    
private:
    Transform2 transform; // stores position within parent, and size
    bool visible = false;
    Widget* parent = nullptr;
    Window* window = nullptr;
    
public:
    Widget() = default;
    Widget(const Widget& other) = delete;
    Widget(Widget&& other) = delete;
    void operator=(const Widget& other) = delete;
    void operator=(Widget&& other) = delete;
    virtual ~Widget() = default;
    
    virtual void arrange(const Vec2 available_area);
    virtual void render(Context& ctx) const { }
    
    virtual Vec2 getMinSize() const { return Vec2{ 1, 1 }; }
    virtual Vec2 getMaxSize() const { return Vec2{ -1, -1 }; }
    
    bool getVisible() const { return visible; }
    void setVisible(const bool value);

    Transform2 getTransform() const { return transform; }
    void setTransform(const Transform2& value);
    void setPosition(const Vec2& value);
    void setSize(const Vec2& value);

    int getChildren() const { return static_cast<int>(children.size()); }
    int addChild(Widget* child);
    void setWindow(Window* _window);

protected:
    void dirty() const;
};

class Label : public Widget
{
private:
    std::string text;
    
public:
    explicit Label(const std::string& _text) : text(_text) { }
    
    void render(Context& ctx) const override;

    Vec2 getMinSize() const override { return Vec2{ 0, 1 }; }
    Vec2 getMaxSize() const override { return Vec2{ -1, 1 }; }
    
    std::string getText() const { return text; }
    void setText(const std::string& value);
};

class ArrangedBox : public Widget
{
public:
    void render(Context& ctx) const override;
};

class VerticalBox : public ArrangedBox
{
public:
    explicit VerticalBox(const std::vector<Widget*>& _children) { children = _children; }
    
    void arrange(Vec2 available_area) override;
};

class HorizontalBox : public ArrangedBox
{
public:
    explicit HorizontalBox(const std::vector<Widget*>& _children) { children = _children; }
    
    void arrange(Vec2 available_area) override;
};

class ArtBlock : public Widget
{
private:
    std::string ascii;
    int pitch;
    
public:
    ArtBlock(const std::string& _ascii, const int _pitch) : ascii(_ascii), pitch(_pitch) { }
    
    void render(Context& ctx) const override;
    
    Vec2 getMinSize() const override { return Vec2{ pitch, static_cast<int>(ascii.size()) / pitch }; }
    Vec2 getMaxSize() const override { return getMinSize(); }
    
    void setData(const std::string& data, const int _pitch);
};

class SizeLimiter : public Widget
{
private:
    Vec2 max_size = Vec2{ -1, -1 };
    Vec2 min_size = Vec2{ 1, 1 };
    
public:
    SizeLimiter(const Vec2& _max_size, const Vec2& _min_size, Widget* _child) : max_size(_max_size), min_size(_min_size) { if (_child) children.push_back(_child); }
    
    void arrange(Vec2 available_area) override;
    void render(Context& ctx) const override;
    
    Vec2 getMinSize() const override { return min_size; }
    Vec2 getMaxSize() const override { return max_size; }
    
    void setMaxSize(const Vec2& value);
    void setMinSize(const Vec2& value);
};

// TODO: size limiter
// TODO: border
// TODO: spacers
// TODO: dividers
// TODO: button
// TODO: list view
// TODO: tab view
// TODO: bitmap image
// TODO: toggle button
// TODO: radio button
// TODO: grid boxes

class Builder
{
private:
    Widget* root;
    std::vector<std::pair<Widget*, int>> current_parent_stack;
    
public:
    Builder() { reset(); }
    
    Label* label(const std::string& text);
    ArtBlock* artBlock(const std::string& ascii, const int pitch);
    HorizontalBox* beginHorizontalBox();
    void endHorizontalBox();
    VerticalBox* beginVerticalBox();
    void endVerticalBox();
    SizeLimiter* sizeLimiter(const Vec2& max_size, const Vec2& min_size);

    Widget* end();
    void reset();

private:
    template<class T> T* insertWidget(T* widget)
    {
        auto& parent = current_parent_stack[current_parent_stack.size() - 1];
        const int children = parent.first->addChild(widget);
        if (parent.second != -1 && children >= parent.second)
            endWidget<Widget>();
        return widget;
    }
    
    template<class T> T* beginWidget(T* widget, int max_children)
    {
        auto& parent = current_parent_stack[current_parent_stack.size() - 1];
        parent.first->addChild(widget);
        current_parent_stack.push_back({ widget, max_children });
        return widget;
    }
    
    template<class T> void endWidget()
    {
        if (dynamic_cast<T*>(current_parent_stack[current_parent_stack.size() - 1].first) == nullptr)
            throw std::runtime_error((std::string("invalid endWidget called for ") + typeid(T).name()));
        current_parent_stack.pop_back();
        auto parent = current_parent_stack[current_parent_stack.size() - 1];
        while (parent.second != -1 && parent.first->getChildren() >= parent.second)
        {
            current_parent_stack.pop_back();
            parent = current_parent_stack[current_parent_stack.size() - 1];
        }
    }
};

}
