#ifndef _PROGASS_LIB_
#define _PROGASS_LIB_

#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <litmus.h>
#include <litmus/rt_param.h>

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <jevents.h>
#include <perf-iter.h>
#include <rdpmc.h>

#define DATA_FILE_NAME "timeinfo.h"
#define BILLION 1000000000UL
#define TIME_METADATA_INDEXFILE "meta_index.info"
#define PIPE_ID "/run/process_pipe"

#define MEM_MODEL

#define CALL( exp ) do { \
  int ret; \
  ret = exp; \
  if (ret != 0) {\
    fprintf(stderr, "%s failed: %m\n", #exp); \
    /*exit(-1);we should do this for error check*/\
  }\
  else \
    fprintf(stderr, "%s ok.\n", #exp); \
} while(0)

typedef unsigned long long ull;

extern struct rt_task params;
extern struct control_page *litmus_cp;
extern int start_job_index;
extern lt_t start_release_time;

extern pid_t task_tid;
extern int Progress_Pipe_ID;

#ifdef MEM_MODEL
extern struct rdpmc_ctx load_ctx, store_ctx;
//the following are readings from Performance Counters
extern ull start_load_val, start_store_val; // this is the value saved when a single run is started
extern ull end_load_val, end_store_val;// this is the end value of a single run
#endif

// Each process will send this message struct to the scheduling agent
extern struct Message_Struct {
  pid_t self_pid; // this process' pid, don't need to be changed
  int fn_id;
  int bb_id;
  ull cur_time;
} msg;

extern struct timespec t;
// this function returns current time (from the epoch) in nanseconds
inline __attribute__((used, always_inline)) ull getTime();

/**** global variables for timing information ******/
// For writing profiling data
extern FILE *dataFile;
//#define DATA_FILE_NAME "timeinfo.log"
extern ull start_time;

static inline __attribute__((used, always_inline)) void setup_perf_counter() {
    struct perf_event_attr load_attri, store_attri;
    // -- Load
    if (resolve_event("mem_inst_retired.all_loads", &load_attri) < 0) {
        printf("Error in resolve_event");
        exit(-1);
    }
    if (rdpmc_open_attr(&load_attri, &load_ctx, NULL) < 0) {
        printf("Error in rdpmc_open_attr");
        exit(-1);
    }

    // -- Store
    if (resolve_event("mem_inst_retired.all_stores", &store_attri) < 0) {
        printf("Error in resolve_event");
        exit(-1);
    }
    if (rdpmc_open_attr(&store_attri, &store_ctx, NULL) < 0) {
        printf("Error in rdpmc_open_attr");
        exit(-1);
    }
    
    printf("PERF Counter is set up.\n");
}

// this function returns current time (from the epoch) in nanseconds
inline __attribute__((used, always_inline)) ull getTime() {
  clock_gettime(CLOCK_MONOTONIC, &t);
  return ((ull)t.tv_sec * BILLION) + t.tv_nsec;
}

static inline __attribute__((used, always_inline)) void profilingSetup() {
  // This is for profiling - and NOT announcing time
  dataFile = fopen(DATA_FILE_NAME, "a+");
  if(dataFile == NULL) {
    perror("profiling set up");
    exit(-1);
  }
#ifdef MEM_MODEL
  setup_perf_counter();
#endif
}

static inline __attribute__((used, always_inline)) void setStartTime() {
  start_job_index = litmus_cp->job_index;
  start_release_time = litmus_cp->release;
#ifdef MEM_MODEL
  start_load_val = rdpmc_read(&load_ctx);
  start_store_val = rdpmc_read(&store_ctx);
#endif
}

static inline __attribute__((used, always_inline)) void profilerStartTime() {
  start_time = getTime();
#ifdef MEM_MODEL
  start_load_val = rdpmc_read(&load_ctx);
  start_store_val = rdpmc_read(&store_ctx);
#endif
}

// This function writes current time from the start of the program
// to a file named with `dataFileName`
static inline __attribute__((used, always_inline)) void writeTime(int FN_ID, int BB_ID) {
  // -- TIME PROFILING --
  //fprintf(dataFile, "#define CP_%d_%d %llu\n",FN_ID, BB_ID, (getTime() - start_time));
  
  #ifdef MEM_MODEL
  // -- MEM PROFILING --
  end_load_val = rdpmc_read(&load_ctx);
  end_store_val = rdpmc_read(&store_ctx);
  fprintf(dataFile, "CP_%d_%d_MEM %llu\t", FN_ID, BB_ID,
    (end_load_val - start_load_val) + (end_store_val - start_store_val));
  #endif 

  fflush(dataFile);
}

// This function should be run after a single iteration to get the final MEM
// data.
static inline __attribute__((used, always_inline)) void endIteration(int FN_ID,
int BB_ID) {
  end_load_val = rdpmc_read(&load_ctx);
  end_store_val = rdpmc_read(&store_ctx);
  fprintf(dataFile, "FULL_MEM %llu\n", 
    (end_load_val - start_load_val) + (end_store_val - start_store_val));
  fflush(dataFile);
}

// Close profiling file pointer and flush data
static inline __attribute__((used, always_inline)) void endProfiling() {
  fclose(dataFile);
}

extern ull extra_cost_arr[200];
extern int extra_cost_it;

// this function sets up the shared memory, gets the scheduler process id
static inline __attribute__((used, always_inline)) void configureIPC() {
#ifdef MEM_MODEL
  setup_perf_counter();
#endif
  task_tid = gettid();
  litmus_cp = get_ctrl_page();
  int i;
  for(i = 0; i < 200; i++) {
    extra_cost_arr[i] = 0;
  }
}

// current perf read is read in end_load_val and end_store_val.
static inline __attribute__((used, always_inline)) void end_perf_read() {
#ifdef MEM_MODEL
    end_load_val = rdpmc_read(&load_ctx);
    end_store_val = rdpmc_read(&store_ctx);
    //printf("Store Access: %llu\n", end_store_val - start_store_val);
    //printf("Load Access: %llu\n", end_load_val - start_load_val);
#endif
}

// this function predicts the number of future memory accesses from the
// observed memory accesses until this point
// cur_mem_access: it's the currently observed number of memory accesses
static inline __attribute__((used, always_inline)) ull predicted_mem_access(ull
cur_mem_access, ull ref_checkpoint_mem_access, ull ref_full_mem_access) {
  // We'll just show a linear prediction model here
  return (double) (cur_mem_access / ref_checkpoint_mem_access) *
  ref_full_mem_access;
}

//#define CP_1_3 1154071965

#define STATIC_KERNELOVERHEAD 200000UL
#define ns2us(x) ((double) (x) / 1000)

//#define COMPENSATE
#define EXTRAPOLATE 1
#define EXTRAPOLATE_FACTOR 1

// For Darknet Object Classification
#define LO_MEM_ACCESS 601561075
#define CP_MEM_ACCESS 208954504

// For dlib object tracking
//#define LO_MEM_ACCESS 34644249
//#define CP_MEM_ACCESS 17881218

#define ANNOUNCE_TIME_MEM(fn, bb) do{\
  /*lt_t cp_s = litmus_clock();*/\
  /*int inter_job_index = litmus_cp->job_index;*/\
  /*printf("SJob:%d, CurJob:%d\n", start_job_index, inter_job_index);*/\
  if (0/*inter_job_index != start_job_index*/) {\
    printf("Very Late\n");\
  } else {\
    end_perf_read();\
    lt_t cur_time, rem;\
    /*lt_t sss = litmus_clock();*/\
    get_current_budget(&cur_time, &rem);\
    /*lt_t ssse = litmus_clock();\
    printf("Overhead: %llu\n", ns2us(ssse - sss));*/\
    ull ref_time = CP_##fn##_##bb;\
    /*printf("C:%llu R:%llu, lag?%d\n", (cur_time), (ref_time), cur_time >\
    ref_time)*/;\
    if(1) {\
      double extra;\
      lt_t prev_time, extra_cost;\
      ull mem_access_cp = (end_store_val - start_store_val) + (end_load_val -\
      start_load_val);/*mem access until a checkpoint*/\
      double pr_lo = ((double) (params.exec_cost) / LO_MEM_ACCESS);\
      double pr_ref_cp = ((double) (ref_time) / CP_MEM_ACCESS);\
      double pr_cp = ((double) (cur_time) / mem_access_cp);\
      ull POST_CP_LO_MEM_ACCESS = LO_MEM_ACCESS - CP_MEM_ACCESS;\
      ull post_cp_pred_mem_access = ((double) POST_CP_LO_MEM_ACCESS / \
      CP_MEM_ACCESS) * mem_access_cp;\
      /*printf("mem_access_cp: %llu pr_ref_cp: %.4f, pr_cp:%.4f, post_cp_pred: %llu\n",\
      mem_access_cp, pr_ref_cp, pr_cp, post_cp_pred_mem_access);\
      printf("mem_lag: %d, time_lag: %d\n", (pr_cp > pr_ref_cp),\
      (cur_time > ref_time));*/\
      if(/*cur_time <= ref_time && */ pr_cp > pr_ref_cp) {\
        /*printf("\tMem Extra Budget: %llu\n", (ull) ((pr_cp - pr_ref_cp) *\
        (double)post_cp_pred_mem_access));*/\
        ull mem_extra_budget = (ull) ((pr_cp - pr_ref_cp) *\
        (double)post_cp_pred_mem_access);\
        /*in case millisecond increase is noticed with memory lag*/\
        if((params.exec_cost) < (params.exec_cost + mem_extra_budget)) {\
          params.exec_cost += mem_extra_budget + us2ns(200);\
          printf("mem:budget asked: %llu\n", ns2ms(params.exec_cost));\
          CALL(set_rt_task_param(task_tid, &params));\
        }\
      } else if(0 && cur_time > ref_time) {\
        printf("SHOULD NOT COME HERE\n");\
        double extra;\
        lt_t prev_time, extra_cost;\
        if(EXTRAPOLATE) {\
          extra = ((double) (cur_time - ref_time) / (double) ref_time);\
          prev_time = ns2ms(params.exec_cost);\
          extra_cost = prev_time * (EXTRAPOLATE_FACTOR * extra);\
        } else {\
          extra_cost = ns2ms(cur_time - ref_time); /* just compensating the
          delay*/\
          extra = (double) extra_cost / ns2ms(params.exec_cost);/*this is the percent; only
          for printing*/\
        }\
          /*extra_cost = (prev_time + extra_cost) -
           * ns2ms(params.exec_cost); /*overestimation case*/\
          /*printf("prio:%d, extra: %.2lf, extra cost: %llu\n", params.priority, extra,\
            extra_cost);*/\
          lt_t poten_exec_cost = params.exec_cost + ms2ns(extra_cost);\
          if (extra_cost > 200 && poten_exec_cost < params.period && poten_exec_cost <=\
            params.exec_cost_hi) {\
            printf("%.2lf \%, %llu budget asked\n", extra * 100,\
            ns2ms(poten_exec_cost));\
            params.exec_cost = poten_exec_cost;\
            /*extra_cost_arr[extra_cost_it] = extra_cost +\
              ns2ms(STATIC_KERNELOVERHEAD);*/\
            CALL(set_rt_task_param(task_tid, &params));\
          }\
        /*}/*overestimation case*/\
      }\
    }\
    /*extra_cost_it++;*/\
  }\
  /*lt_t cp_e = litmus_clock();\
  printf("CP:Overhead: %llu\n", ns2us(cp_e - cp_s));*/\
} while(0)

#define ANNOUNCE_TIME(fn, bb) do{\
  static int x_times = 0;\
  /*lt_t cp_s = litmus_clock();*/\
  /*int inter_job_index = litmus_cp->job_index;*/\
  /*printf("SJob:%d, CurJob:%d\n", start_job_index, inter_job_index);*/\
  if (0/*inter_job_index != start_job_index*/) {\
    printf("Very Late\n");\
  } else {\
    lt_t cur_time, rem;\
    /*lt_t sss = litmus_clock();*/\
    get_current_budget(&cur_time, &rem);\
    /*lt_t ssse = litmus_clock();\
    printf("Overhead: %llu\n", ns2us(ssse - sss));*/\
    ull ref_time = CP_##fn##_##bb;\
    /*printf("C:%.2f R:%.2f, lag?%d\n", ns2us(cur_time), ns2us(ref_time), cur_time >\
    ref_time);*/\
    if(cur_time > ref_time) {\
      double extra;\
      lt_t prev_time, extra_cost;\
      if(EXTRAPOLATE) {\
        extra = ((double) (cur_time - ref_time) / (double) ref_time);\
        prev_time = ns2us(params.exec_cost);\
        /*lt_t prev_time = 345/*hardcoding actual CLO time*/;\
        extra_cost = prev_time * (EXTRAPOLATE_FACTOR * extra);\
        /* below is special extra cost for overestimation */\
        /*if(prev_time + extra_cost > ns2ms(params.exec_cost)) { /*overestimation
          case*/\
      } else {\
        extra_cost = ns2ms(cur_time - ref_time); /* just compensating the
        delay*/\
        extra = (double) extra_cost / ns2ms(params.exec_cost);/*this is the percent; only
        for printing*/\
      }\
        /*extra_cost = (prev_time + extra_cost) -
         * ns2ms(params.exec_cost); /*overestimation case*/\
        /*printf("prio:%d, extra: %.2lf, extra cost: %llu\n", params.priority, extra,\
          extra_cost);*/\
        lt_t poten_exec_cost = params.exec_cost + us2ns(extra_cost) +\
          us2ns(200); /*The Call Overhead*/\
        if (extra_cost > 0 && poten_exec_cost < params.period && poten_exec_cost <=\
          params.exec_cost_hi) {\
          /*printf("%.2lf\%, %.2f budget asked:%d\n", extra * 100,\
          ns2us(poten_exec_cost), ++x_times);*/\
          params.exec_cost = poten_exec_cost;\
          /*extra_cost_arr[extra_cost_it] = extra_cost +\
            ns2ms(STATIC_KERNELOVERHEAD);*/\
          CALL(set_rt_task_param(task_tid, &params));\
        }\
      /*}/*overestimation case*/\
    }\
    /*extra_cost_it++;*/\
  }\
  /*lt_t cp_e = litmus_clock();\
  printf("CP:Overhead: %llu\n", ns2us(cp_e - cp_s));*/\
} while(0)


// This function announes current time to the shared memory object created
// for this file
static inline __attribute__((used, always_inline)) void announceTime(int fn_id, int bb_id) {
  ull cur_time = getTime() - start_time;
  long long lead = cur_time; 
  //printf("LagLead: %llu,", CP_TIME(fn_id, bb_id));
  /*msg.fn_id = fn_id;
  msg.bb_id = bb_id;
  msg.cur_time = getTime() - start_time;
  if(write(Progress_Pipe_ID, &msg, sizeof(msg)) < 0) {
    perror("Error in writing to pipe\n");
    exit(-1);
  }*/
}

// this function finishes IPC communication
static inline __attribute__((used, always_inline)) void endIPC() {
  close(Progress_Pipe_ID);
}

#endif
