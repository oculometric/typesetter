#pragma once

#include <string>
#include <vector>

struct Figure
{
    size_t start_offset;
    std::string identifier;
    std::string target_path;
};

struct Document
{
    std::string content;
    std::vector<Figure> figures;

    // TODO: document parsing
    void indexFigures();
    void parseBibliography();
};
