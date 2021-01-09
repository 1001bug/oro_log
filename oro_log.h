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

#include <stdint.h>


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
    
    //typedef void * Poro_t ;
    //typedef uintptr_t Poro_t_unlocked;
    //typedef void * Poro_t_unlocked;
    
    
    typedef struct{
        void *p;
    } *Poro_t;
    
    typedef struct{
        void *p;
    } *Poro_t_unlocked;

    typedef struct {
        int file_name_with_date;
        int file_name_with_pid;
        int truncate;
        //int time_source; //global!!!

        char *path; //check for / on end
        char *f_name_part1;
        char *f_name_part2;
        char *f_name_part3;


        int return_q; //def?? RETURN_QUANTITY
        //int timestamp_utc; //global!!!

        size_t bufsize; //setvbuf
        int nospinlock; //1 - log file without locks (one thread print only)
        int do_mlock;  //mlock always? if 1 - exit on mlock fail
        
        int bind_cpu; //-1 mean do not bind. valid 0..1024
        
        char *internal_name;
        
        
        


    } oro_attrs_t;

    
    

    //variadic param length 7!
    //#define PARAMS_FIXED_NUM 7
    #define PARAMS_FIXED_NUM 32

    void oroLogfprintf(Poro_t logFile, FILE *File, const char* format,  ...);

    void oroLogFixed5_unlocked(Poro_t_unlocked logFile, const char* format, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5);

    void oroLogFixed(Poro_t logFile, size_t NUM, const char* format, ...);// __attribute__((nonnull (1, 3)));
    
    void oroLogRelaxed(Poro_t logFile, size_t NUM, const char* format, ...);
    void oroLogRelaxed_X(Poro_t logFile, size_t NUM, const char* format,  ...);
    
    

    size_t oroLogFull(Poro_t logFile, size_t expecting_byte, const char* format,  ...);
    
    void oroLogTruncate(Poro_t logFile);

    
    int oroWriterStop(__suseconds_t wait);
    Poro_t oroLogOpen(oro_attrs_t config);
    
    void oroInit(int time__source,int timestamp_utc,void (*error__fun)(char *fstring, ...));

    //Autocount args for N=1+  because some problem with ##__VA_ARGS__. In macro expand it everything ok, but in runtime 0 args gives count=1
#define oroLogFixedA(logFile,format,...) do{ \
extern int (*__Static_assert_function (void)) [!!sizeof (struct {\
            int __error_if_negative: (PARAMS_FIXED_NUM + 1 - COUNT_1_TO_256_ARGS(__VA_ARGS__)  );\
        }\
        )] ;\
oroLogFixed(logFile,COUNT_1_TO_256_ARGS(__VA_ARGS__),format, __VA_ARGS__); \
}while(0);
    
#define oroLogRelaxedA(logFile,format,...) do{ \
extern int (*__Static_assert_function (void)) [!!sizeof (struct {\
            int __error_if_negative: (PARAMS_FIXED_NUM + 1 - COUNT_1_TO_256_ARGS(__VA_ARGS__)  );\
        }\
        )] ;\
oroLogRelaxed(logFile,COUNT_1_TO_256_ARGS(__VA_ARGS__),format, __VA_ARGS__); \
}while(0);

#define oroLogRelaxed_XA(logFile,format,...) {\
_Static_assert(PARAMS_FIXED_NUM >= COUNT_1_TO_256_ARGS(__VA_ARGS__), "PARAMS_FIXED_NUM LIMIT in oroLogRelaxed_XA");\
oroLogRelaxed_X(logFile,COUNT_1_TO_256_ARGS(__VA_ARGS__),format, __VA_ARGS__);\
}    
    
//#define oroLogFixedA(logFile,format,...) oroLogFixed(logFile,COUNT_ARGS_( __VA_ARGS__  ,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0),format, __VA_ARGS__)
//#define COUNT_ARGS_(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z,cnt,...)    cnt
    
#define somefun(P1,P2,...) COUNT_1_TO_256_ARGS(__VA_ARGS__)
#define COUNT_1_TO_256_ARGS(...) COUNT_1_TO_256_ARGS_trick( __VA_ARGS__  ,256,255,254,253,252,251,250,249,248,247,246,245,244,243,242,241,240,239,238,237,236,235,234,233,232,231,230,229,228,227,226,225,224,223,222,221,220,219,218,217,216,215,214,213,212,211,210,209,208,207,206,205,204,203,202,201,200,199,198,197,196,195,194,193,192,191,190,189,188,187,186,185,184,183,182,181,180,179,178,177,176,175,174,173,172,171,170,169,168,167,166,165,164,163,162,161,160,159,158,157,156,155,154,153,152,151,150,149,148,147,146,145,144,143,142,141,140,139,138,137,136,135,134,133,132,131,130,129,128,127,126,125,124,123,122,121,120,119,118,117,116,115,114,113,112,111,110,109,108,107,106,105,104,103,102,101,100,99,98,97,96,95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)
#define COUNT_1_TO_256_ARGS_trick(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42,a43,a44,a45,a46,a47,a48,a49,a50,a51,a52,a53,a54,a55,a56,a57,a58,a59,a60,a61,a62,a63,a64,a65,a66,a67,a68,a69,a70,a71,a72,a73,a74,a75,a76,a77,a78,a79,a80,a81,a82,a83,a84,a85,a86,a87,a88,a89,a90,a91,a92,a93,a94,a95,a96,a97,a98,a99,a100,a101,a102,a103,a104,a105,a106,a107,a108,a109,a110,a111,a112,a113,a114,a115,a116,a117,a118,a119,a120,a121,a122,a123,a124,a125,a126,a127,a128,a129,a130,a131,a132,a133,a134,a135,a136,a137,a138,a139,a140,a141,a142,a143,a144,a145,a146,a147,a148,a149,a150,a151,a152,a153,a154,a155,a156,a157,a158,a159,a160,a161,a162,a163,a164,a165,a166,a167,a168,a169,a170,a171,a172,a173,a174,a175,a176,a177,a178,a179,a180,a181,a182,a183,a184,a185,a186,a187,a188,a189,a190,a191,a192,a193,a194,a195,a196,a197,a198,a199,a200,a201,a202,a203,a204,a205,a206,a207,a208,a209,a210,a211,a212,a213,a214,a215,a216,a217,a218,a219,a220,a221,a222,a223,a224,a225,a226,a227,a228,a229,a230,a231,a232,a233,a234,a235,a236,a237,a238,a239,a240,a241,a242,a243,a244,a245,a246,a247,a248,a249,a250,a251,a252,a253,a254,a255,a256,CNT,...)    CNT
    









#ifdef __cplusplus
}
#endif

#endif /* LOG_H */