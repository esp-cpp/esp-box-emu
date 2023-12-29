#pragma once

#include <string>
#include <algorithm>
#include <cstring>

bool endsWith(const std::string& a, const std::string& b);
std::string ltrim(const std::string &s);
std::string rtrim(const std::string &s);
std::string trim(const std::string &s);
const char* getfield(char* line, int num);
