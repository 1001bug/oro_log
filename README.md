# Asynchronous logging for C99
For HTF testing robots I need logging which returns from log() function as fast as possible. Throughput does not matter.  
I read about C++ asynchronous logs that some how copy variadic parameters. I don't know and don't like C++. I use C99 for project where speed make sense.  

I make some test on x86\_64 Intel and Arm (RaspberryPi) to copy variadic params by Word size and it is work some how.  
So base on it I hardcode limit for parameters count from 5 to 32 and write several log functions. 

- oroLofFixed: for const format string and copyble parameters
- oroLogRelaxed: for const format string and make strdup on %s
- oroLogFull: made with vsnprintf. Slowest. 



Without query time 
- oroLogFixed without locks 35-40 nanoseconds
- oroLogFixed\_unlocked take 40-50 nanoseconds
- oroLogRelaxed about 100 nanoseconds
- oroLogFull - 280
- By the way, fprintf takes 250 nanoseconds


Internaly vsnprintf in gligc not so mach differs from printf. But direct fprintf on FILE\* faster by 30 nano


This is NetBeansIDE project but only 3 files: 
- main.c testing benchmark
- oro\_log.c all log code 
- oro\_log.h public defines

compile with -O2 -mavx

Time measure made with `__asm__ __volatile__("rdtscp;shl $32, %%rdx;or %%rdx, %%rax":"=a"(V):: "%rcx", "%rdx")` and on ARM does not work. Use clock_gettime(CLOCK_MONOTONIC)

