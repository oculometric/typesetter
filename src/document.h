#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

struct Figure
{
    size_t start_offset;
    std::string identifier;
    std::string target_path;
};

struct Section
{
    size_t start_offset;
    std::string identifier;
};

struct Tag
{
    size_t start_offset;
    size_t size;
    std::string type;
    std::map<std::string, std::string> params;
};

struct Document
{
    std::string content;
    std::set<std::string> tag_ids; 
    std::vector<Figure> figures;
    std::vector<Section> sections;
    size_t parsing_error_position = -1;
    std::string parsing_error_desc;

    bool parse();
    std::string getUniqueID(const std::string& name) const;
    Tag extractTag(size_t& start_offset);
};
