/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   func.h
 * Author: ermolovag
 *
 * Created on 20 мая 2019 г., 16:20
 */

#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__CYGWIN__)
    //on windows cygwin has a bug with CLOCK_MONOTONIC_COARSE
    //and CLOCK_REALTIME=CLOCK_REALTIME_COARSE
#define WINDOWS
#define CLOCK_ID_WALL           CLOCK_REALTIME
#define CLOCK_ID_WALL_FAST      CLOCK_REALTIME
#define CLOCK_ID_MEASURE        CLOCK_MONOTONIC
#define CLOCK_ID_MEASURE_FAST   CLOCK_MONOTONIC


#else

#define LINUX
#define CLOCK_ID_WALL           CLOCK_REALTIME
#define CLOCK_ID_WALL_FAST      CLOCK_REALTIME_COARSE
#define CLOCK_ID_MEASURE        CLOCK_MONOTONIC
#define CLOCK_ID_MEASURE_FAST   CLOCK_MONOTONIC_COARSE


#endif    
    
    typedef void * Poro_t;

    typedef struct {
        int file_name_with_date;
        int file_name_with_pid;
        int truncate;
        int time_source; //global!!!

        char *path; //check for / on end
        char *f_name_part1;
        char *f_name_part2;
        char *f_name_part3;


        int return_q; //def?? RETURN_QUANTITY
        int timestamp_utc;

        size_t bufsize; //setvbuf
        int nospinlock; //1 - log file without locks (one thread print only)
        int do_mlock;  //mlock always? if 1 - exit on mlock fail


    } oro_attrs_t;

    typedef struct {
        char *buf;
        size_t n;
    } oroObj_t;



    void oroLogFixed5_unlocked(Poro_t logFile, const char* format, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5);

    void oroLogFixed(Poro_t logFile, size_t NUM, const char* format, ...);
    void oroLogRelaxed(Poro_t logFile, const char* format, ...);
    void oroLogRelaxed_wojt(Poro_t logFile, const char* format, ...);
    void oroLogRelaxedDummy(Poro_t logFile, const char* format, ...);
    void oroLogRelaxedDummy_wojt(Poro_t logFile, const char* format, ...);
    void oroLogRelaxed_Q(Poro_t logFile, const char* format, ...);


    oroObj_t * oroLogFullObj(size_t bytes);
    void oroLogFullObjFree(oroObj_t **Obj);
    void oroLogFull(Poro_t logFile, oroObj_t *o, const char* format, ...);
    size_t oroLogFulla(Poro_t logFile, size_t expecting_byte, const char* format,  ...);
    
    void oroLogTruncate(); //все разом?

    int oroWriterStart(int bind_cpu, void (*error_fun)(char *fstring, ...));
    int oroWriterStop(__suseconds_t wait, void (*error_fun)(char *fstring, ...));
    Poro_t oroLogOpen(oro_attrs_t config, void (*error_fun)(char *fstring, ...));

    //Autocount args for N=1+  because some problem with ##__VA_ARGS__. In macro expand it everything ok, but in runtime 0 args gives count=1
#define oroLogFixedA(logFile,format,...) oroLogFixed(logFile,COUNT_ARGS_( __VA_ARGS__  ,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0),format, __VA_ARGS__)
#define COUNT_ARGS_(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,cnt,...)    cnt






#ifdef __cplusplus
}
#endif

#endif /* LOG_H */

