#pragma once

#include <map>
#include <srslist.h>
#include <string>

inline const std::map<std::string, std::string> ruleSetMap = [] {
    std::map<std::string, std::string> result;
    for (const auto& item : ruleSetList) {
        result.emplace(std::string(item.first), std::string(item.second));
    }
    return result;
}();
