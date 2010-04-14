/****************************************************************
 *  aftimer object...
 ****************************************************************/

#include "aftimer.h"
#include <sys/time.h>
#include <stdio.h>

aftimer::aftimer()
{
    used       = false;
    running    = false;
    total_sec  = 0;
    total_usec = 0;
    lap_time_  = 0;
}

time_t aftimer::tstart()
{
    return t0.tv_sec;
}

void aftimer::start()
{
    used = true;
    if(!running){
	gettimeofday(&t0,NULL);
	running = true;
    }
}

void aftimer::stop(){
    if(running){
	struct timeval t;
	gettimeofday(&t,NULL);
	total_sec  += t.tv_sec - t0.tv_sec;
	total_usec += t.tv_usec - t0.tv_usec;
	lap_time_   = (double)(t.tv_sec - t0.tv_sec)  + (double)(t.tv_usec - t0.tv_usec)/1000000.0;
	running = false;
    }
}

double aftimer::lap_time()
{
    return lap_time_;
}

double aftimer::elapsed_seconds()
{
    double ret = (double)total_sec + (double)total_usec/1000000.0;
    if(running){
	struct timeval t;
	gettimeofday(&t,NULL);
	ret += t.tv_sec - t0.tv_sec;
	ret += (t.tv_usec - t0.tv_usec) / 1000000.0;
    }
    return ret;
}

char *aftimer::hms(char *buf,long t)
{
    int    h = t / 3600;
    int    m = (t / 60) % 60;
    int    s = t % 60;
    sprintf(buf,"%02d:%02d:%02d",h,m,s);
    return buf;
}

const char *aftimer::timer_text(char *buf)
{
    return hms(buf,(int)elapsed_seconds());
}

const char *aftimer::timer_text()
{
    return timer_text(buf);
}

double aftimer::eta(double fraction_done)
{
    double t = elapsed_seconds();
    if(t==0) return -1;			// can't figure it out
    if(fraction_done==0) return -1;	// can't figure it out
    return (t * 1.0/fraction_done - t);
}

const char *aftimer::eta_text(char *buf,double fraction_done)
{
    double e = eta(fraction_done);
    if(e<0) return "n/a";		// can't figure it out
    return hms(buf,(long)e);
}

const char *aftimer::eta_text(double fraction_done)
{
    return eta_text(buf,fraction_done);
}
