#pragma once

#include <vector>
#include <string>

#include "core.h"

namespace STRN
{

class Node : public Drawable
{
public:
    enum NodeElementType : uint8_t
    {
        ELEMENT_INPUT,
        ELEMENT_OUTPUT,
        ELEMENT_TEXT,
        ELEMENT_SPACE,
        ELEMENT_BLOCK
    };
	
    struct NodeElement
    {
        std::string text;
        NodeElementType type;
        int pin_type = 0;
        bool pin_solid = true;
    };
    
public:
    std::string title = "node";
    std::vector<NodeElement> elements;
    int palette_index = 1;

public:
    Node();
    Node(const std::string& _title, const std::vector<NodeElement>& _elements) : title(_title), elements(_elements) { resetSize(); }
    ~Node() override = default;
    
    void render(Context& ctx) override;
    void resetSize();
};

}