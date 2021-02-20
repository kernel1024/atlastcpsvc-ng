#ifndef QSL_H
#define QSL_H

#include <QString>
#include <string>
#include <algorithm>

#define QSL QStringLiteral

inline void wsLtrim(std::wstring &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t ch) {
        return !std::isspace(ch);
    }));
}

inline void wsRtrim(std::wstring &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

#endif // QSL_H
