/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

//#include <bits/string2.h>

//#include <bits/stdio.h>


/*
 * добавление логов после старта oroWriter
 * _unlocked versions
 * -------------------------remove oroObj anf oroLogFull, use oroLogFulla instead
 * -------------------------remove errno on critical path
 * -------------------------remove memset where not realy needed
 * -------------------------mlock and populate all Qs
 * -------------------------Relaxed_X - table with goto insteadof numbers
 * -------------------------remove all testing functions
 * -------------------------finaly! replace Full whis Fulla
 * вставить атрибут оптимизации типа int foo(int i) __attribute__((optimize("-O2")));
 * не проверять укзатель на logFile так же как fprintf не проверяет FILE на NULL
 * -------------------------добавить NUM в Relaxed
 * -------------------------LogFlush seep inerval через ENV? или парметры config или start_log_flush, но как-то хочется задать
 * 
 * более эллегантно синхронный старт log_writer
 * 
 * ============================ switch вметсо for в Fixed
 * 
 * проверить наличие spinlock во всех функциях log*
 * 
 * --------------------------oroLogTruncate отдельно для каждого файла
 * 
 * some placese marked with `STDERR OUTPUT !!!!` (x_alloc) - FIX
 * 
 * --------------------------------------проверить рабоу clock_gettime один раз - при отрытии лога, и больш не проверять!!!
 * 
 * окрубгления заменить на макрос round_UP
 * 
 * не проверять NUM нутри лог функций на превышение NUM_APRAM_MAX
 * 
 * обнулять LOG* как-то при останове. как? хранить адрес клиенской переминнойй?
 * 
 * 
 * макросы DEV_DEBUG для всяких "лишних" проверок
 * 
 * преаллок для Full сделал грубо. надо? размер через config?
 * 
 * возможно нормалный формат даты, через календарную функцию а не через остаток делениЯ
 */


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <stdio_ext.h>
#include <malloc.h>
#include <limits.h>
//rdtscp()
#include "oro_log.h"

#include <bits/stdio_lim.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>



#ifndef NOOPTY
#define UNLIKELY __glibc_unlikely
#define   LIKELY __glibc_likely
#else
#define UNLIKELY  
#define   LIKELY 
#endif

#define assert_msg(expr,msg)    \
  ((   LIKELY(expr)   )                        \
   ? __ASSERT_VOID_CAST (0)      \
   : __assert_fail (__STRING(msg) ": "    __STRING(expr), __FILE__, __LINE__, __ASSERT_FUNCTION))

//fence is OFF - good luck
#define Q_NEXT_FENCE

// + 2 nano
//#define Q_NEXT_FENCE    __asm__ __volatile__ ("sfence" ::: "memory");

//+15 nano both the same
//#define Q_NEXT_FENCE    __sync_synchronize();
//#define Q_NEXT_FENCE    __asm__ __volatile__ ("mfence" ::: "memory");

#define MFENCE    __asm__ __volatile__ ("mfence" ::: "memory")
#define LFENCE    __asm__ __volatile__ ("lfence" ::: "memory")
#define SFENCE    __asm__ __volatile__ ("sfence" ::: "memory")
#define RDTSCP(V) __asm__ __volatile__("rdtscp;shl $32, %%rdx;or %%rdx, %%rax":"=a"(V):: "%rcx", "%rdx")



//подобного вида печатает assert (__func__ vs __FUNCTION__ vs __PRETTY_FUNCTION__ vs )
#define ERR_MSG_HEADER "%s: %s:%u: %s: "
#define ERR_MSG_PARAMS ,__progname,__FILE__,__LINE__,__func__
//fprintf(stderr,ERR_MSG_HEADER "clock_gettime FAILed: %s\n" ERR_MSG_PARAMS,strerror(errno));
//make a format string with params to use with any forma variadic var fun
#define ERR_MESSAGE(format,...)    ERR_MSG_HEADER format  ERR_MSG_PARAMS , ## __VA_ARGS__
//fprintf(stderr, ERR_MESSAGE("val i=%i k=%iand it is not good\n",a,b)  );
//fprintf(stderr, ERR_MESSAGE("val is not good\n")  );

#define ROUND__UP(vol, round_to)  ( (     ((vol)-1) / (round_to) + 1      ) * (round_to)  )

extern const char *__progname;
//backtrace to know caller function
//#include <execinfo.h>

#define LOG_DEBUG__






//проверка парности параметров в формат строке и списке параметров
//grep -h -o -P 'error_fun\(".+;' *.c | perl -lane '/"([^"]+)"([^\;]+)/ && print "$1\n$2\n";$format=$1;$params=$2; $fc=()=$format=~/\%/g; $pc=()=$params=~/,/g;print "$fc\t$pc\n"'




//system dep len of path to file + filename, maybe... not sure. becouse 4096 is max path. does it mean path should include filename?
#ifdef FILENAME_MAX
#define LOG_F_NAME_LEN FILENAME_MAX
#else
#define LOG_F_NAME_LEN 4096
#endif


#define NOSPINLOCK

//return queue from logFlush to logLog and def size
#define RETURN_QUANTITY 2000

//hardcode for now. it is usual to sleep for 10 millisec
static __suseconds_t logFlush_empty_queue_sleep = 10000; //sleep while autoflush
static const int logFlush_fflush_sec = 1; //+sec to next fflush time
//static const long logFlush_lines_at_cycle=20000; //divide by nuber of log files
static long timezone_offset;
//static unsigned long long cycles_in_one_usec = 1000000L; //защита от деления на ноль
static struct timespec logOpenTime;
//static volatile int endlessCycle = 1;
static int time_source = CLOCK_ID_WALL_FAST; //global. same for all logs
static const size_t logLogFullBufLen = 1024;
//not need any more
//static unsigned long logQueueSize(Poro_t logFile, void (*error_fun)(char *fstring,...));
void (*error_fun)(char *fstring, ...) = NULL;



static const uint8_t stop_tb[128] __attribute__ ((aligned(sizeof (uintptr_t)))) ={
    5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* '%' */ 3, 0, /* '\''*/ 0,
    0, 0, /* '*' */ 4, /* '+' */ 0,
    0, /* '-' */ 0, /* '.' */ 0, 0,
    /* '0' */ 0, /* '1' */ 0, /* '2' */ 0, /* '3' */ 0,
    /* '4' */ 0, /* '5' */ 0, /* '6' */ 0, /* '7' */ 0,
    /* '8' */ 0, /* '9' */ 0, 0, 0,
    0, 0, 0, 0,
    0, /* 'A' */ 1, 0, /* 'C' */ 1,
    0, /* 'E' */ 1, /* F */ 1, /* 'G' */ 1,
    0, /* 'I' */ 0, 0, 0,
    /* 'L' */ 0, 0, 0, 0,
    0, 0, 0, /* 'S' */ 2,
    0, 0, 0, 0,
    /* 'X' */ 1, 0, /* 'Z' */ 0, 0,
    0, 0, 0, 0,
    0, /* 'a' */ 1, 0, /* 'c' */ 1,
    /* 'd' */ 1, /* 'e' */ 1, /* 'f' */ 1, /* 'g' */ 1,
    /* 'h' */ 0, /* 'i' */ 1, /* 'j' */ 0, 0,
    /* 'l' */ 0, /* 'm' */ 1, /* 'n' */ 1, /* 'o' */ 1,
    /* 'p' */ 1, /* 'q' */ 0, 0, /* 's' */ 2,
    /* 't' */ 0, /* 'u' */ 1, 0, 0,
    /* 'x' */ 1,
    0, 0, 0, 0, 0
};


//inernal use only, degerous!
//static int logCleaup();

static void logRedirect_stderr(Poro_t logFile, const char *funname, struct timespec time, const char* format, va_list ap);
//пока тут. куда дальше ее сунуть не знаю
static int str_s_cpy(char *dest, const char *src, size_t sizeof_dest);
//size_t const cache_line_size = 64; sysconf(_SC_LEVEL3_CACHE_LINESIZE)
static void *x_alloc_aligned_zeroed_locked(size_t __size, int must_mlock);
static void *x_alloc_aligned_zeroed_locked_page(size_t __size, int must_mlock);

//static volatile int requestForLogsTruncate = 0;


//element of log queue. holds time, format string and params

typedef struct Q_element {
    struct timespec realtime;
    //struct timespec monotime;
    const char *format_string;
    char *full_string;
    uintptr_t params[PARAMS_FIXED_NUM];
    //uintptr_t *params;
    //void *params_p;
    uintptr_t params_free_mask;
    size_t full_string_len;
    int num;
    //int64_t element_number;
    int is_alloc_addr; //when allock by pagesize only first element can be free()
    //volatile int doFlush;
    struct Q_element *next __attribute__ ((aligned(sizeof (uintptr_t))));
    ;



} Q_element __attribute__ ((aligned(sizeof (uintptr_t))));

typedef struct generic_list {
    void *p;
    long n;
    struct generic_list *next;
} generic_list;

//Log Queue. holds pointers to head/tail list to write and head/tail list  of empty elements

typedef struct LOG_Q {
    uint64_t cnt_in __attribute__ ((aligned));
    Q_element *head __attribute__ ((aligned));
    Q_element *return_tail __attribute__ ((aligned));


    //sysconf(_SC_LEVEL3_CACHE_LINESIZE) = 64
    char cache_line_pad1 [64 - sizeof (uint64_t) - sizeof (Q_element *) - sizeof (Q_element *) ];
    //char cache_line_pad1 [64 ];

    uint64_t cnt_out __attribute__ ((aligned));
    Q_element *tail __attribute__ ((aligned));
    Q_element *return_head __attribute__ ((aligned));

    //generic_list *free_list;


} LOG_QUEUE __attribute__ ((aligned));

//represent Log. holds file fd, locks, Queue

struct FD_LIST_ITEM {
    FILE *FD;

    pthread_spinlock_t insert_lock __attribute__ ((aligned));
    LOG_QUEUE QUEUE __attribute__ ((aligned(sizeof (uintptr_t))));
    ;


    char cache_line_pad1 [(sizeof (FILE *) - sizeof (pthread_spinlock_t) - sizeof (LOG_QUEUE)) % 64];


    pthread_mutex_t FD_LIST_ITEM_mutex;
    pthread_t logFlushThread;
    volatile int insert_forbidden __attribute__ ((aligned(sizeof (uintptr_t))));
    volatile int requestForLogsTruncate; //write and read in many places
    volatile int logFlushThreadStarted; //write in Flush func only, other just read



    char *custom_io_buf __attribute__ ((aligned(sizeof (uintptr_t))));
    //clockid_t time_source; //usualy no reason to use different time source
    //int timestamp_utc;     //utc is for mononic time source






    char *log_filename;


} __attribute__ ((aligned(sizeof (uintptr_t))));

//for sinchronius start of write thread
static pthread_mutex_t af_thread_startup_mutex = PTHREAD_MUTEX_INITIALIZER;

//internal fun for log cean
static int logCleaup(struct FD_LIST_ITEM *LOG);




//truncate log by signal (HUP,USR1,...)

void oroLogTruncate(Poro_t logFile) {
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;
    LOG->requestForLogsTruncate = 1;
    return;
}


//any way to respond when mlock failed?
/// Aloocate mem aligned at start and size
/// \param __size will roundup to wordsize
/// \param must_mlock -1 try do mlock, 0 do not, 1 mandatory mlock
/// \return pointer to memory

static void *x_alloc_aligned_zeroed_locked(size_t __size, int must_mlock) {
    //rount to word size
    size_t new_size = (((__size - 1) / sizeof (uintptr_t)) + 1) * sizeof (uintptr_t);

    void * P = aligned_alloc(sizeof (uintptr_t), new_size);

    if (P == NULL)
        return NULL;
    memset(P, 0, new_size);
    errno = 0;
    if (must_mlock >= 0) //-1 do not lock; 0 lock if possible; 1 exit on lock fail
        if (mlock(P, new_size) != 0) {
            if (must_mlock == 1) {
                //STDERR OUTPUT !!!!
                assert_perror(errno);
                //fprintf(stderr, "mlock in x_alloc_aligned_zeroed_locked failed: %s\n",strerror(errno));
                free(P);
                P = NULL;
            }
        }
    return P;


}
//__size % pagesize == 0  !!!!
/// Allocate by whole pages, alligned
/// \param __size must by round to page size
/// \param must_mlock -1 try do mlock, 0 do not, 1 mandatory mlock
/// \return pointer to memory

static void *x_alloc_aligned_zeroed_locked_page(size_t __size, int must_mlock) {

    size_t pagesize = getpagesize();
    assert((__size % pagesize) == 0);

    void * ptr = aligned_alloc(pagesize, __size);
    assert(((uintptr_t) ptr % pagesize) == 0);

    if (ptr == NULL)
        return NULL;
    memset(ptr, 0, __size);
    errno = 0;
    if (must_mlock >= 0) //-1 do not lock; 0 lock if possible; 1 exit on lock fail
        if (mlock(ptr, __size) != 0) {
            if (must_mlock == 1) {
                //STDERR OUTPUT !!!!
                assert_perror(errno);
                //fprintf(stderr, "mlock in x_alloc_aligned_zeroed_locked_page failed: %s\n",strerror(errno));
                free(ptr);
                ptr = NULL;
            }
        }
    return ptr;


}

/// Allocate Queue elements packed to memory pages 
/// \param tail pointer to tail of existed list, link "tail" to end
/// \param lock -1 try do mlock, 0 do not, 1 mandatory mlock
/// \return return head of list

static Q_element * alloc_Q_list_inside_page(Q_element * tail, int lock) {
    size_t pagesize = getpagesize();
    size_t n_of_Qe_in_one_page = pagesize / sizeof (Q_element);
    //pagesize/sizeof (Q_element)

    Q_element *Q_e_ptr = x_alloc_aligned_zeroed_locked_page(pagesize, lock); //FREE
    assert_msg(Q_e_ptr != NULL, "x_alloc_aligned_zeroed_locked_page return NULL");

    Q_e_ptr->is_alloc_addr = 1;

    for (int e = 0; e < (n_of_Qe_in_one_page - 1); e++) {
        //Q_e_ptr[e].params=calloc(PARAMS_FIXED_NUM,sizeof(uintptr_t));
        Q_e_ptr[e].next = &(Q_e_ptr[e + 1]);
        Q_e_ptr[e].full_string = x_alloc_aligned_zeroed_locked(logLogFullBufLen, -1); //FREE
        Q_e_ptr[e].full_string_len = pagesize;
    }
    //alloc one page for logFull, less?
    Q_e_ptr[n_of_Qe_in_one_page - 1].full_string = x_alloc_aligned_zeroed_locked(logLogFullBufLen, -1);
    Q_e_ptr[n_of_Qe_in_one_page - 1].full_string_len = pagesize;
    Q_e_ptr[n_of_Qe_in_one_page - 1].next = tail;

    return Q_e_ptr;
}


/// Init internal general зфкфьы
/// \param time__source CLOCK_REALTIME/CLOCK_MONOTONIC/XXX_COARSE
/// \param timestamp_utc add local offset to utc timestamp (makes sense for REALTIME only)
/// \param sleep WriteThread sleep if nothing todo in milliseconds (-1 mean def 10msec)
/// \param error__fun function pointer to be colled on internal errors. may be null

void oroInit(int time__source, int timestamp_utc, int sleep, void (*error__fun)(char *fstring, ...)) {
    //global vars
    time_source = time__source;
    error_fun = error__fun;
    //;eav def until 0..N value
    if (sleep >= 0)
        logFlush_empty_queue_sleep = sleep;

    //нужно для даты в имени файла
    if (timestamp_utc != 1) {
        struct tm *l_time = NULL;
        time_t t = time(NULL);
        l_time = localtime(&t);
        assert_msg(l_time != NULL, "localtime FAILed");

        timezone_offset = l_time->tm_gmtoff;
    }


}

//individual thread for each LOG

static void * logWrite_thread(void *logFile) {

    //maybe set from config throw LOG individualy?
    __suseconds_t usec_sleep = logFlush_empty_queue_sleep; //GLOBAL

    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;


    //buf for log record date and date format
    char DATE_FORMAT_STRING[] __attribute__ ((aligned(sizeof (uintptr_t)))) = "00:00:00.000000000;;";
    const char date_format_string[] __attribute__ ((aligned(sizeof (uintptr_t)))) = "%02i:%02i:%02i.%%09li;;";
    //                                                         ; TID 139837019477824
    //char TID[] __attribute__ ((aligned(sizeof(uintptr_t)))) = "; TID 18446744073709551615\n";




    struct timespec prevFlush = {.tv_nsec = 0, .tv_sec = 0};



    if (LOG == NULL) {

        //go to exit
        pthread_mutex_unlock(&af_thread_startup_mutex);
        goto logFlush_thread__exit;
    }

    //activate logs

    LOG->insert_forbidden = 0;
    LOG->logFlushThreadStarted = 1;


    pthread_mutex_unlock(&af_thread_startup_mutex);

    //ready to exit counter (+3 time get state: write=0 and insert forbidden)
    int ready_to_exit = 0;

    //endless check and write
    for (;;) {


        //lock for some vars antil while() ends, then sleep (sleep out of lock!)
        pthread_mutex_lock(&LOG->FD_LIST_ITEM_mutex);

        if (UNLIKELY(LOG->requestForLogsTruncate)) {

            LOG->requestForLogsTruncate = 0;

            FILE *F = LOG->FD;

            if (F) {
                //такойспособ не обнуляет блокировку файла
                errno = 0;
                if (fseek(F, 0, SEEK_SET) != 0) {
                    if (error_fun)
                        error_fun(ERR_MESSAGE("Log Truncate failed (rewind): %s\n", strerror(errno)));
                    LOG->insert_forbidden = 1;
                } else {
                    errno = 0;
                    if (ftruncate(fileno(F), 0) != 0) {
                        if (error_fun)
                            error_fun(ERR_MESSAGE("Log Truncate failed (trunc): %s\n", strerror(errno)));

                        LOG->insert_forbidden = 1;
                    }
                }
            }
        }

        //счетчик того сколько всесего записей сделано по всем файлам. если ни одной, то значит ничего не происходит, поспим немного
        long write_cnt = 0;
        //int insert_forbidden_FD = 0;




        __time_t prev_sec = 0;
        int hr = 0;
        int min = 0;
        int sec = 0;

        FILE *F = LOG->FD;

        //if(LOG->insert_forbidden)                insert_forbidden_FD+=1;

        //обратное условие проверим через три строки. там его надо каждый раз проверять

        LOG_QUEUE *Q = &(LOG->QUEUE);





        //по списку одно условие - есть следующий эллемент
        while (LIKELY(F != NULL)
                &&
                Q->tail->next != NULL
                ) {




            /*ПЕЧАТЬ!!*/

            //volatile 
            Q_element *E = Q->tail->next;

            //clock_gettime(CLOCK_ID_WALL_FAST,&(E->realtime));

            //запомнить предыдущую секунду и не высчитывать hr min sec заново!!!

            if (UNLIKELY(prev_sec != E->realtime.tv_sec)) {
                /*
                 * 
                 * возможно нормалный формат даты, через календарную функцию
                     
                 */
                prev_sec = E->realtime.tv_sec;

                //sec = (prev_sec + (LOG->timestamp_utc?0:timezone_offset) ) % 86400; //секунд в последних судках. 24*60*60=86400
                sec = (prev_sec + timezone_offset) % 86400; //секунд в последних судках. 24*60*60=86400

                min = sec / 60;

                sec = sec % 60;

                hr = min / 60;

                min = min % 60;



                int s = snprintf(
                        DATE_FORMAT_STRING
                        , sizeof (DATE_FORMAT_STRING),
                        date_format_string //"%02i:%02i:%02i.%09li;;"

                        , hr
                        , min
                        , sec
                        //, E->realtime.tv_nsec
                        );

                //на случай если что-то напутали в формат строке
                assert_msg(sizeof (DATE_FORMAT_STRING) > s, "snprintf on DATE_FORMAT_STRING overflow");


            }





            //17:41:05.848899473|1475499930484531;;  <37+\0>

            //не слишком ли круто?
            //assert_msg(E->format_string != NULL, "Entry in log Queue with empty format string");
            //может проще? if(E->format_string == NULL) E->format_string="";
            if (E->format_string == NULL)
                E->format_string = "(null)";



            //fputs_unlocked(DATE_FORMAT_STRING,F);
            //вписать наносекунды
            if (time_source >= 0)
                fprintf(F, DATE_FORMAT_STRING, E->realtime.tv_nsec);




            switch (E->num) {

                case -1: //E->num == -1 means logLogFull
                {

                    fputs_unlocked(E->full_string, F);
                    break;

                }

                case 0: //print static string without any params (call to Fixed/Relaxed with NUM=0)
                {
                    fputs_unlocked(E->format_string, F);

                    E->format_string = NULL;
                    break;
                }

                default: // print with params
                {
                    uintptr_t *p = E->params;

                    //COMPILLE TIME limit on params number
                    _Static_assert(PARAMS_FIXED_NUM < 33, "HARCODED MAXIMUM FOR PARAMS_FIXED_NUM IS 32");


                    fprintf(F, E->format_string,
                            p[0]
#if PARAMS_FIXED_NUM > 1
                            , p[1]
#endif
#if PARAMS_FIXED_NUM > 2
                            , p[2]
#endif
#if PARAMS_FIXED_NUM > 3
                            , p[3]
#endif
#if PARAMS_FIXED_NUM > 4
                            , p[4]
#endif
#if PARAMS_FIXED_NUM > 5
                            , p[5]
#endif
#if PARAMS_FIXED_NUM > 6
                            , p[6]
#endif
#if PARAMS_FIXED_NUM > 7
                            , p[7]
#endif
#if PARAMS_FIXED_NUM > 8
                            , p[8]
#endif
#if PARAMS_FIXED_NUM > 9
                            , p[9]
#endif
#if PARAMS_FIXED_NUM > 10
                            , p[10]
#endif
#if PARAMS_FIXED_NUM > 11
                            , p[11]
#endif
#if PARAMS_FIXED_NUM > 12
                            , p[12]
#endif
#if PARAMS_FIXED_NUM > 13
                            , p[13]
#endif
#if PARAMS_FIXED_NUM > 14
                            , p[14]
#endif
#if PARAMS_FIXED_NUM > 15
                            , p[15]
#endif
#if PARAMS_FIXED_NUM > 16
                            , p[16]
#endif
#if PARAMS_FIXED_NUM > 17
                            , p[17]
#endif
#if PARAMS_FIXED_NUM > 18
                            , p[18]
#endif
#if PARAMS_FIXED_NUM > 19
                            , p[19]
#endif
#if PARAMS_FIXED_NUM > 20
                            , p[20]
#endif
#if PARAMS_FIXED_NUM > 21
                            , p[21]
#endif
#if PARAMS_FIXED_NUM > 22
                            , p[22]
#endif
#if PARAMS_FIXED_NUM > 23
                            , p[23]
#endif
#if PARAMS_FIXED_NUM > 24
                            , p[24]
#endif
#if PARAMS_FIXED_NUM > 25
                            , p[25]
#endif
#if PARAMS_FIXED_NUM > 26
                            , p[26]
#endif
#if PARAMS_FIXED_NUM > 27
                            , p[27]
#endif
#if PARAMS_FIXED_NUM > 28
                            , p[28]
#endif
#if PARAMS_FIXED_NUM > 29
                            , p[29]
#endif
#if PARAMS_FIXED_NUM > 30
                            , p[30]
#endif
#if PARAMS_FIXED_NUM > 31
                            , p[31]
#endif

                            );

                    //free some params if needed
                    if (E->params_free_mask) {

                        uintptr_t f = E->params_free_mask;

                        for (int i = 0; i < PARAMS_FIXED_NUM && f != 0; i++) {
                            if ((f & 0x1) == 0x1) {
                                free((void*) (p[i]));
                                p[i] = 0;
                            }
                            f >>= 1;

                        }
                        //sure?
                        //E->params_free_mask = 0;
                    }

                }//default

            }//switch 


            fputs_unlocked("\n", F);

            /*if(E->doFlush==1){
                fflush_unlocked(LOG->FD);
                E->doFlush=0;
            }*/





            //write_limit_cnt++;
            write_cnt++;


            //нужно ли делать memset(0)?? можно потом не делать. или нельзя?
            Q->tail->next = NULL;
            Q->return_head->next = Q->tail;
            Q->return_head = Q->tail;


            Q->tail = E;
            //Q->cnt_out+=1;

        }//while

        //job done for now. unlock and sleep
        pthread_mutex_unlock(&LOG->FD_LIST_ITEM_mutex);

        //ни одной записи? exit или спать!
        if (write_cnt == 0) {

            if (LOG->insert_forbidden == 1) {
                ready_to_exit += 1;
                if (ready_to_exit > 1)
                    goto logFlush_thread__exit;
                //возможно комбо при котором набо обнлить ready_to_exit?
            }


            pthread_yield();

            //for (int N = 0; N < FD_LIST_ITEM_NUM; N++)                fputs_unlocked("#sleep\n",FD_ARR[N].FD);
            if (usec_sleep) { //? почему тут не локальная usec_sleep а глобальная????

                struct timeval sleep_timeout = {0, usec_sleep};

                select(0, NULL, NULL, NULL, &sleep_timeout);
                //may end erlier then sleep_timeout in case of interrupt...

            }


        } else
            write_cnt = 0;

        //FLUSH every SECOND
        struct timespec Flush = {0};
        clock_gettime(CLOCK_ID_MEASURE_FAST, &Flush);
        /*errno = 0;
        if (clock_gettime(CLOCK_ID_MEASURE_FAST, &Flush) < 0) {
            if(error_fun)
                error_fun(ERR_MESSAGE("clock_gettime FAILed: %s\n", strerror(errno)));
            
        }*/
        if (Flush.tv_sec > prevFlush.tv_sec) { //HARDCODE flsuh every 1 sec

            pthread_mutex_lock(&LOG->FD_LIST_ITEM_mutex);
            if (LOG->FD != NULL) {
                errno = 0;
                fputs_unlocked("#fflush\n", LOG->FD);

                fflush_unlocked(LOG->FD);
                if (errno) {
                    if (error_fun)
                        error_fun(
                            ERR_MESSAGE("fflush on %s: %s\n", LOG->log_filename, strerror(errno))
                            );
                }
            }

            prevFlush.tv_sec = Flush.tv_sec + logFlush_fflush_sec;

            pthread_mutex_unlock(&LOG->FD_LIST_ITEM_mutex);
        }




    }//endlessCycle



    //the only return point!!!
logFlush_thread__exit:
    LOG->logFlushThreadStarted = 0;


    return NULL;
}

static int logWriterStart(struct FD_LIST_ITEM *LOG, int bind_cpu) {
    pthread_attr_t af_pth_attrs_r;



    if (LOG->logFlushThreadStarted) {
        if (error_fun)error_fun(
                ERR_MESSAGE("logFlush restart attempt\n")
                );
        return -1;
    }


    if (pthread_attr_init(&af_pth_attrs_r) != 0) {
        if (error_fun)error_fun(
                ERR_MESSAGE("pthread_attr_init FAILed\n")
                );
        return -1;
    }
    if (bind_cpu >= 0) { //valid cpu 0..1024
#ifdef __CYGWIN__
        if (error_fun)error_fun(
                ERR_MESSAGE("CYGWIN!! IGNORE AFFINITY to CPU %i\n", bind_cpu)
                );
#else        
        cpu_set_t cpus_af;
        CPU_ZERO(&cpus_af);
        CPU_SET(bind_cpu, &cpus_af);

        int r = 0;
        if ((r = pthread_attr_setaffinity_np(&af_pth_attrs_r, sizeof (cpu_set_t), &cpus_af)) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("affinity to CPU %i pthread_attr_setaffinity_np FAILed: %s\n", bind_cpu, strerror(r))
                    );
            return -1;
        } else {
            if (error_fun)error_fun(
                    ERR_MESSAGE("affinity to CPU %i\n", bind_cpu)
                    );
        }
#endif
    }//bind cpu
    else {
        if (error_fun)error_fun(
                ERR_MESSAGE("affinity to CPU %i not possible\n", bind_cpu)
                );
    }

    int af_thread_startup_mutex_lock = pthread_mutex_lock(&af_thread_startup_mutex);
    assert(af_thread_startup_mutex_lock == 0);
    //&logFlush_empty_queue_sleep
    int r = 0;
    if ((r = pthread_create(&(LOG->logFlushThread), &af_pth_attrs_r, logWrite_thread, LOG)) != 0) {
        if (error_fun)error_fun(
                ERR_MESSAGE("pthread_create logFlush_thread error %i: %s\n", r, strerror(r))
                );
        pthread_mutex_unlock(&af_thread_startup_mutex);
        pthread_attr_destroy(&af_pth_attrs_r);
        return -1;
        //FATAL!
    }

    //wait logFlush startup
    int af_thread_startup_mutex_lock_again = pthread_mutex_lock(&af_thread_startup_mutex);
    assert(af_thread_startup_mutex_lock_again == 0);

    pthread_mutex_unlock(&af_thread_startup_mutex);

    if (LOG->logFlushThreadStarted != 1) {
        if (error_fun)error_fun(
                ERR_MESSAGE("FILE %s, LINE %i, logFlush start failed")
                );
        return -1;
    }


    {
        int s = pthread_attr_destroy(&af_pth_attrs_r);
        if (s != 0)
            if (error_fun)error_fun(
                    ERR_MESSAGE("pthread_attr_destroy: %i", s)
                    );
    }



    {
        int r = 0;
        if ((r = pthread_setname_np(LOG->logFlushThread, "logFlush")) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("pthread_setname_np for logAutoflush_thread error %i: %s\n", r, strerror(r))
                    );
            //continue, not fatal
        }
    }

    return 0;

}

int oroLogClose(Poro_t logFile, long int wait_usec) {


    __suseconds_t wait = wait_usec;

    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;



    pthread_mutex_lock(&LOG->FD_LIST_ITEM_mutex);
    LOG->insert_forbidden = 1;
    pthread_mutex_unlock(&LOG->FD_LIST_ITEM_mutex);





    for (;;) {


        int still_running = 0;

        //logFlushThreadStarted sets in FlusThread only. 
        //Can we bee shure logFlushThreadStarted=0 means Thread is dead?
        still_running = LOG->logFlushThreadStarted;


        if (still_running == 0)
            break;


        if (wait != 0) {
            //если задана пауза - ждать один раз и обнулить ее. на след. круг сразу выйдем с -1
            select(0, NULL, NULL, NULL, &((struct timeval){0, wait}));
            wait = 0;
        } else //logFlush still working
            return -1;

    }

    //Hope logFlush thread already finish

    //join for cleanup


    int r = 0;
    if ((r = pthread_join(LOG->logFlushThread, NULL)) != 0) {
        if (error_fun)error_fun(
                ERR_MESSAGE("pthread_join to logFlush_thread error %i: %s\n", r, strerror(r))
                );
        //continue, not fatal
        //may be not statrted
    }



    //Close all FD, free Q, io_buf, ...
    int cc = logCleaup(LOG);




    return cc;



}



//internal. do after oroLogClose

static int logCleaup(struct FD_LIST_ITEM *LOG) {

    assert_msg(LOG->FD != NULL, "Close dead LOG? Twice?");

    assert_msg(LOG->insert_forbidden == 1, "LOG must be locked already!");

    //lock if LogFixed/LogRelaxed/logFull is running
    pthread_spin_lock(&LOG->insert_lock);

    //FILE *F = FD_ARR[N].FD;
    LOG_QUEUE *Q = &(LOG->QUEUE);


    //мы не должны попадать сюда с полной очередью
    assert_msg(Q->head == Q->tail, "Cleanup non empty Queue");


    pthread_mutex_lock(&LOG->FD_LIST_ITEM_mutex);

    if (LOG->FD) {
        fprintf(LOG->FD, "#Closed %s.\n", LOG->QUEUE.cnt_in == LOG->QUEUE.cnt_out ? "normally" : " truncated. IN<>OUT");

        errno = 0;
        if (fflush(LOG->FD) != 0) {
            if (error_fun)
                error_fun(ERR_MESSAGE("fflush on %s: %s\n", LOG->log_filename, strerror(errno))
                    );
        }
        errno = 0;
        if (fclose(LOG->FD) != 0) {
            if (error_fun)
                error_fun(ERR_MESSAGE("fclose on %s: %s\n", LOG->log_filename, strerror(errno))
                    );
        }
        LOG->FD = NULL;


    }
    pthread_mutex_unlock(&LOG->FD_LIST_ITEM_mutex);

    //лог должен быть уже закрыт
    //assert_msg(FD_ARR[N].FD == NULL, "Cleanup non closed FD");

    //free setvbuf allocation
    if (LOG->custom_io_buf) {
        free(LOG->custom_io_buf);
        LOG->custom_io_buf = NULL;
    }

    if (LOG->log_filename) {
        free(LOG->log_filename);
        LOG->log_filename = NULL;
    }

    //т.к. аллокация страницами мы не можем осободить серединку
    //free(Q->head);
    void *free_later = NULL;
    //если так поучилось, что сдесь залип первый эллемент, мы не можем его высвободить, т.к. будем сейчас ходить по связанным с ним эллементам
    //запомним и освободим позже
    //Q->head==Q->tail

    if (Q->head) {

        if (Q->head->full_string) {
            free(Q->head->full_string);
        }

        if (Q->head->is_alloc_addr) {
            free_later = Q->head;
        }
        memset(Q->head, 0, sizeof (Q_element));
        Q->tail = Q->head = NULL;
    }



    if (Q->return_tail) {

        long n = 0;
        generic_list *L = NULL;

        for (Q_element *tmp = Q->return_tail; tmp != NULL;) {
            //free and zerro all inside
            Q_element * next = tmp->next;
            if (tmp->full_string) {
                free(tmp->full_string);
            }

            //collect addrs to free later
            if (tmp->is_alloc_addr) {

                generic_list *newL = calloc(1, sizeof (generic_list));
                newL->p = tmp;
                newL->n = ++n;
                newL->next = L;
                L = newL;
            }
            memset(tmp, 0, sizeof (Q_element));
            tmp = next;
        }
        for (generic_list *tmp = L; tmp != NULL;) {
            generic_list *next = tmp->next;
            free(tmp->p);
            free(tmp);
            tmp = next;
        }

        Q->return_tail = NULL;


    }
    if (free_later) {
        free(free_later);
        free_later = NULL;
    }

    Q->head = Q->return_head = Q->return_tail = Q->tail = NULL;


    pthread_spin_unlock(&LOG->insert_lock);






    return 0;


}

//final cleaup. do if shure that no oroLogXXX can be invoked

void oroLogDestroy(Poro_t logFile) {


    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;

    assert_msg(LOG->insert_forbidden == 1, "call to oroLogDestroy of alive log");
    assert_msg(LOG->FD == NULL, "call to oroLogDestroy of alive log");

    free(LOG);

}

Poro_t_unlocked oroLogOpen_onlocked(oro_attrs_t config) {

    //for unlocked version use separate fun
    Poro_t tmp = oroLogOpen(config);

    return (Poro_t_unlocked) tmp;

}
/// Open new log and run write thread for it.
/// \param config params of log
/// \return pointer to be used with oroLogXXX or oroClose/oroDestroy

Poro_t oroLogOpen(oro_attrs_t config) {


    assert_msg(config.f_name_part1 != NULL && strlen(config.f_name_part1) > 0, "First part of file name MUST not be zero length");




    if (config.return_q == 0)
        config.return_q = RETURN_QUANTITY;

    size_t pagesize = 4096;

    {
        int t_ps;
        if ((t_ps = getpagesize()) > 0)
            pagesize = t_ps;
    }

    struct FD_LIST_ITEM * LOG = x_alloc_aligned_zeroed_locked(sizeof (struct FD_LIST_ITEM), config.do_mlock);
    assert_msg(LOG != NULL, "new LOG aligned_alloc failed");








    //int p = PTHREAD_PROCESS_PRIVATE;
    //errno=0;
    assert_msg(
            pthread_spin_init(&(LOG->insert_lock), PTHREAD_PROCESS_PRIVATE) == 0
            , "pthread_spin_init FAILed");


    LOG->FD = NULL;


    char log_filename_template[LOG_F_NAME_LEN] = {0};







    if (logOpenTime.tv_sec == 0) {
        errno = 0;
        //критично. сразу на выход
        //проверяем что clock_gettime работает только здесь!!!
        //больше нигде
        assert_msg(clock_gettime(CLOCK_ID_WALL_FAST, &logOpenTime) == 0, "clock_gettime FAILed");
    }

    //нужно для даты в имени файла
    struct tm *br_dw_time = NULL;
    br_dw_time = localtime(&logOpenTime.tv_sec);
    assert_msg(br_dw_time != NULL, "localtime FAILed");

    //нужно потом, для правильной печати даты
    //if(config.timestamp_utc != 1)
    //    timezone_offset=br_dw_time->tm_gmtoff;



    //file name construct
    //   path / f_name_part1 . f_name_part2  . f_name_part3 . datetime .log


    //memset(log_filename_template,0,sizeof(log_filename_template));
    char *p = log_filename_template;
    const size_t so = sizeof (log_filename_template);

    if (config.path) {
        size_t path_len = strlen(config.path);
        if (path_len > 0) {
            assert_msg(
                    config.path[path_len - 1] == '/'
                    ||
                    config.path[path_len - 1] == '\\'
                    , "Path should ends with slash");

            int copied = str_s_cpy(p, config.path, so - (p - log_filename_template));

            assert_msg(copied > 0, "log path exceed filesystem maximum len");

            p += copied;
        }
    }
    //copy f_name_part1. we've checked len priviusly
    {

        int copied = str_s_cpy(p, config.f_name_part1, so - (p - log_filename_template));
        assert_msg(copied > 0, "f_name_part1 exceed filesystem maximum len");
        p += copied;
    }

    if (config.f_name_part2 && strlen(config.f_name_part2)) {
        *p++ = '.';

        int copied = str_s_cpy(p, config.f_name_part2, so - (p - log_filename_template));
        assert_msg(copied > 0, "f_name_part1 exceed filesystem maximum len");
        p += copied;

    }

    if (config.f_name_part3 && strlen(config.f_name_part3)) {
        *p++ = '.';

        int copied = str_s_cpy(p, config.f_name_part3, so - (p - log_filename_template));
        assert_msg(copied > 0, "f_name_part3 exceed filesystem maximum len");
        p += copied;

    }

    if (config.f_name_strftime) {
        if (strlen(config.f_name_strftime) == 0)
            config.f_name_strftime = "%Y%B%d-%H%M";
        *p++ = '.';

        size_t limit = so - (p - log_filename_template);
        int copied = strftime(p, limit, config.f_name_strftime, br_dw_time);
        assert_msg(copied != 0 && copied < limit, "strftime fail on format (date part) or exceed filesystem maximum len");

        /*int copied = snprintf(p, limit, "%04i%02i%02i-%02i%02i"
        , br_dw_time->tm_year + 1900
        , br_dw_time->tm_mon + 1
        , br_dw_time->tm_mday
        , br_dw_time->tm_hour
        , br_dw_time->tm_min);
        
    
        assert_msg(limit > copied ,"date part exceed filesystem maximum len");
         */
        p += copied;
    }

    if (config.file_name_with_pid) {
        *p++ = '.';

        size_t limit = so - (p - log_filename_template);
        int copied = snprintf(p, limit, "%i", getpid());

        assert_msg(limit > copied, "date part exceed filesystem maximum len");
        p += copied;
    }

    {//add .log
        int copied = str_s_cpy(p, ".log", so - (p - log_filename_template));
        assert_msg(copied > 0, "ending .log exceed filesystem maximum len");
        p += copied;
    }




    //filename ready

    errno = 0;
    FILE *t = fopen(log_filename_template, "a+");
    if (t == NULL) {
        if (error_fun)error_fun(
                ERR_MESSAGE("fopen (a+) FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                );
        goto logOpen__exit;
    }

    errno = 0;
    if (lockf(fileno(t), F_TLOCK, 0) != 0) {
        if (error_fun)error_fun(
                ERR_MESSAGE("Lock FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                );
        goto logOpen__exit;
    }

    if (config.truncate) {
        errno = 0;
        if (fseek(t, 0, SEEK_SET) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("Rewind FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                    );
            goto logOpen__exit;
        }


        errno = 0;
        if (ftruncate(fileno(t), 0) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("Truncate FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                    );
            goto logOpen__exit;
        }
    }

    ////  FILE OPENED AND LOCKED


    if (config.bufsize > 0) {
        size_t st_blksize = 4096;


        struct stat sb;
        errno = 0;
        if (fstat(fileno(t), &sb) == 0) {
            if (sb.st_blksize > 0)
                st_blksize = sb.st_blksize;
        } else {
            if (error_fun)error_fun(
                    ERR_MESSAGE("fstat to get st_blksize FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                    );
        }
        //round bufsize to st_blksize
        config.bufsize = (((config.bufsize - 1) / st_blksize) + 1) * st_blksize;

        //round alloc to pagesize
        size_t alloc_size = (((config.bufsize - 1) / pagesize) + 1) * pagesize;
        //maybe round alloc size to page size? but pass to setvbuf size of config.bufsize (end of alloca spase not used)
        LOG->custom_io_buf = x_alloc_aligned_zeroed_locked_page(alloc_size, config.do_mlock);
        assert_msg(LOG->custom_io_buf != NULL, "custom_io_buf = aligned_alloc");
        errno = 0;
        //set custom buf with _IOFBF  (size of buf!!! no alloc size!!!! if alloc>buf then tail not used)
        if (setvbuf(t, LOG->custom_io_buf, _IOFBF, config.bufsize) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("setvbuf custom buf size FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                    );
            goto logOpen__exit;
        }
        //Z = __fbufsize(t);
        //fprintf(stderr,"__fbufsize: %zu\n",Z); 
    } else {
        errno = 0;
        //just set _IOFBF (buf size should be st_blksize by default)
        if (setvbuf(t, NULL, _IOFBF, 0) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("setvbuf _IOFBF FAILed for '%s', err: %s\n", log_filename_template, strerror(errno))
                    );
            goto logOpen__exit;
        }
    }







    size_t n_of_Qe_in_one_page = pagesize / sizeof (Q_element);
    config.return_q = (((config.return_q - 1) / n_of_Qe_in_one_page) + 1) * n_of_Qe_in_one_page;
    size_t total_pages_we_need = config.return_q / n_of_Qe_in_one_page;

    /*fprintf(stderr,"PARAMS_FIXED_NUM: %i\n"
            "n_of_Qe_in_one_page: %zu\n"
            "sizeof (Q_element): %zu\n"
            "total_pages_we_need: %zu\n"
            , PARAMS_FIXED_NUM
            ,n_of_Qe_in_one_page
            ,sizeof (Q_element)
            ,total_pages_we_need);*/

    Q_element *HEAD = NULL;
    Q_element *TAIL = NULL;

    //формируем в голову
    for (int p = 0; p < total_pages_we_need; p++) {
        HEAD = alloc_Q_list_inside_page(HEAD, config.do_mlock);
    }
    //поиск конца
    for (Q_element *tmp = HEAD; tmp != NULL; tmp = tmp->next) {
        if (tmp->next == NULL)
            TAIL = tmp;
    }


    LOG->QUEUE.return_head = TAIL;
    LOG->QUEUE.return_tail = HEAD->next;

    //first element use for WriteThread passiv element
    LOG->QUEUE.head = HEAD;
    LOG->QUEUE.tail = HEAD;
    HEAD->next = NULL;



    fprintf(t, "# File opened. time_source %i, timezone_offset %li, bufsize %zi, return_q %i, bind CPU %i\n", time_source, timezone_offset, config.bufsize, config.return_q, config.bind_cpu);


    LOG->FD = t;


    //for errors with FD
    LOG->log_filename = strdup(log_filename_template);



    //until logFlush starts do not let oroLogXXX work
    LOG->insert_forbidden = 1;




    //start write thread
    if (logWriterStart(LOG, config.bind_cpu) != 0) {
        LOG->logFlushThread = 0;
        goto logOpen__exit;
    }

    //the only exit point!!!!


    return (Poro_t) LOG;

logOpen__exit:
    return NULL;
}

/// (inernal)Redisrect to stderr if LOG file is not ready (insert forbidn))
/// \param logFile
/// \param funname
/// \param time
/// \param format
/// \param ...

static void logRedirect_stderr_f(Poro_t logFile, const char *funname, struct timespec time, const char* format, ...) {
    va_list arglist;
    va_start(arglist, format);
    logRedirect_stderr(logFile, funname, time, format, arglist);
    va_end(arglist);

}

static void logRedirect_stderr(Poro_t logFile, const char *funname, struct timespec time, const char* format, va_list ap) {
    //add  logFile inernal name or full path
    //lock
    char *eformat;
    if (asprintf(&eformat, "%li.%09li\t%s(%p): %s\n", time.tv_sec + timezone_offset, time.tv_nsec, funname, logFile, format) < 0) {
        return;
    } else {

        if (eformat) {
            vfprintf(stderr, eformat, ap);
            free(eformat);
        }
    }

    //unlock
}


/// Log with no locks atall. can be dangerous. faster then Fixed about 5-10nanosec. need 5 params for format string. not more. not less. feed zoro if not used. Only static fromat string.
/// \param logFile
/// \param format
/// \param p1
/// \param p2
/// \param p3
/// \param p4
/// \param p5

void oroLogFixed5_unlocked(Poro_t_unlocked logFile, const char* format, uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4, uintptr_t p5) {
    _Static_assert(PARAMS_FIXED_NUM >= 5, "oroLogFixed8 need 8 params to store");

    struct timespec time = {0};
    //time_source is global
    if (time_source >= 0)
        clock_gettime(time_source, &time);

    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;


    //Do we need sanity check?
    //assert_msg(LOG->withlock == 0, "Use oroLogFixed5_unlocked for LOG expecting locks");


    if (UNLIKELY(LOG->insert_forbidden == 1)) {
        logRedirect_stderr_f((Poro_t) logFile, __func__, time, format, p1, p2, p3, p4, p5);
        return;
    }



    Q_element *ENTRY;
    LOG_QUEUE *Q = &(LOG->QUEUE);

    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //mLOCK after start is dangeruus!!!!
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;
    Q->return_tail = Q->return_tail->next;



    //MFENCE;
    //MFENCE_LITE;

    //а нада?
    //assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");


    uintptr_t *p = ENTRY->params;

    p[0] = p1;
    p[1] = p2;
    p[2] = p3;
    p[3] = p4;
    p[4] = p5;

    ENTRY->format_string = format;
    ENTRY->num = 5;
    ENTRY->params_free_mask = (uintptr_t) 0;

    ENTRY->realtime = time;
    ENTRY->next = NULL;

    //    Q_NEXT_FENCE;
    //__sync_synchronize();



    /*next!*/
    Q->head->next = ENTRY;
    Q->head = ENTRY;
    //Q->cnt_in+=1;


}

/**
 * Log format string with fixed numer of prameters. Only static fromat string. Only copyble params allowed! Do not log non static strings! Use macro oroLogFixedA to count NUM at compille time
 * @param logFile
 * @param NUM
 * @param format
 * @param ...
 */
void oroLogFixed(Poro_t logFile, size_t NUM, const char* format, ...) {

    struct timespec time = {0};

    //time_source is global
    if (time_source >= 0)
        clock_gettime(time_source, &time);


    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;


    va_list arglist;

    pthread_spin_lock(&LOG->insert_lock);

    if (UNLIKELY(LOG->insert_forbidden == 1)) {

        va_start(arglist, format);
        logRedirect_stderr(logFile, __func__, time, format, arglist);
        va_end(arglist);

        return;
    }



    Q_element *ENTRY;
    LOG_QUEUE *Q = &(LOG->QUEUE);




    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //lock after start is dangerous!!! 
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;
    Q->return_tail = Q->return_tail->next;


    //MFENCE;
    //MFENCE_LITE;


    uintptr_t *p = ENTRY->params;

    va_start(arglist, format); //LAST PARAM!!!

    //switch???
    //боюсь что забирая значения по ширине машинного слова, могу забрать сразу два
    for (int num = 0; num < NUM; num++) {
        p[num] = va_arg(arglist, uintptr_t);
    }


    va_end(arglist);

    ENTRY->format_string = format;
    ENTRY->num = NUM;
    ENTRY->params_free_mask = (uintptr_t) 0;

    ENTRY->realtime = time;
    ENTRY->next = NULL;

    //if need flush
    //ENTRY->doFlush=1;



    /*next!*/
    Q->head->next = ENTRY;
    Q->head = ENTRY;
    //Q->cnt_in+=1;

    pthread_spin_unlock(&LOG->insert_lock);

    //if need flush wait for writeThread
    /*while(1){
        if(ENTRY->doFlush==0)
            break;
    }*/

}

/**
 * Log format string with limited number of params, but doing strdup() on %s. Only static format string.
 * @param logFile
 * @param format
 * @param ...
 */
void oroLogRelaxed(Poro_t logFile, size_t NUM, const char* format, ...) {

    struct timespec time = {0};


    if (time_source >= 0)
        clock_gettime(time_source, &time);

    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;

    va_list arglist;

    pthread_spin_lock(&LOG->insert_lock);

    if (UNLIKELY(LOG->insert_forbidden == 1)) {
        va_start(arglist, format);
        logRedirect_stderr(logFile, __func__, time, format, arglist);
        va_end(arglist);
        return;
    }


    Q_element *ENTRY;
    LOG_QUEUE *Q = &(LOG->QUEUE);



    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //locking mem after start is dangerous!!! 
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;
    Q->return_tail = Q->return_tail->next;



    //а нада?
    //assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");



    va_start(arglist, format); //LAST PARAM!!!

    uintptr_t *p = ENTRY->params;

    ENTRY->params_free_mask = (uintptr_t) 0;

    const char *string = format;
    int num = 0;
    __builtin_prefetch(stop_tb);
    while ((string = strchr(string, '%')) != NULL) {


        ++string;
        for (;;) {
            switch (UNLIKELY(*string > 'x') ? 0 : stop_tb[(int) *string]) {
                case 0://any
                {
                    ++string;
                    continue;
                }
                case 1://'i' 'u' 'd' 'a' ...
                {
                    p[num] = va_arg(arglist, uintptr_t);
                    ++num;
                    goto next;
                }
                case 2: //'s'
                {
                    p[num] = (uintptr_t) strdup(va_arg(arglist, char *));
                    ENTRY->params_free_mask |= 1 << num;
                    ++num;
                    goto next;

                }
                case 3: //'%'
                {
                    goto next;
                }
                case 4: //'*'
                {
                    p[num] = va_arg(arglist, uintptr_t);
                    ++num;
                    ++string;
                    continue;
                }
                case 5: //'\0'
                {
                    format = "(bad format)";
                    goto exit_parsing;

                }
                default:
                {
                    ++string;
                    continue;
                }
            }

        }//search for end



next:
        ++string;
    }//outer while

exit_parsing:

    va_end(arglist);

    ENTRY->format_string = format;
    ENTRY->num = num; //количество либо 0 либо больше нуля


    assert_msg(num == NUM, "oroLogRelaxed: NUM and number of format params differs");

    ENTRY->realtime = time;
    ENTRY->next = NULL;

    //    Q_NEXT_FENCE;



    Q->head->next = ENTRY;
    Q->head = ENTRY;
    //Q->cnt_in+=1;

    pthread_spin_unlock(&LOG->insert_lock);


}




/// Same as oroLogRelaxed but made with jump table
/// \param logFile
/// \param NUM
/// \param format
/// \param ...

void oroLogRelaxed_X(Poro_t logFile, size_t NUM, const char* format, ...) {


    static const void *const step0_jumps[] = {
        &&case_any_char, &&case_copyable_format, &&case_dup_format, &&case_percent_symbol, &&case_variable_width_format, &&case_end_of_string,
    };


    struct timespec time = {0};


    if (time_source >= 0)
        clock_gettime(time_source, &time);

    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;

    va_list arglist;

    pthread_spin_lock(&LOG->insert_lock);

    if (UNLIKELY(LOG->insert_forbidden == 1)) {
        va_start(arglist, format);
        logRedirect_stderr(logFile, __func__, time, format, arglist);
        va_end(arglist);
        return;
    }


    Q_element *ENTRY;
    LOG_QUEUE *Q = &(LOG->QUEUE);



    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //lock after start is dangerous!!! 
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;
    Q->return_tail = Q->return_tail->next;





    va_start(arglist, format); //LAST PARAM!!!

    uintptr_t *p = ENTRY->params;

    ENTRY->params_free_mask = (uintptr_t) 0;

    const char *string = format;
    int num = 0;
    __builtin_prefetch(stop_tb);
    while ((string = strchr(string, '%')) != NULL) {


        ++string;
        for (;;) {

            if (*string > 'x')
                goto case_any_char;
            goto *(step0_jumps [ stop_tb[(int) *string]]);



case_any_char://any
            {
                ++string;
                continue;}
case_copyable_format://'i' 'u' 'd' 'a' ...
            {
                p[num] = va_arg(arglist, uintptr_t);
                ++num;
                goto next;}
case_dup_format: //'s'
            {
                p[num] = (uintptr_t) strdup(va_arg(arglist, char *));
                ENTRY->params_free_mask |= 1 << num;
                ++num;
                goto next;}
case_percent_symbol: //'%'
            {
                goto next;}
case_variable_width_format:
            {
                p[num] = va_arg(arglist, uintptr_t);
                ++num;
                ++string;
                continue;}
case_end_of_string: //'\0'
            {
                format = "(bad format)";
                goto exit_parsing;}
        }//search for end



next:
        ++string;
    }//outer while

exit_parsing:

    va_end(arglist);

    ENTRY->format_string = format;
    ENTRY->num = num; //количество либо 0 либо больше нуля


    assert_msg(num == NUM, "oroLogRelaxed_X: NUM and number of format params differs");

    ENTRY->realtime = time;
    ENTRY->next = NULL;

    //    Q_NEXT_FENCE;



    Q->head->next = ENTRY;
    Q->head = ENTRY;


    pthread_spin_unlock(&LOG->insert_lock);


}

/**
 * Log by format string with unlimited number of parameters. Buffer preallocated and expanding as needed. No static or copyble restrictions. vsnprintf() inside. Slow.
 * @param logFile poinert to log
 * @param expecting_byte preallocate buffer
 * @param format
 * @param ...
 * @return lenght of buffer. may by ge expecting_byte
 */
size_t oroLogFull(Poro_t logFile, size_t expecting_byte, const char* format, ...) {


    /*
     * pagesize allocation!!!!????
     */

    struct timespec time = {0};
    if (time_source >= 0)
        clock_gettime(time_source, &time);

    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;



    va_list arglist;

    pthread_spin_lock(&LOG->insert_lock);

    if (UNLIKELY(LOG->insert_forbidden == 1)) {
        va_start(arglist, format);
        logRedirect_stderr(logFile, __func__, time, format, arglist);
        va_end(arglist);
        return expecting_byte;
    }

    Q_element *ENTRY;
    LOG_QUEUE *Q;
    Q = &(LOG->QUEUE);


    /*if(LOG->withlock) */

    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //lock after start is dangerous!!! 
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;
    Q->return_tail = Q->return_tail->next;



    //а нада?
    //assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");


    int n = 0;
    /*
     * round mem to pagesize?
     */
    if (ENTRY->full_string == NULL) {
        fprintf(stderr, "oroLogFull: preallog full_string\n");
        ENTRY->full_string = aligned_alloc(sizeof (uintptr_t), expecting_byte);
        ENTRY->full_string_len = expecting_byte;

    } else if (ENTRY->full_string_len < expecting_byte) {
        fprintf(stderr, "oroLogFull: expand full_string by expecting_byte\n");
        char *t = aligned_alloc(sizeof (uintptr_t), expecting_byte);
        assert(t != NULL);
        free(ENTRY->full_string);
        ENTRY->full_string = t;
        ENTRY->full_string_len = expecting_byte;
    }


    do {
        va_start(arglist, format); //LAST PARAM!!!

        n = vsnprintf(ENTRY->full_string, ENTRY->full_string_len, format, arglist);

        va_end(arglist);
        if (n >= ENTRY->full_string_len) {
            fprintf(stderr, "oroLogFull: expand full_string by ret val of vsnprintf\n");
            size_t need = n - ENTRY->full_string_len;
            /*
             * round mem to pagesize?
             */
            size_t new_size = (((ENTRY->full_string_len + need + 1) / sizeof (uintptr_t)) + 1) * sizeof (uintptr_t);
            char *t = aligned_alloc(sizeof (uintptr_t), new_size);
            assert_msg(t != NULL, "oroLogFull buf expand aligned_alloc failed");
            free(ENTRY->full_string);
            ENTRY->full_string = t;
            ENTRY->full_string_len = new_size;
            expecting_byte = new_size;
        } else {
            break;
        }

    } while (1);

    assert_msg(n > 0, "negative n means something wrong with vsnprintf");


    ENTRY->format_string = NULL;

    ENTRY->num = -1;

    ENTRY->params_free_mask = (uintptr_t) 0;

    ENTRY->realtime = time;

    ENTRY->next = NULL;

    //    Q_NEXT_FENCE;


    Q->head->next = ENTRY;
    Q->head = ENTRY;
    //Q->cnt_in+=1;


    pthread_spin_unlock(&LOG->insert_lock);

    return expecting_byte;
}

/// parce string with config params (delimetr is ;) and return struct. Caution! strdup on path!!! need free
/// \param params string like 'pid=1;truncate=1;dufsize=4098;...'
/// \return 
oro_attrs_t config_from_str(const char *params){
    /*
     * file_name_with_pid
     * truncate
     * do_mlock

     * path
     * return_q
     * bufsize
     * bind_cpu
     */
    oro_attrs_t config = {0};
    
    if(strstr(params,"pid=1")!=NULL)
        config.file_name_with_pid=1;
    if(strstr(params,"truncate=1")!=NULL)
        config.truncate=1;
    if(strstr(params,"do_mlock=1")!=NULL)
        config.do_mlock=1;
    
    {
        //return_q
        char *param = "return_q=";
        char *p = strstr(params, param);
        if (p) {
            p += strlen(param);
            config.return_q = atoi(p);
        }
    }
    {
        //bufsize
        char *param = "bufsize=";
        char *p = strstr(params, param);
        if (p) {
            p += strlen(param);
            config.bufsize = atoi(p);
        }
    }
    {
        //bufsize
        char *param = "bind_cpu=";
        char *p = strstr(params, param);
        if (p) {
            p += strlen(param);
            config.bind_cpu = atoi(p);
        }
    }
    {
        //pah
        char *param = "path=";
        char *p = strstr(params, param);
        if (p) {
            p += strlen(param);
            char *partial = strdupa(p);
            char *end = strchr(partial,';');
            if(end){
                *end='\0';
            }
            config.path=strdup(partial);
            
        }
    }
    
    return config;
}
/**
 * simplified version of strscpy from Linux kernel source
 * @param dest
 * @param src
 * @param sizeof_dest for sizeof(dest)
 * @return coppyed num of chars or -1 if cropped
 */
static int str_s_cpy(char *dest, const char *src, size_t sizeof_dest) {

    int res = 0;

    while (sizeof_dest) {
        char c;
        c = src[res];
        dest[res] = c;
        if (!c)
            return res; //copyed number of chars _not_ include NUL
        res++;
        sizeof_dest--;
    }

    /* Hit buffer length without finding a NUL; force NUL-termination. */
    if (res)
        dest[res - 1] = '\0';
    return -1;
}

void oroLogfprintf(Poro_t logFile, FILE *File, const char* format, ...) {
    va_list arglist;
    struct timespec time = {0};
    //time_source is global
    if (time_source >= 0)
        clock_gettime(time_source, &time);

    __builtin_prefetch((struct FD_LIST_ITEM *) logFile);
    struct FD_LIST_ITEM *LOG = (struct FD_LIST_ITEM *) logFile;


    //Do we need sanity check?
    //assert_msg(LOG->withlock == 0, "Use oroLogFixed5_unlocked for LOG expecting locks");


    if (UNLIKELY(LOG->insert_forbidden == 1)) {

        va_start(arglist, format);
        logRedirect_stderr(logFile, __func__, time, format, arglist);
        va_end(arglist);

        return;
    }



    Q_element *ENTRY;
    LOG_QUEUE *Q = &(LOG->QUEUE);

    //add some to Q if it is empty
    if (UNLIKELY(Q->return_tail->next == NULL)) {
        //mLOCK after start is dangeruus!!!!
        Q->return_tail = alloc_Q_list_inside_page(Q->return_tail, -1);
        //fprintf(stderr,"RETURN Q EMPTY\n");
    }
    //not checking. Under lock it does not metter
    //alloc_Q_list_inside_page does abort on problem with memory
    ENTRY = Q->return_tail;


    va_start(arglist, format);
    vfprintf(File, format, arglist);
    va_end(arglist);

    ENTRY = NULL;
}