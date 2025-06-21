#pragma once
#include <ctime>

inline std::time_t timegm_utc(std::tm* tm) {
#ifdef _WIN32
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}
