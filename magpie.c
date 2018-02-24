#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <emmintrin.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#ifdef __MACH__
#include <mach/mach_traps.h>
#include <mach/mach_time.h>
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif

#ifndef __MACH__
#define TIME_ABSOLUTE CLOCK_REALTIME
typedef struct timespec mach_timespec_t;
typedef unsigned int mach_port_t;

static inline uint64_t mach_absolute_time(void) {
    mach_timespec_t tp;
    int res = clock_gettime(CLOCK_REALTIME, &tp);
    if (res < 0) {
        perror("clock_gettime");
        exit(1);
    }
    uint64_t result = tp.tv_sec * NSEC_PER_SEC;
    result += tp.tv_nsec;
    return result;
}

// non-conformant wrapper just for the purposes of this application
static inline void clock_sleep_trap(mach_port_t clock_port, int sleep_type, time_t sec, long nsec, mach_timespec_t *remain) {
    mach_timespec_t req = { sec, nsec };
    int res = clock_nanosleep(sleep_type, TIMER_ABSTIME, &req, remain);
    if (res < 0) {
        perror("clock_nanosleep");
        exit(1);
    }
}
#endif // __MACH__

__m128i reg;
__m128i reg_zero;
__m128i reg_one;
mach_port_t clock_port;
mach_timespec_t remain;

static inline void square_am_signal(float time, float frequency) {
    uint64_t period = NSEC_PER_SEC / frequency;

    uint64_t start = mach_absolute_time();
    uint64_t end = start + (uint64_t)(time * NSEC_PER_SEC);

    while (mach_absolute_time() < end) {
        uint64_t mid = start + period / 2;
        uint64_t reset = start + period;
        while (mach_absolute_time() < mid) {
            _mm_stream_si128(&reg, reg_one);
            _mm_stream_si128(&reg, reg_zero);
        }
        clock_sleep_trap(clock_port, TIME_ABSOLUTE, reset / NSEC_PER_SEC, reset % NSEC_PER_SEC, &remain);
        start = reset;
    }
}
static inline void showhelp(char *argv[]){
            fprintf(stderr, "usage: %s [-m|a|h] [-l loops] -f fname\n",argv[0]);
            fprintf(stderr, "magpie - an AM-FSK file transmitter in the 1500khzrange\n\n Uses PWM of square waves to generate AM-FSK transmission of a file \n\nbased on SBR by William Entriken:https://github.com/fulldecent\n\n");
            fprintf(stderr," Required:\n");
            fprintf(stderr,"  -f [filename] \t the file specified in filename is read and transmitted\n");
            fprintf(stderr,"\n Optional:\n");
            char uc[] = "  -l [int] \t\t loops sets the number of times the data is transmitted. \n";
            fprintf(stderr,"%s",uc);
            char uc2[] = "  -m\t\t\t uses 8FSK encoding.\n  -a \t\t\t uses AFSK encoding (0=1200hz, 1=2200hz) (Default)\n\n";
            fprintf(stderr,"%s",uc2);
}

static inline void preamble(char pre){
if(pre==1){
fprintf(stderr, "\n 0  1  1  0 ");
square_am_signal(1.0 * 200 /2 / 1000, 1200);
square_am_signal(1.0 * 200 /2 / 1000, 2200);
square_am_signal(1.0 * 200 /2 / 1000, 2200);
square_am_signal(1.0 * 200 /2 / 1000, 1200);
} else {
fprintf(stderr, "\n");
}
return;
}

static inline void afskbit(char b, int time_ms){
    int freq_hz = 0;
    if(b == 1){
        fprintf(stderr," 1 ");
        freq_hz = 1200;
        }
    else {
        freq_hz = 2200;
        fprintf(stderr," 0 ");
        }
        square_am_signal(1.0 * time_ms / 1000, freq_hz);
}

static inline void mfsknib(char nib, int time_ms){
    //read byte in, and split to bit array.
        int freq=0;
        switch(nib){
        case 0:
        freq=200*2;
        break;
        case 1:
        freq=200*3;
        break;
        case 2:
        freq=200*5;
        break;
        case 3:
        freq=200*7;
        break;
        case 4:
        freq=200*11;
        break;
        case 5:
        freq=200*13;
        break;
        case 6:
        freq=200*17;
        break;
        case 7:
        freq=200*19;
        break;
        }
        fprintf(stderr, " %d hz", freq);
        square_am_signal(1.0 * time_ms /2 / 1000, freq);
}

char makenib(char mb_0, char mb_1, char mb_2){
return (mb_0 + (mb_1 << 1) + (mb_2 << 2));

}

static inline void fileplayer(char *file, int loops, int enc, char pre){
FILE *fileptr;
char *buffer;
long filelen;

fileptr = fopen(file, "rb");  // Open the file in binary mode
fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
filelen = ftell(fileptr);             // Get the current byte offset in the file
printf("filelen:%ld\n",filelen);
rewind(fileptr);                      // Jump back to the beginning of the file

buffer = (char *)malloc((filelen+1)*sizeof(char)); // Enough memory for file + \0
fread(buffer, filelen, 1, fileptr); // Read in the entire file
fclose(fileptr); // Close the file
char bitnow=0;

int time_ms=200;

for ( int j = 0; j < loops; j ++){
int bc=0;
for ( int i = 0; i < filelen + 1; i++) {
    char mb_0=0;
    char mb_1=0;
    char mb_2=0;

    if( i  %  2 == 0 ){
    if (enc == 1){
                //preamble (0110) every 16 bits
                preamble(pre);
            }else {
            fprintf(stderr,"\n");
            }
        }
    for ( int b =0; b < 7; b++) { 
    bitnow = (buffer[i] & ( 1 << b )) >> b ;
    
    if (enc == 1){
        afskbit(bitnow, time_ms); 
    }
    else {
            //rolling 3 bit windows, used to determine 8FSK frequency.
            mb_2 = bitnow & 1;
            char nib = makenib(mb_2, mb_1, mb_0);
            mfsknib(nib, time_ms);
            mb_0=mb_1 & 1;
            mb_1=mb_2 & 1;
        }

    }
}
bc+=1;
fprintf(stdout, "%d\n", bc);
}
}


int
main(int argc, char *argv[ ])
{
    int c;
    int tflg=0, eflg=0, pflg=0, errflg = 0;
    char *file = NULL;
    int loops=1;
    extern char *optarg;
    extern int optind, optopt;
    while ((c = getopt(argc, argv, ":hmaf:l:")) != -1) {
        switch(c) {
        case 'h':
            showhelp(argv);
            exit(0);
        case 'm':
            if (eflg){
                errflg++;
                }
            else{
                tflg=1;
                }
            break;
        case 'a':
            if (tflg){
                errflg++;
                }
            else {
                eflg=1;
            }
            break;
        case 'f':
            file = optarg;
           break;
        case 'l':
            loops = atoi(optarg);
            break;
            case ':':       /* -f or -o without operand */
                    fprintf(stderr,
                            "Option -%c requires an operand\n", optopt);
                    errflg++;
                    break;
        case '?':
            showhelp(argv);
            exit(0);
        }
    }
    if (file == NULL)
        errflg++;
    
    if (errflg) {
        fprintf(stderr, "usage: %s [-m|a|h] [-l loops] -f fname\n",argv[0]);
        exit(2);
    }
//Setup below

#ifdef __MACH__
    mach_timebase_info_data_t theTimeBaseInfo;
    mach_timebase_info(&theTimeBaseInfo);
    puts("TESTING TIME BASE: the following should be 1 / 1");
    printf("  Mach base: %u / %u nanoseconds\n", theTimeBaseInfo.numer, theTimeBaseInfo.denom);
#endif
    uint64_t start = mach_absolute_time();
    uint64_t end = mach_absolute_time();
    printf("TESTING TIME TO EXECUTE mach_absolute_time()\n  Result: %"PRIu64" nanoseconds\n", end - start);

    reg_zero = _mm_set_epi32(0, 0, 0, 0);
    reg_one = _mm_set_epi32(-1, -1, -1, -1);

int enc=1;
if(tflg==1){
    fprintf(stderr, "Encoding set to 8FSK.\n");
    enc=2;
    }else {
    fprintf(stderr, "Encoding set to AFSK.\n");
    }
char pre=0;
if(pflg==1){
    pre=1;
    fprintf(stderr, "Preamble enabled.\n");
    }else{
    fprintf(stderr, "Preamble disabled.\n");
    }
// Setup Code goes ^^^
fileplayer(file,loops,enc,pre);
}
