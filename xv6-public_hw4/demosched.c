#include "types.h"
#include "stat.h"
#include "user.h"


int factorial() {
    long long int fact = 1;
    // for (int j = 1 ; j <= 100000000 ; j ++ ) {
    //     for (int p = 1 ; p <= 1000000000 ; p ++ ) {
    //         fact = p*p*j;
    //         // p = p*j;
    //         fact = p*j;
            
    //     }

    // }
    for (int j = 1 ; j <= 100000000 ; j ++ ) {
        fact = j*fact;
    }
    // printf(1, "%d", fact);

        
    return fact;
}

int whileloop() {
    long long int cnt = 0;
    while (cnt != 1000000000000) {
        long long int p = cnt*cnt*cnt;
        if (p == 2) {
            break;
        }
        cnt ++;
    }
    // printf(1, "cnt =  %d\n", cnt);

    return cnt;
}

// the function creates 16 children and prioritize them, that way the prioritizing 
// mechanism could be verified
// there are 8 priority levels, creating 16 children such that there would be pairs of
// priority level, thus it could be verified that the prioritizing indeed workds
// each child would perform the same calculation
int main (int argc, char *argv[]) {
    int pid = 0;
    
    int childrenNum = 16;

    setprio(7);
    for (int i = 0 ; i < childrenNum ; i++ ) {
        pid = fork();

        if (pid < 0 ) {
            printf(1, "failed to create fork for process index %d\n", i);
        }
        // child 
        else if ( pid == 0) {
            int priority = i%8;
            if (setprio(priority) != 0) {
                printf(1, "couldn't set priority to child index %d", i);
                break;
            }
            
            // printf(1, "child %d created with pid %d\n", i, getpid());
            int calcStartTime = uptime();
            int fact = factorial();
            if (fact == 2){ // for some reason without it, it maybe ignore the function, guess it's an optimization of the compiler because there is no use of the return value
                break;
            }
            int calcTime = uptime() - calcStartTime;
            printf(1, "child index\tpriority level\tcalc duration[ms]\n%d\t\t%d\t\t%d\n", i, getprio(), calcTime);

            break;

        } 

    }
    for (int k = 0 ; k < childrenNum ; k++ ) {
        wait();
    }
    exit();
}