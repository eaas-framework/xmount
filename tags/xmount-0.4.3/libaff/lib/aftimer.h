#ifndef __TIMER_H__
#define __TIMER_H__

#include <sys/time.h>

#ifdef __cplusplus

class aftimer {
    bool used;				// was this timer used?
    struct timeval t0;
    bool running;
    long total_sec;
    long total_usec;
    double lap_time_;			// time from when we last did a "stop"
    char buf[64];			// internal time buffer
public:
    aftimer();
    time_t tstart();
    void start();
    void stop();
    double elapsed_seconds();		//
    double lap_time();
    static char *hms(char *b,long t);   // turn a number of seconds into hms
    const char *timer_text(char *buf);	// return the time spent reading, as text
    const char *timer_text();			// uses internal buffer
    double eta(double fraction_done);	// calculate ETA in seconds, given fraction
    const char *eta_text(char *buf,double fraction_done);
    const char *eta_text(double fraction_done);
};
#endif

#endif
