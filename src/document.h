#pragma once

#include <string>
#include <vector>
#include <map>

struct Figure
{
    size_t start_offset;
    std::string identifier;
    std::string target_path;
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
    std::vector<Figure> figures;

    // TODO: document parsing
    void parse();
    Tag extractTag(size_t& start_offset);
};
