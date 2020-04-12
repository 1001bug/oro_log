/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

//#include <bits/string2.h>

//#include <bits/stdio.h>





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


#ifndef NOOPTY
#define UNLIKELY __glibc_unlikely
#define   LIKELY __glibc_likely
#else
#define UNLIKELY  
#define   LIKELY 
#endif

# define assert_msg(expr,msg)    \
  ((   LIKELY(expr)   )                        \
   ? __ASSERT_VOID_CAST (0)      \
   : __assert_fail (__STRING(msg) ": "    __STRING(expr), __FILE__, __LINE__, __ASSERT_FUNCTION))

 #define RDTSCP(V) __asm__ __volatile__("rdtscp;shl $32, %%rdx;or %%rdx, %%rax":"=a"(V):: "%rcx", "%rdx")

 

//подобного вида печатает assert (__func__ vs __FUNCTION__ vs __PRETTY_FUNCTION__ vs )
#define ERR_MSG_HEADER "%s: %s:%u: %s: "
#define ERR_MSG_PARAMS ,__progname,__FILE__,__LINE__,__func__
//fprintf(stderr,ERR_MSG_HEADER "clock_gettime FAILed: %s\n" ERR_MSG_PARAMS,strerror(errno));
//make a format string with params to use with any forma variadic var fun
#define ERR_MESSAGE(format,...)    ERR_MSG_HEADER format  ERR_MSG_PARAMS , ## __VA_ARGS__
//fprintf(stderr, ERR_MESSAGE("val i=%i k=%iand it is not good\n",a,b)  );
//fprintf(stderr, ERR_MESSAGE("val is not good\n")  );



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

//variadic param length
#define PARAMS_FIXED_NUM 16

//return queue from logFlush to logLog and def size
#define RETURN
#define RETURN_QUANTITY 2000

//hardcode for now. it is usual to sleep for 10 millisec
static __suseconds_t logFlush_empty_queue_sleep = 10000; //sleep while autoflush
static const int logFlush_fflush_sec = 1; //+sec to next fflush time
static const long logFlush_lines_at_cycle=20000; //divide by nuber of log files
static long timezone_offset;
//static unsigned long long cycles_in_one_usec = 1000000L; //защита от деления на ноль
static struct timespec logOpenTime;
//static volatile int endlessCycle = 1;
static const size_t logLogFullObjBufLen = 2048;
static int time_source = CLOCK_ID_WALL_FAST; //global. same for all logs



//static const char stop_tb[120]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,1,1,1,1,1,0,1,0,0,0,0,1,1,1,0,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//static const char stop_tb[255]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,1,1,1,1,1,0,1,0,0,0,0,1,1,1,0,0,1,0,1,0,0,1};
/*static const char stop_tb[255] __attribute__ ((aligned(sizeof(uintptr_t)))) = 
{   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,1,0,0,0,0,0,0,0,
    0,1,0,1,1,1,1,1,0,1,0,0,0,0,1,1,1,0,0,2,0,1,0,0,1} ;*/

static const uint8_t stop_tb[128] __attribute__ ((aligned(sizeof(uintptr_t)))) =
  {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,
                  /* '%' */  3,            0, /* '\''*/  0,
	       0,            0, /* '*' */  0, /* '+' */  0,
	       0, /* '-' */  0, /* '.' */  0,            0,
    /* '0' */  0, /* '1' */  0, /* '2' */  0, /* '3' */  0,
    /* '4' */  0, /* '5' */  0, /* '6' */  0, /* '7' */  0,
    /* '8' */  0, /* '9' */  0,            0,            0,
	       0,            0,            0,            0,
	       0, /* 'A' */  1,            0, /* 'C' */  1,
	       0, /* 'E' */  1, /* F */    1, /* 'G' */  1,
	       0, /* 'I' */  0,            0,            0,
    /* 'L' */  0,            0,            0,            0,
	       0,            0,            0, /* 'S' */  2,
	       0,            0,            0,            0,
    /* 'X' */  1,            0, /* 'Z' */  0,            0,
	       0,            0,            0,            0,
	       0, /* 'a' */  1,            0, /* 'c' */  1,
    /* 'd' */  1, /* 'e' */  1, /* 'f' */  1, /* 'g' */  1,
    /* 'h' */  0, /* 'i' */  1, /* 'j' */  0,            0,
    /* 'l' */  0, /* 'm' */  1, /* 'n' */  1, /* 'o' */  1,
    /* 'p' */  1, /* 'q' */  0,            0, /* 's' */  2,
    /* 't' */  0, /* 'u' */  1,            0,            0,
    /* 'x' */  1,
    0,0,0,0,0
  };

//compact 64 byte dic. 4 symbol types in 1 byte: {0..3, 0..3, 0..3, 0..3}
static uint8_t stop_tb_Q[256/4] __attribute__ ((aligned(sizeof(uintptr_t)))) = {0};

//inernal use only, degerous!
//static int logCleaup();
static int logCleaup(void (*error_fun)(char *fstring,...));
static void logRedirect_stderr(int logFile, const char *funname, struct timespec time, const char* format, va_list ap);
//пока тут. куда дальше ее сунуть не знаю
static int str_s_cpy(char *dest, const char *src, size_t sizeof_dest);
static void prepare_Q();
//size_t const cache_line_size = 64; sysconf(_SC_LEVEL3_CACHE_LINESIZE)

static volatile int requestForLogsTruncate = 0;


typedef struct Q_element{
    struct timespec realtime;
    struct timespec monotime;
    const char *format_string;
    uintptr_t params[PARAMS_FIXED_NUM];
    uintptr_t params_free_mask;
    int64_t num;
    //pthread_t tid __attribute__ ((aligned(sizeof(uintptr_t)))); //не знаю, это выравнивать надо... должен быть 8 байт
    
    struct Q_element *next;
    
    
} Q_element __attribute__ ((aligned(sizeof(uintptr_t))));


typedef struct LOG_Q {
    //сгруппировать поля по пользователям
    //встатвить разрыв в cacheline (64 байта?)

    uint64_t cnt_in __attribute__ ((aligned));
    Q_element *head __attribute__ ((aligned));
    Q_element *return_tail __attribute__ ((aligned));
    pthread_spinlock_t insert_lock __attribute__ ((aligned));
    
    //sysconf(_SC_LEVEL3_CACHE_LINESIZE) = 64
    char cache_line_pad1 [64 - sizeof(uint64_t) - sizeof(Q_element *) - sizeof(Q_element *) - sizeof(pthread_spinlock_t)]; 
    //char cache_line_pad1 [64 ];
    
    uint64_t cnt_out __attribute__ ((aligned));
    Q_element *tail __attribute__ ((aligned));
    Q_element *return_head __attribute__ ((aligned));
    
    
    
    
} LOG_QUEUE __attribute__ ((aligned));

//для целей autoflush
struct FD_LIST_ITEM{
    FILE *FD;
    LOG_QUEUE QUEUE  __attribute__ ((aligned(sizeof(uintptr_t))));;
    struct FD_LIST_ITEM *next;
    char *custom_io_buf;
    
    volatile int insert_forbidden __attribute__ ((aligned(sizeof(uintptr_t))));
    char log_filename[LOG_F_NAME_LEN];
    
} __attribute__ ((aligned(sizeof(uintptr_t)))); 

static struct FD_LIST_ITEM *FD_ARR = NULL;
static volatile int FD_LIST_ITEM_NUM = 0;
//lock FD_ARR. Block logOpen after log flusher start
static pthread_mutex_t FD_ARR_mutex = PTHREAD_MUTEX_INITIALIZER;


static pthread_t af_thread = 0;
static pthread_mutex_t af_thread_startup_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile long logFlushThreadStarted = 0;









void logSetRequestForLogsTruncate(){
    requestForLogsTruncate=1;
    return;
}

/*
 * функция гадит в STDERR!!! в двух местах
 */
static void * logFlush_thread(void *S){
    //__suseconds_t usec_sleep = *(__suseconds_t *) S;
    __suseconds_t usec_sleep = logFlush_empty_queue_sleep ; //GLOBAL
    
    
    void (*error_fun)(char *fstring,...) = S;
    
    char DATE_FORMAT_STRING[] __attribute__ ((aligned(sizeof(uintptr_t))))=      "00:00:00.000000000;;";
    const char date_format_string[] __attribute__ ((aligned(sizeof(uintptr_t))))="%02i:%02i:%02i.%%09li;;";
    //                                                         ; TID 139837019477824
    //char TID[] __attribute__ ((aligned(sizeof(uintptr_t)))) = "; TID 18446744073709551615\n";
    


    //struct timespec prevAlert = {.tv_nsec = 0, .tv_sec = 0};
    struct timespec prevFlush = {.tv_nsec = 0, .tv_sec = 0};
    
    
    //forbiden add new log files from thist point
    
    pthread_mutex_lock(&FD_ARR_mutex);


    if (FD_ARR == NULL || FD_LIST_ITEM_NUM == 0) {

        //go to exit
        pthread_mutex_unlock(&af_thread_startup_mutex);
        goto logFlush_thread__exit;
    }
    
    
    //activate all logs
    for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
        FD_ARR[N].insert_forbidden = 0;
    }
    
    logFlushThreadStarted=1;
    pthread_mutex_unlock(&af_thread_startup_mutex);
    
    //ready to exit counter (+3 time get state: write=0 and all files are locked)
    int ready_to_exit = 0;
    
    //has total limit of writes. divide by num of files
    const long logFlush_lines_each_file = logFlush_lines_at_cycle / FD_LIST_ITEM_NUM;
    
    for(;;) {

        

        if (UNLIKELY(requestForLogsTruncate)) {
            requestForLogsTruncate = 0;
            for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
                FILE *F = FD_ARR[N].FD;

                if (F) {
                    //такойспособ не обнуляет блокировку файла
                    errno = 0;
                    if (fseek(F, 0, SEEK_SET) != 0) {
                        if(error_fun)
                            error_fun(ERR_MESSAGE("Log Truncate failed (rewind): %s\n", strerror(errno)));
                        FD_ARR[N].insert_forbidden = 1;
                    } else {
                        errno = 0;
                        if (ftruncate(fileno(F), 0) != 0) {
                            if(error_fun)
                                error_fun(ERR_MESSAGE("Log Truncate failed (trunc): %s\n", strerror(errno)));
                            
                            FD_ARR[N].insert_forbidden = 1;
                        }
                    }
                }

            }
        }

        //счетчик того сколько всесего записей сделано по всем файлам. если ни одной, то значит ничего не происходит, поспим немного
        long write_cnt = 0;
        int insert_forbidden_FD = 0;

        __time_t prev_sec=0;
        int hr=0;
        int min=0;
        int sec=0;
        
        for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
            FILE *F = FD_ARR[N].FD;
            
            if(FD_ARR[N].insert_forbidden)
                insert_forbidden_FD+=1;

            //обратное условие проверим через три строки. там его надо каждый раз проверять
            
            LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);

            //счетчик чтобы в каждый файл писать по немного
            long write_limit_cnt = 0;
            //Если актуальный и не пустой  и за раз не больше 10000
            
            
            //по списку одно условие - есть следующий эллемент
            while ( LIKELY(F != NULL) 
                    && 
                    Q->tail->next != NULL
                    &&
                    write_limit_cnt < logFlush_lines_each_file) {



                
                /*ПЕЧАТЬ!!*/

                //volatile 
                Q_element *E = Q->tail->next;
                
                //clock_gettime(CLOCK_ID_WALL_FAST,&(E->realtime));

                //запомнить предыдущую секунду и не высчитывать hr min sec заново!!!
                
                if(UNLIKELY(prev_sec != E->realtime.tv_sec)){
                    
                    prev_sec = E->realtime.tv_sec;
                            
                    sec = (prev_sec + timezone_offset) % 86400; //секунд в последних судках. 24*60*60=86400
                    
                    min = sec / 60;
                    
                    sec = sec % 60;
                    
                    hr = min / 60;
                    
                    min = min % 60;
                    
                    
                    
                    int s = snprintf(
                            DATE_FORMAT_STRING
                            , sizeof (DATE_FORMAT_STRING),
                            date_format_string  //"%02i:%02i:%02i.%09li;;"
                            
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
                if(E->format_string == NULL) 
                    E->format_string="(null)";



                //fputs_unlocked(DATE_FORMAT_STRING,F);
                //вписать наносекунды
                if (time_source >= 0)
                    fprintf(F, DATE_FORMAT_STRING, E->realtime.tv_nsec);


                
                
                switch (E->num) {

                    
                    case -1: //E->num == -1 means logLogFull and we need free on format_string
                    {
                        fputs_unlocked(E->format_string, F);
                        free((void *) E->format_string);
                        E->format_string = NULL;
                        break;

                    }
                        
                    case 0: //print static string wo any params
                    {
                        fputs_unlocked(E->format_string, F);

                        E->format_string = NULL;
                        break;
                    }

                    default: // print w params
                    {
                        uintptr_t *p = E->params;

                        //COMPILLE TIME limit on params number
                        _Static_assert(PARAMS_FIXED_NUM < 25, "HARCODED MAXIMUM FOR PARAMS_FIXED_NUM IS 24");


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

                                );

                        //free some params if needed
                        if (E->params_free_mask) {

                            uintptr_t f = E->params_free_mask;

                            for (int i = 0; i < PARAMS_FIXED_NUM; i++) {
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
                    
                
                fputs_unlocked("\n",F);
                
                //check io errors on every second fflush()
                
                
                write_limit_cnt++;
                write_cnt++;

                
#ifndef RETURN
                free(Q->tail);
#else                
                //нужно ли делать memset(0)?? можно потом не делать. или нельзя?
                Q->tail->next=NULL;
                Q->return_head->next=Q->tail;
                Q->return_head=Q->tail;
#endif                
                Q->tail = E;
                Q->cnt_out+=1;

            }//while
        }//for N
        
        if(insert_forbidden_FD == FD_LIST_ITEM_NUM && write_cnt == 0){
            //может какой счетчик добавить? с 3-го раза на выход?
            ready_to_exit+=1;
            if(ready_to_exit>1)
                goto logFlush_thread__exit;
            //возможно комбо при котором набо обнлить ready_to_exit?
        }
        
        //прошлись по всем файлам но ни одной записи? спать!
        if (write_cnt == 0) {
            
            //for (int N = 0; N < FD_LIST_ITEM_NUM; N++)                fputs_unlocked("#sleep\n",FD_ARR[N].FD);
            if(logFlush_empty_queue_sleep){
            //замер времени 
            pthread_yield();
            //замер времени 
            //если прошло слишком мало, то SLEEP 
            //можно вычесть проведенное время в pthread_yield из usec_sleep
            
            
            struct timeval sleep_timeout={0,usec_sleep};
            
            select(0,NULL,NULL,NULL,&sleep_timeout);
            }
            

        } else
            write_cnt = 0;

        //FLUSH every SECOND
        struct timespec Flush = {0};
        errno = 0;
        if (clock_gettime(CLOCK_ID_MEASURE_FAST, &Flush) < 0) {
            if(error_fun)
                error_fun(ERR_MESSAGE("clock_gettime FAILed: %s\n", strerror(errno)));
            
        }
        if (Flush.tv_sec > prevFlush.tv_sec) {           //HARDCODE flsuh every 1 sec
            for (int N = 0; N < FD_LIST_ITEM_NUM; N++)
                if (FD_ARR[N].FD != NULL){
                    errno=0;
                    fputs_unlocked("#fflush\n",FD_ARR[N].FD);
                    
                    fflush_unlocked(FD_ARR[N].FD);
                    if(errno)
                    {
                        if(error_fun)
                            error_fun(
                                    ERR_MESSAGE("fflush on %s: %s\n",FD_ARR[N].log_filename,strerror(errno))
                                    );
                    }
                }
            prevFlush.tv_sec = Flush.tv_sec + logFlush_fflush_sec;
        }
        
        
        //*/
    }//endlessCycle



    //the only return point!!!
logFlush_thread__exit:
    logFlushThreadStarted=0;
    pthread_mutex_unlock(&FD_ARR_mutex);
    
    return NULL;
}

int logFlushStart(int bind_cpu, void (*error_fun)(char *fstring, ...)) {
    pthread_attr_t af_pth_attrs_r;
    
    prepare_Q();
    
    if(logFlushThreadStarted){
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
        if ((r=pthread_attr_setaffinity_np(&af_pth_attrs_r, sizeof (cpu_set_t), &cpus_af)) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("affinity to CPU %i pthread_attr_setaffinity_np FAILed: %s\n",bind_cpu, strerror(r))
                    );
            return -1;
        } else {
            if (error_fun)error_fun(
                    ERR_MESSAGE("affinity to CPU %i\n", bind_cpu)
                    );
        }
#endif
    }//bind cpu
    else{
        if (error_fun)error_fun(
                    ERR_MESSAGE("affinity to CPU %i not possible\n",bind_cpu)
                    );
    }

    int af_thread_startup_mutex_lock = pthread_mutex_lock(&af_thread_startup_mutex);
    assert(af_thread_startup_mutex_lock == 0);
    //&logFlush_empty_queue_sleep
    int r = 0;
    if ((r=pthread_create(&af_thread, &af_pth_attrs_r, logFlush_thread, error_fun)) != 0) {
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
    
    if(logFlushThreadStarted != 1){
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
        if ((r = pthread_setname_np(af_thread, "logFlush")) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("pthread_setname_np for logAutoflush_thread error %i: %s\n", r, strerror(r))
                    );
            //continue, not fatal
        }
    }
    
    return 0;

}

int logFlushStop(__suseconds_t wait, void (*error_fun)(char *fstring,...)){

    //check for logFlushThreadStarted=1 ????


    


        for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
            FD_ARR[N].insert_forbidden = 1;
        }



        while (pthread_mutex_trylock(&FD_ARR_mutex) != 0) {

            if (wait != 0) {
                //если задана пауза - ждать один раз и обнулить ее. на след. круг сразу выйдем с -1
                select(0, NULL, NULL, NULL, &((struct timeval){0, wait}));
                wait = 0;
            } else //logFlush still working
                return -1;

        }

        //logFlush thread already finish (on exit it should unlock FD_ARR_mutex)

        //join наверно лишне, но вдруг там какая-то чистка делается, надо уточнить
        int r=0;
        if ((r=pthread_join(af_thread, NULL)) != 0) {
            if (error_fun)error_fun(
                    ERR_MESSAGE("pthread_join to logFlush_thread error %i: %s\n",r, strerror(r))
                    );
            //continue, not fatal
            //may be not statrted
        }
        
        //Close all FD, free Q, io_buf, ...
        logCleaup(error_fun);
        
        
        pthread_mutex_unlock(&FD_ARR_mutex);
        

        return 0;






        //pthread_cancel(af_thread);
        //fprintf(stderr,"autoflush stoped\n");

    /* else {
        if (error_fun)error_fun("FILE %s, LINE %i, logAutoflush Flush thread discriptor problem!\n", __FILE__, __LINE__);
        return -1;
    }*/
    
}

//CPU AFFINITY!!!!!!!
//start_stop = 'R'un 'S'top
/*int logFlush(int cmd,__suseconds_t wait, char *cpu_list, void (*error_fun)(char *fstring,...)){ //передать сюда атрибуты для прибивки по ядрам. но как?????

    
    static int started = 0;
    
    assert_msg(cmd == 'R' || cmd == 'S', "Wrong cmd: 'R'un or 'S'top");

    //start!
    if(cmd == 'R' && started == 0){
        
        if(pthread_attr_init(&af_pth_attrs_r) != 0){
            if(error_fun)error_fun("FILE %s, LINE %i, pthread_attr_init FAILed\n",__FILE__,__LINE__);
            return -1;
        }
        
        if(strlen(cpu_list)>0 && setCPUaffinity(&af_pth_attrs_r,cpu_list,error_fun) != 0){
            if(error_fun)error_fun("FILE %s, LINE %i, logAutoflush setCPUaffinity FAILed\n",__FILE__,__LINE__);
            //continue, not fatal
        }
        
        //global
        logFlush_empty_queue_wait = wait > 0 && wait < 900000? wait : logFlush_empty_queue_wait;
        errno=0;
        if (pthread_create(&af_thread, &af_pth_attrs_r, logFlush_thread, &logFlush_empty_queue_wait) != 0) {
                if(error_fun)error_fun("FILE %s, LINE %i, pthread_create logAutoflush_thread error %i: %s\n",__FILE__,__LINE__,errno,strerror(errno));
                return -1;
                //FATAL!
        }
        {
            int s = pthread_attr_destroy(&af_pth_attrs_r);
            if (s != 0)
                if(error_fun)error_fun("FILE %s, LINE %i, logAutoflush: pthread_attr_destroy: %i",s);
        }
        errno = 0;
        if(pthread_setname_np(af_thread, "logAutoflush") != 0){
            if(error_fun)error_fun("FILE %s, LINE %i, pthread_setname_np logAutoflush_thread error %i: %s\n",__FILE__,__LINE__,errno,strerror(errno));
            //continue, not fatal
        }
        
        started = 1;
        return 0;
        
    }
    //STOP!!
    else if(cmd == 'S' && started == 1){
        
        if(af_thread){
            
            
            for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
                FD_ARR[N].insert_forbidden = 1;
            }
            
            
            
            while (pthread_mutex_trylock(&FD_ARR_mutex) != 0) {

                if (wait != 0) {
                    //если задана пауза - ждать один раз и обнулить ее. на след. круг сразу выйдем с -1
                    select(0, NULL, NULL, NULL, &((struct timeval){0, wait}));
                    wait = 0;
                } else   //logFlush still working
                    return -1;

            }
            
            //logFlush thread already finish (on exit it should unlock FD_ARR_mutex)
            
            //join наверно лишне, но вдруг там какая-то чистка делается, надо уточнить
            errno = 0;
            if (pthread_join(af_thread, NULL) != 0) {
                if (error_fun)error_fun("FILE %s, LINE %i, pthread_join logAutoflush_thread error %i: %s\n", __FILE__, __LINE__, errno, strerror(errno));
                //continue, not fatal
            }
            {
                //Close all FD, free Q, io_buf, ...
                int r = logCleaup();
                if (error_fun)error_fun("FILE %s, LINE %i, logCleaup: %i\n", __FILE__, __LINE__, r);
            }
            
            return 0;

            
            
            
            
            
            //pthread_cancel(af_thread);
            //fprintf(stderr,"autoflush stoped\n");
            
        }else{
            if(error_fun)error_fun("FILE %s, LINE %i, logAutoflush Flush thread discriptor problem!\n",__FILE__,__LINE__);
            return -1;
        }
    }
    
    if(error_fun)error_fun("FILE %s, LINE %i, logAutoflush restarting or restopping!\n",__FILE__,__LINE__);
    return -1;
}*/

unsigned long logQueueSize(int logFile, void (*error_fun)(char *fstring,...)){
    int N=logFile-1;
    
    if(N <0 || logFile > FD_LIST_ITEM_NUM){
        if(error_fun)error_fun(
                ERR_MESSAGE("wrong logFile: %i\n",logFile)
                );
        return -1;
    }
    
    
    
    return (FD_ARR[N].QUEUE.head == FD_ARR[N].QUEUE.tail) ? (0) : (FD_ARR[N].QUEUE.cnt_in-FD_ARR[N].QUEUE.cnt_out);
    //return    (FD_ARR[N].QUEUE.cnt_in-FD_ARR[N].QUEUE.cnt_out);
    
}


int logCleaup(void (*error_fun)(char *fstring,...)){ //shuold be called ONLY from logFlush (STOP)
    
    
    /*if (FD_LIST_ITEM_NUM == 0) {
        fprintf(stderr,"FD_LIST_ITEM_NUM=0\n");
            return 0;
        }*/
    
    
    for (int N = 0; N < FD_LIST_ITEM_NUM; N++) {
            //FILE *F = FD_ARR[N].FD;
            LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);
            
            //мы не должны попадать сюда с полной очередью
            assert_msg(Q->head==Q->head, "Cleanup non empty Queue");
            
            if(FD_ARR[N].FD){
            fprintf(FD_ARR[N].FD, "#Closed %s.\n",FD_ARR[N].QUEUE.cnt_in==FD_ARR[N].QUEUE.cnt_out?"normally":" truncated. IN<>OUT"   );
            
            errno=0;
            if(fflush(FD_ARR[N].FD) != 0){
                if(error_fun)
                    error_fun(ERR_MESSAGE("fflush on %s: %s\n",FD_ARR[N].log_filename,strerror(errno))
                            );
            }
            errno=0;
            if(fclose(FD_ARR[N].FD) != 0){
                if(error_fun)
                    error_fun(ERR_MESSAGE("fclose on %s: %s\n",FD_ARR[N].log_filename,strerror(errno))
                            );
            }
            FD_ARR[N].FD = NULL;
            
            
            }
            
            //лог должен быть уже закрыт
            //assert_msg(FD_ARR[N].FD == NULL, "Cleanup non closed FD");
            
            //free setvbuf allocation
            if(FD_ARR[N].custom_io_buf){
                free(FD_ARR[N].custom_io_buf);
                FD_ARR[N].custom_io_buf=NULL;
            }
            
            //основная очередь
            //должен был остаться толкьо один элемент, возможно чтоит проверить!!!!!
            free(Q->head);
            Q->head=NULL;

#ifdef RETURN
            //если у нас была обратная очередь, то съесть ее с конца
            if(Q->return_tail){
                while(Q->return_tail->next){
                    //fprintf(stderr,"S %p n-> %p\n",Q->return_tail,Q->return_tail->next);
                Q_element *t = Q->return_tail->next;
                free(Q->return_tail);
                Q->return_tail = t;
                //fprintf(stderr,"E %p n-> %p\n\n",Q->return_tail,Q->return_tail->next);
                }
                free(Q->return_tail);
                Q->return_tail=NULL;
            }
            
#endif            
            
    }
    
    //race!!! if logLog* run same time
    FD_LIST_ITEM_NUM=0;
    if (FD_ARR) {
        free(FD_ARR);
        FD_ARR = NULL;
    }
    
    
    
    
    
    //fprintf(stderr,"logCleaup\n");
    
    return 0;
    
    
}


int logOpen(int *logFile,log_attrs config, void (*error_fun)(char *fstring,...)){
    
    
    //сделать мьютекс !!! чтобы не запустить LogOpen из двух мест сразу
    
    //и переделать на список!!!
    
    //нулевой эллемент пропустим. начинаем с 1-го
    //static last_FD_LIST_ITEM_NUM=0;
    
    //повторно создавать лог файл неправильно!
    assert(logFile != NULL);
    assert_msg(*logFile == 0,"Attempt to re-open log?");
    
    assert_msg(config.f_name_part1 !=NULL && strlen(config.f_name_part1)>0,"First part of file name MUST not be zero length");

    
    if(config.return_q == 0)
        config.return_q = RETURN_QUANTITY;
    
    
    
    //переделать бы на список. тога можно добавлять не зависимо от того запущен FLUSH тред или нет. 
    //а так realoc всё сломает
    //мьютекс!!! ? а в одно ли место эти треды придут? надо проверить
    //первый логфайл
    //int mtx = pthread_mutex_lock(&FD_ARR_mutex);
    
    //если запущен тред записи на диск, не блокировать вызывающиц тред, а вернуть ошибку
    //как быть с возможно параллельным запуском LogOpen?
    int mtx = pthread_mutex_trylock(&FD_ARR_mutex);
    if(mtx != 0){
        if(error_fun)error_fun(
                ERR_MESSAGE("Looks like flush thread already running or parallel LogOpen()\n")
                );
        return -1;
    }
    
    
    //assert(mtx==0);
    
    if(FD_LIST_ITEM_NUM == 0){

        //FD_ARR=calloc(1,sizeof(struct FD_LIST_ITEM));
        
        FD_ARR=aligned_alloc(sizeof(uintptr_t), sizeof(struct FD_LIST_ITEM));
        
        assert_msg(FD_ARR != NULL,"FD_ARR aligned_alloc failed");


        FD_LIST_ITEM_NUM=1;
    }
    else{
        struct FD_LIST_ITEM *tmp=NULL;
        errno=0;
        //tmp=realloc(FD_ARR,(FD_LIST_ITEM_NUM+1)*sizeof(struct FD_LIST_ITEM));
        
        tmp=aligned_alloc(sizeof(uintptr_t), (FD_LIST_ITEM_NUM+1)*sizeof(struct FD_LIST_ITEM));
        
        assert_msg(tmp != NULL,"add FD_LIST_ITEM to FD_ARR aligned_alloc failed");
        
        
        memcpy(tmp,FD_ARR,FD_LIST_ITEM_NUM*sizeof(struct FD_LIST_ITEM));
        free(FD_ARR);

        FD_ARR=tmp;
        FD_LIST_ITEM_NUM+=1;
        
    }//realloc
    
    
    
    const int N=FD_LIST_ITEM_NUM-1;
    memset(&(FD_ARR[N]),0,sizeof(struct FD_LIST_ITEM));
    
    
    
    
    
    /*{
    
    uint64_t c1,c2,diff;
    RDTSCP(c1);
    usleep(1000);
    RDTSCP(c2);
    
    diff = (c2-c1)/1000UL;
    
    if(cycles_in_one_usec == 0 || diff<cycles_in_one_usec)

            cycles_in_one_usec=diff;
    }
    */
    
    
    
    
    
    
    
    
            
    errno = 0;
    
    //Q_element *passive = calloc(1, sizeof (Q_element));
    Q_element *passive = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
    assert_msg(passive != NULL, "Q_element *passive = aligned_alloc");
        
    
    memset(passive,0,sizeof (Q_element));
    
    FD_ARR[N].QUEUE.head=passive;
    FD_ARR[N].QUEUE.tail=passive;
    
#ifdef RETURN
        Q_element *passive2 = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
        memset(passive2,0,sizeof (Q_element));
        FD_ARR[N].QUEUE.return_head = passive2;
        FD_ARR[N].QUEUE.return_tail=passive2;
        
        
        for(int p=0;p<config.return_q;p++){
             Q_element *P = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
             assert_msg(P != NULL, "Q_element aligned_alloc failed");
             memset(P,0,sizeof (Q_element));
             
             FD_ARR[N].QUEUE.return_head->next = P;
             FD_ARR[N].QUEUE.return_head = P;
        }
        
#endif    
    
    //int p = PTHREAD_PROCESS_SHARED;
    errno=0;
    assert_msg(
            pthread_spin_init(&(FD_ARR[N].QUEUE.insert_lock),PTHREAD_PROCESS_SHARED) == 0 
            , "pthread_spin_init FAILed");
        
    
    
    
    FD_ARR[N].next=NULL; //LIKED LIST OF FDs NOT USED
    FD_ARR[N].FD=NULL;
    
    
    char log_filename_template[LOG_F_NAME_LEN]={0};
    
    //char log_filename_date[25]={48,0}; //%Y%m%d_%H%M 20180413_1441
    
    
    
    


        if (logOpenTime.tv_sec == 0) {
            errno = 0;
            //критично. сразу на выход
            assert_msg (clock_gettime(CLOCK_ID_WALL_FAST, &logOpenTime) == 0, "clock_gettime FAILed");
        }

        //нужно для даты в имени файла
        struct tm *br_dw_time=NULL;
        br_dw_time = localtime(&logOpenTime.tv_sec);
        assert_msg(br_dw_time != NULL, "localtime FAILed");

        //нужно потом, для правильной печати даты
        if(config.timestamp_utc != 1)
            timezone_offset=br_dw_time->tm_gmtoff;
    

        
        //file name construct
        //   path / f_name_part1 . f_name_part2  . f_name_part3 . datetime .log


        memset(log_filename_template,0,sizeof(log_filename_template));
        char *p = log_filename_template;
        const size_t so = sizeof(log_filename_template);
        
    if (config.path) {
        size_t path_len = strlen(config.path);
        if (path_len > 0) {
            assert_msg(
                    config.path[path_len - 1] == '/'
                    ||
                    config.path[path_len - 1] == '\\'
                    , "Path should ends with slash");
            
            int copied = str_s_cpy(p,config.path,so - (p-log_filename_template));
            
            assert_msg(copied>0 ,"log path exceed filesystem maximum len");
            
            p+=copied;
        }
    }
        //copy f_name_part1. we've checked len priviusly
        {
            
            int copied = str_s_cpy(p,config.f_name_part1,so - (p-log_filename_template));
            assert_msg(copied>0 ,"f_name_part1 exceed filesystem maximum len");
            p+=copied;
        }
    
    if(config.f_name_part2 && strlen(config.f_name_part2)){
        *p++='.';
        
        int copied = str_s_cpy(p,config.f_name_part2,so - (p-log_filename_template));
            assert_msg(copied>0 ,"f_name_part1 exceed filesystem maximum len");
            p+=copied;
        
    }
    
    if(config.f_name_part3 && strlen(config.f_name_part3)){
        *p++='.';
        
        int copied = str_s_cpy(p,config.f_name_part3,so - (p-log_filename_template));
            assert_msg(copied>0 ,"f_name_part3 exceed filesystem maximum len");
            p+=copied;
        
    }

    if(config.file_name_with_date){
        *p++='.';
        
        size_t limit = so - (p-log_filename_template);
        int copied = snprintf(p, limit, "%04i%02i%02i-%02i%02i"
        , br_dw_time->tm_year + 1900
        , br_dw_time->tm_mon + 1
        , br_dw_time->tm_mday
        , br_dw_time->tm_hour
        , br_dw_time->tm_min);
        
    
        assert_msg(limit > copied ,"date part exceed filesystem maximum len");
        p+=copied;
    }
    
    if(config.file_name_with_pid){
        *p++='.';
        
        size_t limit = so - (p-log_filename_template);
        int copied = snprintf(p, limit, "%i",getpid());
    
        assert_msg(limit > copied ,"date part exceed filesystem maximum len");
        p+=copied;
    }
    
    {//add .log
    int copied = str_s_cpy(p,".log",so - (p-log_filename_template));
    assert_msg(copied>0 ,"ending .log exceed filesystem maximum len");
    p+=copied;    
    }
    
    
    
    
    //filename ready
    
    errno=0;
    FILE *t=fopen(log_filename_template, "a+");
    if (t== NULL) {
        if(error_fun)error_fun(
                ERR_MESSAGE("fopen (a+) FAILed for '%s', err: %s\n",log_filename_template, strerror(errno))
                );
        goto logOpen__exit;
    }
    
    errno=0;
    if(lockf(fileno(t),F_TLOCK,0) != 0){
        if(error_fun)error_fun(
                ERR_MESSAGE("Lock FAILed for '%s', err: %s\n",log_filename_template, strerror(errno))
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
    
    
    if(config.bufsize>0){
        FD_ARR[N].custom_io_buf = aligned_alloc(sizeof(uintptr_t), config.bufsize);
        assert_msg(FD_ARR[N].custom_io_buf != NULL, "custom_io_buf = aligned_alloc");
        errno=0;
        if(setvbuf(t,FD_ARR[N].custom_io_buf,_IOFBF,config.bufsize) !=0){
            if(error_fun)error_fun(
                ERR_MESSAGE("setvbuf FAILed for '%s', err: %s\n",log_filename_template, strerror(errno))
            );
            goto logOpen__exit;
        }
        //Z = __fbufsize(t);
        //fprintf(stderr,"__fbufsize: %zu\n",Z); 
    }
    
    fprintf(t,"# File opened. time_source %i, timestamp_utc %i, bufsize %zi, return_q %i\n",config.time_source,config.timestamp_utc,config.bufsize,config.return_q);
        
   
    FD_ARR[N].FD=t;
    memcpy(FD_ARR[N].log_filename,log_filename_template,sizeof(log_filename_template));
    
    *logFile=N+1;
    
    //global!!! for all logs same tome source!!!!
    time_source=config.time_source;
    
    //until logFlush starts
    FD_ARR[N].insert_forbidden = 1;
    
    //the only exit point!!!!
    
    pthread_mutex_unlock(&FD_ARR_mutex);
    return 0;

logOpen__exit:    
    pthread_mutex_unlock(&FD_ARR_mutex);
    return -1;
}


void logRedirect_stderr(int logFile, const char *funname, struct timespec time, const char* format, va_list ap){
    //lock
    char *eformat;
    if(asprintf(&eformat,"%li.%09li\t%s(%i): %s\n", time.tv_sec + timezone_offset, time.tv_nsec, funname, logFile, format) <0  ){
        return;
    }else{
        
        if(eformat){
            vfprintf(stderr,eformat,ap);
            free(eformat);
        }
    }
    
    //unlock
}

/*
 * функция гадит в STDERR 
 */
void logLogFixed(int logFile, size_t NUM, const char* format,  ...) { //вызывать как *file,(0|секунды),(0|микросекунды),"строка формата а-ля printf",переменный список аргументов
   // FILE *logFile=NULL;
  //  struct timespec logTime;
//    char va_list_formated_buffer[VA_MACRO_LEN];
    
    assert_msg(NUM<=PARAMS_FIXED_NUM, "NUM is limited! MAX: " __STRING(PARAMS_FIXED_NUM));
    assert_msg(NUM>=0, "NUM valid range is 0.." __STRING(PARAMS_FIXED_NUM));
    
    va_list arglist;
    Q_element *ENTRY;
    struct timespec time={0};
    
    
    if(time_source>=0)
        clock_gettime(time_source,&time);
    

    int N=logFile-1;
    
    errno = 0;

    if(
            UNLIKELY(N < 0)
            ||
            UNLIKELY(logFile>FD_LIST_ITEM_NUM)
            ||
            UNLIKELY(FD_ARR == NULL)
            ||
            UNLIKELY(FD_ARR[N].FD == NULL)
            ||
            UNLIKELY(FD_ARR[N].insert_forbidden == 1)
            
            ){
        
        va_start(arglist, format);
        logRedirect_stderr(logFile,__func__,time,format,arglist);
        va_end(arglist);
        
        return;
    }
    
    
    
    
    LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);
    
    
#ifndef RETURN
    ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#else
    
//несколько тредов будут разбирать список, нужна блокировка. это конечно неахти
    pthread_spin_lock(&(Q->insert_lock));
    
    //есть а наличии следующий эллемент? берем текущий, а слежующий помечем хвостом
    if(LIKELY(Q->return_tail->next != NULL)){
        ENTRY = Q->return_tail;
        Q->return_tail = Q->return_tail->next;
    }else{
        ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#ifdef LOG_DEBUG
        fprintf(stderr,"RETURN Q EMPTY\n");
#endif        
    }
    
    pthread_spin_unlock(&(Q->insert_lock));
    
#endif    
    
    
 
    
    
    
    //неважно какой истоник элемента - обнулить!
    assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");
    memset(ENTRY,0,sizeof (Q_element));
    
    
    
    uintptr_t *p = ENTRY->params;

    va_start(arglist, format); //LAST PARAM!!!
    
    
    //боюсь что забирая значения по ширине машинного слова, могу забрать сразу два
    for(int num = 0 ; num <NUM ; num++) {
        p[num] = va_arg(arglist,uintptr_t);
    }
    va_end(arglist);
    
    ENTRY->format_string = format;
    ENTRY->num = NUM;

    ENTRY->realtime = time;
    
    //очередь одна, вставка многими - нужна блокировка
    pthread_spin_lock(&(Q->insert_lock));
    
    
    Q->head->next=ENTRY;
    Q->head=ENTRY;
    Q->cnt_in+=1;
    
    //мем фенс!!!
    //__sync_synchronize;
    //__asm__ __volatile__("":::"memory");
    
    /*next!*/
    
    pthread_spin_unlock(&(Q->insert_lock));
    
    
}

void logLogRelaxed(int logFile, const char* format,  ...) {
    

    
    va_list arglist;
    Q_element *ENTRY;
    
        struct timespec time={0};
    
    
    if(time_source>=0)
        clock_gettime(time_source,&time);

    
    //ENTRY = calloc(1,sizeof(Q_element));
    //ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
    
    //assert_msg(ENTRY != NULL);
    //memset(ENTRY,0,sizeof (Q_element));
    //сначала запоминаем время
    errno = 0;
    /*if(clock_gettime(CLOCK_ID_WALL,&(ENTRY->realtime))<0){
        perror("logLog-clock_gettime(CLOCK_ID_WALL)");
    }*/
    int N=logFile-1;
    
    errno = 0;

    if(
            UNLIKELY(N < 0)
            ||
            UNLIKELY(logFile>FD_LIST_ITEM_NUM)
            ||
            UNLIKELY(FD_ARR == NULL)
            ||
            UNLIKELY(FD_ARR[N].FD == NULL)
            ||
            UNLIKELY(FD_ARR[N].insert_forbidden == 1)
            
            ){
        
        va_start(arglist, format);
        logRedirect_stderr(logFile,__func__,time,format,arglist);
        va_end(arglist);
        
        return;
    }
    
    
    
    LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);
    
#ifndef RETURN
    ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#else
    
//несколько тредов будут разбирать список, нужна блокировка. это конечно неахти
    pthread_spin_lock(&(Q->insert_lock));
    
    //есть а наличии следующий эллемент? берем текущий, а слежующий помечем хвостом
    if(Q->return_tail->next){
        ENTRY = Q->return_tail;
        Q->return_tail = Q->return_tail->next;
    }else{
        ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#ifdef LOG_DEBUG        
        fprintf(stderr,"RETURN Q EMPTY\n");
#endif        
    }
    
    pthread_spin_unlock(&(Q->insert_lock));
    
#endif        
    
    assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");
    memset(ENTRY,0,sizeof (Q_element));
    
    
    
    
    errno = 0;
    va_start(arglist, format); //LAST PARAM!!!
    
    uintptr_t *p = ENTRY->params;

    const char *string = format;
    int num = 0;
    while (*string != '\0'){
    if (*string == '%'){
        
        ++string;
        while(*string != '\0'){
            switch (UNLIKELY(*string > 'x') ? 0 : stop_tb[(int)*string]) {
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
                    default:
                    {
                        ++string;
                        continue;
                    }
                }
            
        }//search for end

    }//start of format
next:    
    ++string;
  }//outer while
    
    

    va_end(arglist);
    
    ENTRY->format_string = format;
    ENTRY->num = num; //количество либо 0 либо больше нуля
    
    assert_msg(num <= PARAMS_FIXED_NUM, "NUM is limited! MAX: " __STRING(PARAMS_FIXED_NUM));
    
    ENTRY->realtime = time;
    
    
    pthread_spin_lock(&(Q->insert_lock));
    
    Q->head->next=ENTRY;
    Q->head=ENTRY;
    Q->cnt_in+=1;

    pthread_spin_unlock(&(Q->insert_lock));

    
}
static int char_spec_mean(char c){
        switch (c) {
                case 'A':
                case 'C':
                case 'E':
                case 'F':
                case 'G':
                case 'X':
                case 'a':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'i':
                case 'm':
                case 'n':
                case 'o':
                case 'p':
                case 'u':
                case 'x':
                    {
                        return 1;
                    }
                    case 's':
                    case 'S':
                    {
                        return 2;

                    }
                    case '%':
                    {
                        return 3;
                    }
                    default:
                    {
                        return 0;
                    }
                }
        
        perror("switch");
        exit(EXIT_FAILURE);

}
static void prepare_Q(){
    //return;
    int jtb_index=0;
    for(int i=0;i<256;i+=4){
        int m =0;
        char j=0;
        m = char_spec_mean(i);
        j|=m;
        
        m = char_spec_mean(i+1);
        j|=m<<2;
        
        m = char_spec_mean(i+2);
        j|=m<<4;
        
        m = char_spec_mean(i+3);
        j|=m<<6;
        
        stop_tb_Q[jtb_index]=j;
        jtb_index++;
    }
}

void logLogRelaxed_Q(int logFile, const char* format,  ...) {
    
    va_list arglist;
    Q_element *ENTRY;
    
        struct timespec time={0};
    
    
    if(time_source>=0)
        clock_gettime(time_source,&time);

    
    //ENTRY = calloc(1,sizeof(Q_element));
    //ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
    
    //assert_msg(ENTRY != NULL);
    //memset(ENTRY,0,sizeof (Q_element));
    //сначала запоминаем время
    errno = 0;
    /*if(clock_gettime(CLOCK_ID_WALL,&(ENTRY->realtime))<0){
        perror("logLog-clock_gettime(CLOCK_ID_WALL)");
    }*/
    int N=logFile-1;
    
    errno = 0;

    if(
            UNLIKELY(N < 0)
            ||
            UNLIKELY(logFile>FD_LIST_ITEM_NUM)
            ||
            UNLIKELY(FD_ARR == NULL)
            ||
            UNLIKELY(FD_ARR[N].FD == NULL)
            ||
            UNLIKELY(FD_ARR[N].insert_forbidden == 1)
            
            ){
        
        va_start(arglist, format);
        logRedirect_stderr(logFile,__func__,time,format,arglist);
        va_end(arglist);
        
        return;
    }
    
    
    
    LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);
    
#ifndef RETURN
    ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#else
    
//несколько тредов будут разбирать список, нужна блокировка. это конечно неахти
    pthread_spin_lock(&(Q->insert_lock));
    
    //есть а наличии следующий эллемент? берем текущий, а слежующий помечем хвостом
    if(Q->return_tail->next){
        ENTRY = Q->return_tail;
        Q->return_tail = Q->return_tail->next;
    }else{
        ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#ifdef LOG_DEBUG        
        fprintf(stderr,"RETURN Q EMPTY\n");
#endif        
    }
    
    pthread_spin_unlock(&(Q->insert_lock));
    
#endif        
    
    assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");
    memset(ENTRY,0,sizeof (Q_element));
    
    
    
    
    errno = 0;
    va_start(arglist, format); //LAST PARAM!!!
    
    uintptr_t *p = ENTRY->params;

    const char *string = format;
    int num = 0;
    while (*string != '\0'){
    if (*string == '%'){
        
        ++string;
        while(*string != '\0'){
            uint8_t v=0;
            char c=*string;
                //if (LIKELY(c < '{' && c > ' ')) {
                //if (LIKELY(c < '{')) 
                    

                    uint8_t pos = c / 4;
                    
                    uint8_t sub_pos = (c % 4)*2;

                    v = stop_tb_Q[pos];
                    
                    v >>= sub_pos;
                    v &= 0x3;

                //}
            switch (v) {
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
                    default:
                    {
                        ++string;
                        continue;
                    }
                }
            
        }//search for end

    }//start of format
next:    
    ++string;
  }//outer while
    
    

    va_end(arglist);
    
    ENTRY->format_string = format;
    ENTRY->num = num; //количество либо 0 либо больше нуля
    
    assert_msg(num <= PARAMS_FIXED_NUM, "NUM is limited! MAX: " __STRING(PARAMS_FIXED_NUM));
    
    ENTRY->realtime = time;
    
    
    pthread_spin_lock(&(Q->insert_lock));
    
    Q->head->next=ENTRY;
    Q->head=ENTRY;
    Q->cnt_in+=1;

    pthread_spin_unlock(&(Q->insert_lock));

    
}


void logLogRelaxed_wojt(int logFile, const char* format,  ...) {
    

    
    va_list arglist;
    Q_element *ENTRY;
    
        struct timespec time={0};
    
    
    if(time_source>=0)
        clock_gettime(time_source,&time);

    
    //ENTRY = calloc(1,sizeof(Q_element));
    //ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
    
    //assert_msg(ENTRY != NULL);
    //memset(ENTRY,0,sizeof (Q_element));
    //сначала запоминаем время
    errno = 0;
    /*if(clock_gettime(CLOCK_ID_WALL,&(ENTRY->realtime))<0){
        perror("logLog-clock_gettime(CLOCK_ID_WALL)");
    }*/
    int N=logFile-1;
    
    errno = 0;

    if(
            UNLIKELY(N < 0)
            ||
            UNLIKELY(logFile>FD_LIST_ITEM_NUM)
            ||
            UNLIKELY(FD_ARR == NULL)
            ||
            UNLIKELY(FD_ARR[N].FD == NULL)
            ||
            UNLIKELY(FD_ARR[N].insert_forbidden == 1)
            
            ){
        
        va_start(arglist, format);
        logRedirect_stderr(logFile,__func__,time,format,arglist);
        va_end(arglist);
        
        return;
    }
    
    
    
    LOG_QUEUE *Q = &(FD_ARR[N].QUEUE);
    
#ifndef RETURN
    ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#else
    
//несколько тредов будут разбирать список, нужна блокировка. это конечно неахти
    pthread_spin_lock(&(Q->insert_lock));
    
    //есть а наличии следующий эллемент? берем текущий, а слежующий помечем хвостом
    if(Q->return_tail->next){
        ENTRY = Q->return_tail;
        Q->return_tail = Q->return_tail->next;
    }else{
        ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#ifdef LOG_DEBUG        
        fprintf(stderr,"RETURN Q EMPTY\n");
#endif        
    }
    
    pthread_spin_unlock(&(Q->insert_lock));
    
#endif        
    
    assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");
    memset(ENTRY,0,sizeof (Q_element));
    
    
    
    
    errno = 0;
    va_start(arglist, format); //LAST PARAM!!!
    
    uintptr_t *p = ENTRY->params;

    const char *string = format;
    int num = 0;
    while (*string != '\0'){
    if (*string == '%'){
        
        ++string;
        while(*string != '\0'){
            switch (*string) {

                    case 'A':
                    case 'C':
                    case 'E':
                    case 'F':
                    case 'G':
                    case 'X':
                    case 'a':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                    case 'g':
                    case 'i':
                    case 'm':
                    case 'n':
                    case 'o':
                    case 'p':
                    case 'u':
                    case 'x':
                    {
                        p[num] = va_arg(arglist, uintptr_t);
                        ++num;
                        goto next;
                    }
                    case 's':
                    case 'S':
                    {
                        p[num] = (uintptr_t) strdup(va_arg(arglist, char *));
                        ENTRY->params_free_mask |= 1 << num;
                        ++num;
                        goto next;

                    }
                    case '%':
                    {
                        goto next;
                    }
                    default:
                    {
                        ++string;
                        continue;
                    }
                }

            }//search for end

    }//start of format
next:    
    ++string;
  }//outer while
    
    

    va_end(arglist);
    
    ENTRY->format_string = format;
    ENTRY->num = num; //количество либо 0 либо больше нуля
    
    assert_msg(num <= PARAMS_FIXED_NUM, "NUM is limited! MAX: " __STRING(PARAMS_FIXED_NUM));
    
    ENTRY->realtime = time;
    
    
    pthread_spin_lock(&(Q->insert_lock));
    
    Q->head->next=ENTRY;
    Q->head=ENTRY;
    Q->cnt_in+=1;

    pthread_spin_unlock(&(Q->insert_lock));

    
}


void logLogRelaxedDummy(int logFile, const char* format,  ...) { 
    

    
    va_list arglist;
    Q_element ENTRY;
    
    struct timespec time={0};
    
    if(time_source>=0)
        clock_gettime(time_source,&time);
    
    //int N=logFile-1;
    
    
    va_start(arglist, format); //LAST PARAM!!!

    //просто пробежаться пл сроке - 100+ нано

    const char *string = format;
    int num = 0;
    while (*string != '\0'){
    if (*string == '%'){
        
        ++string;
        while(*string != '\0'){
            //int n = stop_tb[*string];
            switch (*string > 'x' ? 0 : stop_tb[(int)*string]) {
                    case 0:
                    {
                        ++string;
                        continue;
                    }
                    case 1:
                    {
                        ENTRY.params[num] = va_arg(arglist, uintptr_t);
                        ++num;
                        goto next;
                    }
                    case 2:
                    {
                        //мы пишем в стековую ENTRY и потом при выходе то что сделал strdup потеряется
                        ENTRY.params[num] = (uintptr_t) strdup(va_arg(arglist, char *));
                        ENTRY.params_free_mask |= 1 << num;
                        ++num;
                        goto next;

                    }
                    case 3:
                    {
                        goto next;
                    }
                    default:
                    {
                        ++string;
                        continue;
                    }
                }
        }//search for end

    }//start of format
next:    
    ++string;
  }//outer while
    
    

    va_end(arglist);
    
    ENTRY.realtime = time;
    
    ENTRY.format_string = format;
    ENTRY.num = num;
    
    //assert_msg(ENTRY.num <= PARAMS_FIXED_NUM);
    
    

    
}
#define NOT_IN_JUMP_RANGE(Ch) ((Ch) < '%' || (Ch) > 'x')
#define CHAR_CLASS(Ch) (jump_table[(int) (Ch) - '%'])

void logLogRelaxedDummy_wojt(int logFile, const char* format,  ...) { 
    

    
    va_list arglist;
    Q_element ENTRY;
    
    struct timespec time={0};
    
    if(time_source>=0)
        clock_gettime(time_source,&time);
    
    //int N=logFile-1;
    
    
    va_start(arglist, format); //LAST PARAM!!!

    //просто пробежаться пл сроке - 100+ нано

    const char *string = format;
    int num = 0;
    while (*string != '\0'){
    if (*string == '%'){
        
        ++string;
        while(*string != '\0'){
            //int n = stop_tb[*string];
            //int c = (int)*string; 
            //int v = NOT_IN_JUMP_RANGE (c) ? 0 : CHAR_CLASS (c);
            switch (*string) {
                    /*case 0:
                    {
                        ++string;
                        continue;
                    }*/

                case 'A':
                case 'C':
                case 'E':
                case 'F':
                case 'G':
                case 'X':
                case 'a':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'i':
                case 'm':
                case 'n':
                case 'o':
                case 'p':
                case 'u':
                case 'x':
                    {
                        ENTRY.params[num] = va_arg(arglist, uintptr_t);
                        ++num;
                        goto next;
                    }
                    case 's':
                    case 'S':
                    {
                        //мы пишем в стековую ENTRY и потом при выходе то что сделал strdup потеряется
                        ENTRY.params[num] = (uintptr_t) strdup(va_arg(arglist, char *));
                        ENTRY.params_free_mask |= 1 << num;
                        ++num;
                        goto next;

                    }
                    case '%':
                    {
                        goto next;
                    }
                    default:
                    {
                        ++string;
                        continue;
                    }
                }
        }//search for end

    }//start of format
next:    
    ++string;
  }//outer while
    
    

    va_end(arglist);
    
    ENTRY.realtime = time;
    
    ENTRY.format_string = format;
    ENTRY.num = num;
    
    //assert_msg(ENTRY.num <= PARAMS_FIXED_NUM);
    
    

    
}


logObj * logLogFullObj(size_t bytes){
    if(bytes == 0){
        bytes = logLogFullObjBufLen; //global Static 2048 bytes
    }
    logObj *Obj = aligned_alloc(sizeof(uintptr_t), sizeof (logObj));
    assert_msg(Obj != NULL, "logObj aligned_alloc failed");
    Obj->buf = aligned_alloc(sizeof(uintptr_t), bytes);
    assert_msg(Obj->buf != NULL,"logObj buffer aligned_alloc failed");
    memset(Obj->buf,0,bytes);
    Obj->n = bytes;
    
    return Obj;
}

void logLogFullObjFree(logObj **Obj){
    assert_msg(Obj != NULL, "Not valid Obj poiner");
    assert_msg(*Obj != NULL, "Not valid Obj. Already free?");
    
    if((*Obj)->buf)
        free((*Obj)->buf);

    //memset(*Obj,0,sizeof(logObj));

    free(*Obj);
    *Obj=NULL;
    
    return;
}

void logLogFull(int logFile, logObj *obj, const char* format,  ...) {
    Q_element *ENTRY;
    LOG_QUEUE *Q;
    
    va_list arglist;
    

        struct timespec time={0};
    
    
    if(time_source>=0)
        clock_gettime(time_source,&time);


    //сначала запоминаем время
    //errno = 0;
    /*if(clock_gettime(CLOCK_ID_WALL,&(ENTRY->realtime))<0){
        perror("logLog-clock_gettime(CLOCK_ID_WALL)");
    }*/
    int N=logFile-1;
    
    errno = 0;

    if(
            UNLIKELY(N < 0)
            ||
            UNLIKELY(logFile>FD_LIST_ITEM_NUM)
            ||
            UNLIKELY(FD_ARR == NULL)
            ||
            UNLIKELY(FD_ARR[N].FD == NULL)
            ||
            UNLIKELY(FD_ARR[N].insert_forbidden == 1)
            
            ){
        
        va_start(arglist, format);
        logRedirect_stderr(logFile,__func__,time,format,arglist);
        va_end(arglist);
        
        return;
    }
    
    //вот это врятли
    if(obj->buf == NULL){
        obj->buf = aligned_alloc(sizeof(uintptr_t), logLogFullObjBufLen);
        assert_msg(obj->buf != NULL,"logObj buf aligned_alloc failed");
        obj->n = logLogFullObjBufLen;
        //fprintf(stderr,"temp_format alloc\n");
    }
    Q = &(FD_ARR[N].QUEUE);

    
#ifndef RETURN
    ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#else
    
//несколько тредов будут разбирать список, нужна блокировка. это конечно неахти
    pthread_spin_lock(&(Q->insert_lock));
    
    //есть а наличии следующий эллемент? берем текущий, а слежующий помечем хвостом
    if(Q->return_tail->next){
        ENTRY = Q->return_tail;
        Q->return_tail = Q->return_tail->next;
    }else{
        ENTRY = aligned_alloc(sizeof(uintptr_t), sizeof (Q_element));
#ifdef LOG_DEBUG        
        fprintf(stderr,"RETURN Q EMPTY\n");
#endif        
    }
    
    pthread_spin_unlock(&(Q->insert_lock));
    
#endif        
    
    assert_msg(ENTRY != NULL,"Got non valid Queue element pointer");
    memset(ENTRY,0,sizeof (Q_element));

    int n=0;
    
    do {
        va_start(arglist, format); //LAST PARAM!!!
        errno = 0;
        n = vsnprintf(obj->buf, obj->n, format, arglist);
        assert_perror(errno);
        va_end(arglist);
        if (n >= obj->n) {
            obj->n += logLogFullObjBufLen;
            char *t = aligned_alloc(sizeof (uintptr_t), obj->n);
            assert_msg(t != NULL,"Obj buf expand aligned_alloc failed");
            obj->buf = t;
        } else {
            break;
        }

    } while (1);
    
    assert_msg(n>0, "something wrong with vsnprintf");
    
    n+=1;
    
    char *f = malloc(n);
    
    assert_msg(f != NULL,"malloc for finil string failed");
    
    memcpy(f,obj->buf,n);
    
    ENTRY->format_string=f;
    
    
    
    ENTRY->num = -1; // need FREE on `format_string`
    
    
    ENTRY->realtime = time;
    
    
    //if(1)
    //    ENTRY->tid = pthread_self();
    
    pthread_spin_lock(&(Q->insert_lock));
    
    Q->head->next=ENTRY;
    Q->head=ENTRY;
    Q->cnt_in+=1;

    pthread_spin_unlock(&(Q->insert_lock));

    
}

static int str_s_cpy(char *dest, const char *src, size_t sizeof_dest) {
//simplified version of strscpy from Linux kernel source
    
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