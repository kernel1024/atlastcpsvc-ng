#ifndef QSL_H
#define QSL_H

#include <QString>
#include <string>
#include <algorithm>

#define QSL QStringLiteral

inline void wsZeroRightTrim(std::wstring &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) {
        return (ch != '\0');
    }).base(), s.end());
}

#endif // QSL_H
