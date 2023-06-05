#include "types.h"
#include "stat.h"
#include "user.h"

int func(){
    int sum = 0;
    while (sum != 55555) {
        double pieCalc;
        for (int z = 0; z < 55555555; z++) {
            pieCalc = 3.14*5.5;
            sum = sum + pieCalc;   // useless calculations to consume CPU time
        }
        sum = sum - 388888880;
        sum = sum/ 10000;
    }
    return sum;
}
int factorial(int n) {
    int fact = 1;
    for (int j = 1 ; j <= 50 ; j ++ ) {
        fact *= j;
    }
    return fact;
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
            func();
            
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