#include "document.h"

#include <ObjectArray.h>

using namespace std;

static size_t contextAwareFind(const string& find_target, const string& content, size_t start, size_t end)
{
    // TODO: make this aware of brackets, curly braces, and quotes
    size_t result = content.find(find_target, start);
    if (result >= end)
        return string::npos;
    return result;
}

void Document::parse()
{
    vector<Tag> tags;
    size_t offset = 0;
    // step through the document
    while (offset < content.size())
    {
        if (content[offset] == '%')
        {
            // identify % tags
            tags.push_back(extractTag(offset));
        }
        // identify other formations (bold, italic, header)
        else if (content[offset] == '*')
        {
        }
        else if (content[offset] == '_')
        {
        }
        else if (content[offset] == '#')
        {
            // tag headers according to their section number (and subsection, and subsubsection etc)
        }
        ++offset;
    }
    
    // store all of this in a list of elements (text, bold, italic, header, tags)
    tag_ids.clear();
    figures.clear();
    for (auto& tag : tags)
    {
        if (tag.params.contains("id"))
            tag_ids.insert(tag.params["id"]);
        if (tag.type == "fig")
            figures.emplace_back(tag.start_offset, tag.params["id"], tag.params["image"]);
        // TODO: check for invalid tags
        // TODO: check each tag has all required params
    }
}

string Document::getUniqueID(string name)
{
    // generate a random number from the name
    size_t seed = 0;
    for (char c : name)
        seed += c;
    // seed RNG using the number
    srand(seed);
    
    static const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    // generate 8 letter (Aa) name
    string id;
    do
    {
        id.clear();
        for (size_t i = 0; i < 8; ++i)
            id.push_back(letters[rand() % strlen(letters)]);
        // check if that name exists (if so, increment seed and repeat)
    } while (tag_ids.contains(id));
    
    return id;
}

Tag Document::extractTag(size_t& start_offset)
{
    // find the end of the tag, and the closing brace (context-aware)
    size_t current = start_offset + 1;
    int state = 0; // 0 = looking for open curly, 1 = looking for close curly
    size_t open_curly = 0;
    size_t close_curly = 0;
    size_t current_equals = -1;
    vector<pair<size_t, size_t>> semicolon_tracker;
    bool inside_quotes = false;
    while (true)
    {
        if (current >= content.size())
        {
            // TODO: throw error
            ++start_offset;
            return { };
        }
        char c = content[current];
        if (state == 0)
        {
            // TODO: if c is not a letter char, throw error
            if (c == '{')
            {
                open_curly = current;
                state = 1;
            }
        }
        else if (state == 1)
        {
            if (inside_quotes)
            {
                if (c == '\"')
                    inside_quotes = false;
            }
            else
            {
                if (c == '\"')
                    inside_quotes = true;
                // TODO: if c is not a letter/number char, or " ; = _ -, throw error
                if (c == ';')
                {
                    semicolon_tracker.emplace_back(current, current_equals);
                    current_equals = -1;
                }
                else if (c == '=')
                {
                    if (current_equals == static_cast<size_t>(-1))
                        current_equals = current;
                    else
                    {
                        // TODO: throw error
                    }
                }
                else if (c == '}')
                {
                    close_curly = current;
                    break;
                }
            }
        }
        ++current;
    }
    semicolon_tracker.emplace_back(current, current_equals);
    
    // store offsets and info for each tag
    Tag result;
    // TODO: check for zero size tag type and throw error
    result.type = content.substr(start_offset + 1, open_curly - start_offset - 1);
    result.start_offset = start_offset;
    result.size = close_curly - start_offset;

    // parse the contents of the brackets for THING=value; (context-aware)
    size_t last_end = open_curly;
    for (const auto& [semicolon_offset, equals_offset] : semicolon_tracker)
    {
        string key;
        string value;
        if (equals_offset == static_cast<size_t>(-1))
            key = content.substr(last_end + 1, semicolon_offset - last_end - 1);
        else
        {
            key = content.substr(last_end + 1, equals_offset - last_end - 1);
            value = content.substr(equals_offset + 1, semicolon_offset - equals_offset - 1);
        }
        last_end = semicolon_offset;
        if (!key.empty())
            result.params.emplace(key, value);
    }
    
    start_offset = close_curly;
    
    return result;
}
