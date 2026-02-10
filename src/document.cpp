#include "document.h"

#include <cstring>

using namespace std;

bool Document::parse()
{
    parsing_error_position = -1;
    parsing_error_desc = "";
    
    vector<Tag> tags;
    size_t offset = 0;
    // step through the document
    while (offset < content.size())
    {
        if (content[offset] == '%')
        {
            // identify % tags
            Tag t = extractTag(offset);
            if (t.start_offset == (size_t)-1)
                return false;
            tags.push_back(t);
            ++offset;
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
    sections.clear();
    figures.clear();
    for (auto& tag : tags)
    {
        if (tag.type == "figref")
        {
            string id;
            if (tag.params.contains("id"))
                id = tag.params["id"];
            else
            {
                parsing_error_position = tag.start_offset;
                parsing_error_desc = "'figref' tag missing 'id' param";
                return false;
            }
        }
        else if (tag.type == "sectref")
        {
            string id;
            if (tag.params.contains("id"))
                id = tag.params["id"];
            else
            {
                parsing_error_position = tag.start_offset;
                parsing_error_desc = "'sectref' tag missing 'id' param";
                return false;
            }
        }
        else if (tag.type == "fig")
        {
            string id;
            if (tag.params.contains("id"))
                id = tag.params["id"];
            else
            {
                parsing_error_position = tag.start_offset;
                parsing_error_desc = "'fig' tag missing 'id' param";
                return false;
            }
            tag_ids.insert(id);
            
            string image;
            if (tag.params.contains("image"))
                image = tag.params["image"];
            else
            {
                parsing_error_position = tag.start_offset;
                parsing_error_desc = "'fig' tag missing 'image' param";
                return false;
            }
            
            figures.emplace_back(tag.start_offset, id, image);
        }
        else if (tag.type == "title")
        {
        }
        else if (tag.type == "config")
        {
        }
        else if (tag.type == "bib")
        {
        }
        else if (tag.type == "section")
        {
            string id;
            if (tag.params.contains("id"))
                id = tag.params["id"];
            else
            {
                parsing_error_position = tag.start_offset;
                parsing_error_desc = "'sect' tag missing 'id' param";
                return false;
            }
            tag_ids.insert(id);
            
            sections.emplace_back(tag.start_offset, id);
        }
        else if (tag.type == "cite")
        {
            
        }
        else
        {
            parsing_error_position = tag.start_offset;
            parsing_error_desc = "unrecognised tag type";
            return false;
        }
    }
    
    return parsing_error_position == (size_t)-1;
}

string Document::getUniqueID(const string& name) const
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
            parsing_error_position = start_offset;
            parsing_error_desc = "incomplete tag";
            return { (size_t)-1 };
        }
        const char c = content[current];
        if (state == 0)
        {
            if (c == '{')
            {
                open_curly = current;
                state = 1;
            }
            else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            {
                parsing_error_position = current;
                parsing_error_desc = "invalid character in tag type";
                return { (size_t)-1 };
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
                else if (c == ';')
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
                        parsing_error_position = current;
                        parsing_error_desc = "only one '=' token permitted per statement";
                        return { (size_t)-1 };
                    }
                }
                else if (c == '}')
                {
                    close_curly = current;
                    break;
                }
                else if (!(c == '_' || c == '-' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                {
                    parsing_error_position = current;
                    parsing_error_desc = "invalid character inside tag";
                    return { (size_t)-1 };
                }
            }
        }
        ++current;
    }
    semicolon_tracker.emplace_back(current, current_equals);
    
    // store offsets and info for each tag
    Tag result;
    result.type = content.substr(start_offset + 1, open_curly - start_offset - 1);
    if (result.type.empty())
    {
        parsing_error_position = start_offset;
        parsing_error_desc = "missing tag type";
        return { (size_t)-1 };
    }
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
