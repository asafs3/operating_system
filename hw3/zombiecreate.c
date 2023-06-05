#include "types.h"
#include "stat.h"
#include "user.h"

int main(){
    printf(1, "printingggg\n");
    int pid = fork();
    if(pid < 0) {
        printf(1, "zombiefailed\n");
        exit(); 
        }
    if(pid == 0){
        printf(1, "child exit\n");
        exit();
        }
    else {
        printf(1, "father sleep\n");
        sleep(1000); 
        printf(1, "father wake\n");
        wait();
    }
    exit();
    
}