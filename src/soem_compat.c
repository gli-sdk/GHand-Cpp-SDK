#ifdef __MINGW32__
#include <time.h>
#include <windows.h>

// MinGW 13.x 兼容性: 提供 timespec64_get 的 stub 实现
#ifdef __cplusplus
extern "C" {
#endif

// Windows 需要下划线前缀
int _timespec64_get(struct timespec *ts, int base) {
__attribute__((alias("_timespec64_get"))) int timespec64_get(struct timespec *ts, int base);
#endif

// 不带下划线的版本（别名）
#ifdef _MSC_VER
// MSVC 不支持 __attribute__, 使用 pragma
#pragma comment(linker, "/alternatename:_timespec64_get=timespec64_get")
int timespec64_get(struct timespec *ts, int base) {
    return _timespec64_get(ts, base);
}
#else
// GCC/MinGW 使用 alias
int timespec64_get(struct timespec *ts, int base);
    // 使用 Windows API 获取高精度时间
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }

    QueryPerformanceCounter(&counter);

    ts->tv_sec = counter.QuadPart / frequency.QuadPart;
    ts->tv_nsec = (long)((counter.QuadPart % frequency.QuadPart) * 1000000000LL / frequency.QuadPart);

    return base;
}

#ifdef __cplusplus
}
#endif

#endif
