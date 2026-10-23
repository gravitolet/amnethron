#pragma once

#include <QStringList>
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

// Rule-set names for editors, built once: the catalog holds thousands of entries.
inline const QStringList& ruleSetNames() {
    static const QStringList names = [] {
        QStringList result;
        result.reserve(static_cast<qsizetype>(ruleSetMap.size()));
        for (const auto& item : ruleSetMap) result << QString::fromStdString(item.first);
        return result;
    }();
    return names;
}

// Completion entries for simple-rule editors: the given prefixes followed by every "ruleset:<name>".
inline QStringList ruleSetCompletionItems(QStringList prefixes) {
    prefixes.reserve(prefixes.size() + ruleSetNames().size());
    for (const auto& name : ruleSetNames()) prefixes << "ruleset:" + name;
    return prefixes;
}
