#include "/home/common_shared/PAStime/pastime/lib.h"
#include <time.h>

int Progress_Pipe_ID = 0;
ull start_time = 0;
struct timespec t;
FILE *dataFile;
struct Message_Struct msg;
pid_t task_tid;
ull extra_cost_arr[200];
int extra_cost_it = 0;
struct control_page *litmus_cp;
int start_job_index;
lt_t start_release_time;
#ifdef MEM_MODEL
struct rdpmc_ctx load_ctx, store_ctx;
ull start_load_val, start_store_val;
ull end_load_val, end_store_val;
#endif
