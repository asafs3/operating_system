#include "types.h"
#include "stat.h"
#include "user.h"


// the function translates the state index from the processInfo struct to its corresponding string
// the mapping is done according to proc.h : enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

char*
translate_state(int state_idx) {
    switch (state_idx)
    {
    case 0:
        return "UNUSED";
    case 1:
        return "EMBRYO";
    case 2:
        return "SLEEPING";
    case 3:
        return "RUNNABLE";
    case 4:
        return "RUNNING";
    case 5:
        return "ZOMBIE";    
    default:
        return "UNUSED";
    }

}


int
main()
{   
    struct processInfo info;
    int procNum = getNumProc();
    int maxPid = getMaxPid();
    printf(1, "Total number of active processes: %d\n", procNum);
    printf(1, "Maximum PID: %d\n\n", maxPid);
    printf(1, "PID\tSTATE\t\tPPID\tSZ\tNFD\tNRSWITCH\n");

    // the pids are incrementing from 1 to maxPid
    for (int pid = 1 ; pid <= maxPid ; pid ++) {
        // check if processInfo could be achieved
        if (getProcInfo(pid, &info) == 0) {
            printf(1,"%d\t%s  \t%d\t%d\t%d\t%d\t\n", pid, translate_state(info.state), info.ppid, info.sz, info.nfd, info.nrswitch);
        }  
  }
  exit();

}
