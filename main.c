/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: ErmolovAG
 *
 * Created on 2 марта 2020 г., 11:21
 */
#define _GNU_SOURCE




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include<stdarg.h>
#include<time.h>
#include<unistd.h>
#include <pthread.h>
#include <stdint.h>
#include "oro_log.h"
//#include "cfuncs/func.h"


//#include "cfuncs/cpucnt.h"

#include <gnu/libc-version.h>
#include <limits.h>






#define LOG_HEADER ""
//#define LOG_HEADER "%02i:%02i:%02i.%09li|%li%09li;"
#define NN 10000
/*
 * 
 */
#define MFENCE    __asm__ __volatile__ ("mfence" ::: "memory")
#define LFENCE    __asm__ __volatile__ ("lfence" ::: "memory")
#define SFENCE    __asm__ __volatile__ ("sfence" ::: "memory")

#define _RDTSCP(V) __asm__ __volatile__("rdtscp;shl $32, %%rdx;or %%rdx, %%rax":"=a"(V):: "%rcx", "%rdx")
#define RDTSCP(V) MFENCE;\
_RDTSCP(V);\
MFENCE


#define CYCLES 1
#define CLOCK  2
#define TIME_SPEC_DIFF(S,E) (((E).tv_sec * 1000000000L + (E).tv_nsec)-((S).tv_sec * 1000000000L + (S).tv_nsec))
#define TIME_SPEC_NANO(S) ((S).tv_sec * 1000000000L + (S).tv_nsec)

extern const char *__progname; //иммя программы без прибамбасов
void PRINT_STAT(const char *name, uint64_t *time_S, uint64_t *time_E, uint64_t *time_Dif, int cnt, int ts_source);

void on_error_stderr(char *fstring, ...) {
    va_list arglist;
    va_start(arglist, fstring);
    vfprintf(stderr, fstring, arglist);
    fprintf(stderr, "\n");
    va_end(arglist);
    return;
}

int compare_lu(const void* a, const void* b) {
    unsigned long _a = *((unsigned long*) a);
    unsigned long _b = *((unsigned long*) b);

    if (_a == _b) return 0;
    else if (_a < _b) return -1;
    else return 1;
}

int setCPUaffinityCur1(pthread_t af_pth_r, int cpu_n) {

#ifndef __CYGWIN__
    cpu_set_t cpus_af;
    CPU_ZERO(&cpus_af);
    CPU_SET(cpu_n, &cpus_af);
    if (pthread_setaffinity_np(af_pth_r, sizeof (cpu_set_t), &cpus_af) != 0)
        return -1;

    return 0;
#endif
    return -1;
}

void* threaded_log_actions(void *p) {
    Poro_t L = (Poro_t) p;
    int KK = 1000;
    struct timespec cur = {0};

    for (int m = 0; m < KK; m++) {
        for (int r = 0; r < KK; r++) {

            oroLogFixed(L, 7, "1ogLogFixed   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                    , __ASSERT_FUNCTION
                    , KK
                    , cur.tv_sec
                    , cur.tv_nsec
                    , 55 << 10
                    , 31337, 44889955
                    );


        }
        select(0, NULL, NULL, NULL, &((struct timeval){0, 10000}));
    }

    return NULL;
}



uint64_t cycles_in_one_usec = 0;

int main(int argc, char** argv) {


    if (setCPUaffinityCur1(pthread_self(), 3) != 0) {
        fprintf(stderr, "setCPUaffinityCur1 failed\n");
    }

    printf("__func__           : %s\n", __func__);
    printf("__FUNCTION__       : %s\n", __FUNCTION__);
    printf("__PRETTY_FUNCTION__: %s\n", __PRETTY_FUNCTION__);

    {
        uint64_t c1, c2;
        RDTSCP(c1);
        usleep(1000);
        RDTSCP(c2);
        cycles_in_one_usec = (c2 - c1) / 1000;
    }


    //struct timespec *measure_S, *measure_E;

    /*measure_S = calloc(NN,sizeof(struct timespec));
    measure_E = calloc(NN,sizeof(struct timespec));*/

    uint64_t start;
    uint64_t *rdtscp_S;
    uint64_t *rdtscp_E;
    uint64_t *rdtscp_dif;

    FILE *OUT;

    OUT = fopen("direct_out.log", "w");


    assert(OUT != NULL);

    rdtscp_S = calloc(NN, sizeof (uint64_t));
    rdtscp_E = calloc(NN, sizeof (uint64_t));
    rdtscp_dif = calloc(NN, sizeof (uint64_t));


    char *static_string_ptr = ">static_string_ptr<";
    Poro_t LOG = 0;

    
    oro_attrs_t config = {
    .f_name_part1 = "out",
    .f_name_strftime = "",

    .path = "",
    .bufsize = 16384,
    .return_q=NN,

    .truncate = 1,

    .file_name_with_pid = 0,
    
    .do_mlock = 0,
    .bind_cpu = 2,

    
    
    //config.path="some";

    //config.time_source=CLOCK_ID_WALL;
    //config.time_source=-1;
    //config.timestamp_utc = 1;

    };

    
    
    oro_attrs_t config2 = config_from_str("pid=1;truncate=1;do_mlock=1;return_q=123;bufsize=321;bind_cpu=5;path=some/path/");
    
    oroInit(-1, 0, -1, on_error_stderr);
    LOG = oroLogOpen(config);


    //int r = logOpen(&LOG, 0, "", "main", "A", on_error_stderr);
    //int some = 33;

    assert(LOG != 0);

    //config.f_name_part1="out_2";
    //config.return_q = 2000000;
    //config.bufsize=40960;

    Poro_t_unlocked LOG_u = 0;
    //LOG_u = (Poro_t_unlocked)LOG;
    LOG_u = LOG;


    //pthread_attr_destroy(&morelog_pth_attrs_r);






    //LOG2 = oroLogOpen(config,on_error_stderr);

    //assert(LOG2);



    //logFlush('R', 0, "2", on_error_stderr);


    //oroWriterStart(2);




    sleep(1);

    //pthread_t morelog_thread;
    //pthread_attr_t morelog_pth_attrs_r;

    //pthread_attr_init(&morelog_pth_attrs_r);








    struct timespec cur = {0};
    struct timespec stop = {0};
    struct timespec more = {0};
    struct timespec more2 = {0};

    clock_gettime(CLOCK_ID_MEASURE, &cur);

    stop = cur;
    stop.tv_sec += 5;

    fprintf(stderr, "Start\n");
    /*char c1 = 41;
    char c2 = 42;
    char c3 = 43;
    char c4 = 44;*/



#define RDTSC_timesource


#ifndef RDTSC_timesource
    struct timespec SSS = {0};
    struct timespec EEE = {0};
    uint64_t TMP1 = 0;
#endif








    sleep(1);





    oroLogFixedA(LOG, "logLogFixed WRONG   entry str='%s' %u with TIME=%li%09li anf current sec number %l ................... ", ">str<", 1, 2, 3, 4);
    oroLogRelaxedA(LOG, "oroLogRelaxed WRONG  entry str='%s' %u with TIME=%li%09li anf current sec number %l................. ", ">str<", 1, 2, 3);
    oroLogRelaxed_XA(LOG, "oroLogRelaxed WRONG  entry str='%s' %u with TIME=%li%09li anf current sec number %l................. ", ">str<", 1, 2, 3);
    //(LOG, 25, "oroLogRelaxed WRONG  entry str='%s' %u with TIME=%li%09li anf current sec number %l ................ " ,">str<",1,2,3,4);
    oroLogFixed5_unlocked(LOG_u, "asasasas", 0, 0, 0, 0, 0);

    //    logLogFixedA(LOG, "1ogLogFixed   entry str='s' u with TIME=li09li anf current sec number li an more over and so on llu:llu");

    /*
     
     * сделать тест с двумя тредами на барьере пишуших в один лог!!!!!!!!!!
     
     */
    /////////////////////////////****************************////////////
    start = 0;
    //CLOCK_THREAD_CPUTIME_ID
    __clockid_t cl_1 = 0;
    __clockid_t cl_2 = 0;

    //pthread_getcpuclockid()
    //clock_getcpuclockid

    //cur PROCESS
    if (clock_getcpuclockid(0, &cl_1) != 0) {
        perror("clock_getcpuclockid");
    }
    if (clock_gettime(cl_1, &cur) != 0) {
        perror("clock_gettime w cl");
    }

    //cur THREAD
    if (pthread_getcpuclockid(pthread_self(), &cl_2) != 0) {
        perror("clock_getcpuclockid");
    }

    if (clock_gettime(cl_2, &stop) != 0) {
        perror("clock_gettime w cl");
    }

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &more);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &more2);

    printf("%li.%09li clock_getcpuclockid(%i)  -> %i\n", cur.tv_sec, cur.tv_nsec, 0, cl_1);
    printf("%li.%09li pthread_getcpuclockid(%lu) -> %i\n", stop.tv_sec, stop.tv_nsec, pthread_self(), cl_2);
    printf("%li.%09li CLOCK_THREAD_CPUTIME_ID -> %i\n", more.tv_sec, more.tv_nsec, CLOCK_THREAD_CPUTIME_ID);
    printf("%li.%09li CLOCK_PROCESS_CPUTIME_ID -> %i\n", more2.tv_sec, more2.tv_nsec, CLOCK_PROCESS_CPUTIME_ID);
    printf("\n\n\n");



    printf("test val 8 zero padd: >%08i<, "
            "test val 8 space padd: >%8i<, "
            "test var 10 var zero padd: %0*i, "
            "fin val 66: >%i<\n"
            , 12
            , 13
            , 10, 14,
            66
            );


    oroLogFixedA(LOG,
            "oroLogFixedA:    test val 8 zero padd: >%08i<, "
            "test val 8 space padd: >%8i<, "
            "test var 10 var zero padd: %0*i, "
            "fin val 66: >%i<\n"
            , 12
            , 13
            , 10, 14,
            66
            );
    oroLogRelaxedA(LOG,
            "oroLogRelaxed:   test val 8 zero padd: >%08i<, "
            "test val 8 space padd: >%8i<, "
            "test var 10 var zero padd: %0*i, "
            "fin val 66: >%i<\n"
            , 12
            , 13
            , 10, 14,
            66
            );
    oroLogRelaxed_XA(LOG,
            "oroLogRelaxed_X: test val 8 zero padd: >%08i<, "
            "test val 8 space padd: >%8i<, "
            "test var 10 var zero padd: %0*i, "
            "fin val 66: >%i<\n"
            , 12
            , 13
            , 10, 14,
            66
            );



    //goto end;

    for (int r = 0; r < NN; r++) {
        clock_gettime(CLOCK_ID_MEASURE, &cur);

        SFENCE;

        oroLogFixedA(LOG, "oroLogFixedA CLOCK_ID_MEASURE   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , __ASSERT_FUNCTION
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55 << 10
                , start, cycles_in_one_usec
                );
        MFENCE;

        clock_gettime(CLOCK_ID_MEASURE, &stop);
        rdtscp_S[r] = TIME_SPEC_NANO(cur);
        rdtscp_E[r] = TIME_SPEC_NANO(stop);

    }
    PRINT_STAT("logLogFixed CLOCK_ID_MEASURE", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CLOCK);
    sleep(1);



    //goto end;


    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFixed5_unlocked(LOG_u, "oroLogFixed5_unlocked   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on llu:llu"
                , (uintptr_t) __ASSERT_FUNCTION
                , (uintptr_t) NN
                , (uintptr_t) cur.tv_sec
                , (uintptr_t) cur.tv_nsec
                , (uintptr_t) 55 << 10
                //,start,cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;

    }
    PRINT_STAT("oroLogFixed5_unlocked ", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFixed5_unlocked(LOG_u, "oroLogFixed5_unlocked   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on llu:llu"
                , (uintptr_t) __ASSERT_FUNCTION
                , (uintptr_t) NN
                , (uintptr_t) cur.tv_sec
                , (uintptr_t) cur.tv_nsec
                , (uintptr_t) 55 << 10
                //,start,cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;

    }
    PRINT_STAT("oroLogFixed5_unlocked ", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);



    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFixed(LOG, 7, "2ogLogFixed   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55 << 10
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;

    }
    PRINT_STAT("logLogFixed ", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFixedA(LOG, "3ogLogFixed   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55 << 10
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogFixed AAA 3", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFixed(LOG, 7, "4ogLogFixed   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55 << 10
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogFixed 4", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        _Static_assert(COUNT_1_TO_256_ARGS(, static_string_ptr
                , NN
                , NN, NN
                , 55 << 10
                , 31337, cycles_in_one_usec) < 18, "blablabla");
        oroLogFixed(LOG, 7, "5ogLogFixed   entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                /*,cur.tv_sec
                ,cur.tv_nsec*/
                , NN, NN
                , 55 << 10
                , 31337, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogFixed STAATIC ASSERT 5", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);




    // goto end; /////////////////////////////////////////// STOP HERE ////////////////////////////////////////////

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxedA(LOG, "logLogRelaxed entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogRelaxed Cheat", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxedA(LOG, "logLogRelaxed entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogRelaxed Cheat", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);


    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxed_XA(LOG, "logLogRelaxed XXX entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogRelaxed XXX Cheat", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxed_XA(LOG, "logLogRelaxed XXX entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogRelaxed XXX Cheat", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);





    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxedA(LOG, "logLogRelaxed entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogRelaxed", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);



    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFull(LOG, 256, "logLogFulla    entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("logLogFulla", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);

    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogFull(LOG, 256, "logLogFulla    entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %llu:%llu"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10L
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;

    }
    PRINT_STAT("logLogFulla", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);


    fflush(OUT);
    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogfprintf(LOG, OUT, "fprintf         entry str='%s' %u with TIME=%li%09li anf current sec number %li an more over and so on %lu:%lu\n"
                , static_string_ptr
                , NN
                , cur.tv_sec
                , cur.tv_nsec
                , 55L << 10
                , start, cycles_in_one_usec
                );
        RDTSCP(rdtscp_E[r]);
        rdtscp_S[r] = start;
    }
    PRINT_STAT("oroLogfprintf", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    fflush(OUT);
    sleep(1);



    /*for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxedDummy(LOG,"logLogRelaxedDummy entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on"
                ,static_string_ptr
                ,NN
                ,cur.tv_sec
                ,cur.tv_nsec
                ,55<<10L
                );
        RDTSCP(rdtscp_E[r]);rdtscp_S[r]=start;
    }
    PRINT_STAT("logLogRelaxedDummy jt", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);
    
    for (int r = 0; r < NN; r++) {
        RDTSCP(start);
        oroLogRelaxedDummy_wojt(LOG,"logLogRelaxedDummy entry str='%p' %u with TIME=%li%09li anf current sec number %li an more over and so on"
                ,static_string_ptr
                ,NN
                ,cur.tv_sec
                ,cur.tv_nsec
                ,55<<10L
                );
        RDTSCP(rdtscp_E[r]);rdtscp_S[r]=start;
    }
    PRINT_STAT("logLogRelaxedDummy wo jt", rdtscp_S, rdtscp_E, rdtscp_dif, NN, CYCLES);
    sleep(1);*/


    /*
 
     perl -lane 'if(/(\d+):(\d+)$/){$cur= int(($1*1000)/$2);if(1){print scalar($cur-$prev)."\t$_";}$prev=$cur;}' logtest2_list.main.A.0.log
 
     */
end:

    fprintf(stderr, "End\n");


    fclose(OUT);

    //    free(O.buf);





    while (oroLogClose(LOG, 1000000) != 0) {
        fprintf(stderr, "Waiting for writThread to finish...\n");
    }


    fprintf(stderr, "== Cleanup! ==\n");

    oroLogDestroy(LOG);
    /*{
        unsigned long sum = 0;
        unsigned long cnt = 0;
        unsigned long min = UINT64_MAX;
        unsigned long max = 0;

        for (int r = 0; r < NN; r++) {
            unsigned long tmp = (measure_E[r].tv_sec * 1000000000 + measure_E[r].tv_nsec)-(measure_S[r].tv_sec * 1000000000 + measure_S[r].tv_nsec);
            if (tmp > 0) {
                sum += tmp;
                cnt += 1;
                if (tmp > max)
                    max = tmp;

                if (tmp < min)
                    min = tmp;

            }
        }
        fprintf(stderr, "CNT: %lu, SUM: %lu, AVG: %lu, MIN: %lu, MAX: %lu\n", cnt, sum, sum / cnt, min, max);
    }*/





    free(rdtscp_dif);
    free(rdtscp_E);
    free(rdtscp_S);
    //free(measure_S);
    //free(measure_E);


    printf("gnu_get_libc_version() = %s\n", gnu_get_libc_version());

    return (EXIT_SUCCESS);
}

int cmpare(const void *a, const void *b) {

    const uint64_t *ia = (const uint64_t *) a;

    const uint64_t *ib = (const uint64_t *) b;

    return *ia - *ib;

}

void PRINT_STAT(const char *name, uint64_t *time_S, uint64_t *time_E, uint64_t *time_Dif, int count, int ts_source) {







    uint64_t sum = 0;
    int cnt = 0;

    for (int i = 0; i < count; i++) {

        uint64_t tmp = time_E[i] - time_S[i];

        if (tmp > 0) {

            if (ts_source == CYCLES) {
                tmp = (tmp * 1000L) / cycles_in_one_usec;
            }

            time_Dif[cnt] = tmp;
            sum += tmp;

            cnt += 1;

        }

    }





    qsort(time_Dif, cnt, sizeof (uint64_t), cmpare);





    fprintf(stderr,

            "%s\t   NANO\t %lu \tCNT: %i\n"

            "MIN: %4lu, "

            "P01: %4lu, "

            "P25: %4lu, "

            "P50: [[ %4lu, ]] "

            "P75: %4lu, "

            "P99: %5lu, "

            "MAX: %4lu, "

            "AVG: %4.3f  "



            "\n\n"

            , name

            , cycles_in_one_usec

            , cnt

            , time_Dif[0]

            , time_Dif[cnt / 100]

            , time_Dif[(cnt * 25) / 100]

            , time_Dif[(cnt * 50) / 100]

            , time_Dif[(cnt * 75) / 100]

            , time_Dif[(cnt * 99) / 100]

            , time_Dif[cnt - 1]

            , sum * 1.0 / cnt



            );







}