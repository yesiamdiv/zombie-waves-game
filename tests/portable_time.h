#ifndef ZW_TESTS_PORTABLE_TIME_H
#define ZW_TESTS_PORTABLE_TIME_H

/* W3: monotonic clock + sub-millisecond sleep for the two net tests.
 *
 * clock_gettime(CLOCK_MONOTONIC, ...) and nanosleep() are POSIX and do not
 * exist in MSVC's CRT. Both tests used them directly, so neither compiled on
 * Windows.
 *
 * This shim is deliberately header-only and deliberately does NOT use SDL,
 * even though SDL_GetTicksNS/SDL_DelayNS would do the job: zombie_net_spike
 * links only ENet on purpose (CMakeLists.txt, "Independent of the game code on
 * purpose so the transport layer is validated before the real net code is
 * written"). Pulling SDL in for a clock would quietly destroy that
 * independence.
 *
 * The POSIX branch holds exactly the code the two tests had before, so on
 * Linux the generated code is the same calls with the same arguments.
 */

#if defined(_WIN32)

#include <windows.h>

static double zw_now_ms(void) {
    /* QueryPerformanceCounter is the monotonic clock on Windows: it does not
     * jump with wall-clock changes, and the frequency is fixed for the
     * lifetime of the process, so it is safe to cache. */
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static void zw_sleep_ns(long nanoseconds) {
    /* Sleep() has millisecond granularity and the callers ask for 0.5 ms and
     * 1 ms purely to stop a service loop pegging a core. Sleep(0) yields the
     * rest of the timeslice, which serves that purpose; rounding a sub-
     * millisecond request up to a full millisecond would slow the handshake
     * loops down instead. Anything >= 1 ms is passed through. */
    long ms = nanoseconds / 1000000L;
    Sleep((DWORD)(ms > 0 ? ms : 0));
}

#else

#include <time.h>

static double zw_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void zw_sleep_ns(long nanoseconds) {
    struct timespec ts = {0, nanoseconds};
    nanosleep(&ts, NULL);
}

#endif

#endif /* ZW_TESTS_PORTABLE_TIME_H */
