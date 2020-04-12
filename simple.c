#include <stdio.h>
#include <stdlib.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <jevents.h>
#include <perf-iter.h>
#include <rdpmc.h>

#include <litmus.h>
#include <litmus/rt_param.h>
#include <lib.h>

struct rdpmc_ctx ctx;

/*static inline __attribute__((used, always_inline)) void setup_perf_counter() {
    printf("Entering PERF Counter Setup.\n");
    struct perf_event_attr attri;

    if (resolve_event("mem_inst_retired.all_stores", &attri) < 0) {
    //if (resolve_event("mem-stores", &attri) < 0) {
        printf("Error in resolve_event");
        exit(-1);
    }

    if (rdpmc_open_attr(&attri, &ctx, NULL) < 0) {
        printf("Error in rdpmc_open_attr");
        exit(-1);
    }
    printf("PERF Counter is set up.\n");
}

#define CALL( exp ) do { \
  int ret; \
  ret = exp; \
  if (ret != 0) \
    fprintf(stderr, "%s failed: %m\n", #exp); \
  else \
    fprintf(stderr, "%s ok.\n", #exp); \
} while(0)
*/
struct rt_task params;
void init_rt_props(lt_t exec_cost, lt_t exec_cost_hi, lt_t period, int prio,
lt_t r_lo, lt_t r_star, pid_t tid) {
#define PARTITION 1
    CALL(init_litmus());

    params.exec_cost = ms2ns(exec_cost); //1 - exec
    params.exec_cost_hi = ms2ns(exec_cost_hi); //7 - exec
    params.period = ms2ns(period); //2 - period
    params.relative_deadline = params.period;
    params.cpu = domain_to_first_cpu(PARTITION);
    params.priority = prio; //3 - prio
    params.r_lo = ms2ns(r_lo);//4 - r_lo
    params.r_star = ms2ns(r_star);//6 - r_star
    params.budget_policy = PRECISE_ENFORCEMENT;
    params.cls = RT_CLASS_HARD;
    params.exec_cost_crit[0] = -1;
    params.exec_cost_crit[1] = -1;
  
    CALL(set_rt_task_param(tid, &params));
    CALL(be_migrate_to_domain(PARTITION));

    CALL(task_mode(LITMUS_RT_TASK));
}

int main() {
  //setup_perf_counter();  
  //init_rt_props();
  configureIPC();
  return 0;
}
