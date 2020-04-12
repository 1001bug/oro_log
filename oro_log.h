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
        
        
    } log_attrs;
    
    typedef struct {
        char *buf;
        size_t n;
    }logObj;
   

    void logLog(int logFile, const char* format, ...);
    void logLogFixed(int logFile, size_t NUM, const char* format,  ...);
    void logLogRelaxed(int logFile, const char* format,  ...);
    void logLogRelaxed_wojt(int logFile, const char* format,  ...);
    void logLogRelaxedDummy(int logFile, const char* format,  ...);
    void logLogRelaxedDummy_wojt(int logFile, const char* format,  ...);
    void logLogRelaxed_Q(int logFile, const char* format,  ...);
    void logLogRelaxed_QAB(int logFile, const char* format,  ...);
    
    logObj * logLogFullObj(size_t bytes);
    void logLogFullObjFree(logObj **Obj);
    void logLogFull(int logFile, logObj *o, const char* format,  ...);
    void logSetRequestForLogsTruncate();
    //int logFlush(int cmd,__suseconds_t wait, char *cpu_list, void (*error_fun)(char *fstring,...));
    int logFlushStart(int bind_cpu, void (*error_fun)(char *fstring, ...));
    int logFlushStop(__suseconds_t wait, void (*error_fun)(char *fstring,...));
    int logOpen(int *logFile,log_attrs config, void (*error_fun)(char *fstring,...));
    
//Autocount args for N=1+  because some problem with ##__VA_ARGS__. In macro expand it everything ok, but in runtime 0 args gives count=1
#define logLogFixedA(logFile,format,...) logLogFixed(logFile,COUNT_ARGS_( __VA_ARGS__  ,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0),format, __VA_ARGS__)
#define COUNT_ARGS_(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,cnt,...)    cnt
    





#ifdef __cplusplus
}
#endif

#endif /* LOG_H */

