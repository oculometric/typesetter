#include "document.h"

using namespace std;

static size_t contextAwareFind(const string& find_target, const string& content, size_t start, size_t end)
{
    // TODO: make this aware of brackets, curly braces, and quotes
    size_t result = content.find(find_target, start);
    if (result >= end)
        return string::npos;
    return result;
}

void Document::indexFigures()
{
    figures.clear();
    size_t index = 0;
    while (index < content.size())
    {
        if (content[index] == '%')
        {
            static const char* fig_search = "%fig{";
            if (content.compare(index, strlen(fig_search), fig_search) == 0)
            {
                size_t start_index = index + strlen(fig_search);
                index = content.find('}', start_index);
                size_t image_tag = contextAwareFind("image=", content, start_index, index);
                size_t id_tag = contextAwareFind("id=", content, start_index, index);
                if (image_tag == string::npos || id_tag == string::npos)
                    continue;
                size_t img_end = min(contextAwareFind(";", content, image_tag, index), index);
                string img = content.substr(image_tag + strlen("image="), img_end - (image_tag + strlen("image=")));
                size_t id_end = min(contextAwareFind(";", content, id_tag, index), index);
                string id = content.substr(id_tag + strlen("id="), id_end - (id_tag + strlen("id=")));
                figures.emplace_back(start_index, id, img);
            }
        }
        ++index;
    }
}
