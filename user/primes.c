#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int rd){//rd是管道的读取端
    int n;
    read(rd, &n, 4);//把读取的内容放进n
    printf("prime %d\n", n);//输出当前的n
    int created = 0;//是否已创建管道
    int p[2];
    int num;
    while(read(rd, &num, 4) != 0){//用num存管道里在n之后的数
        if(created == 0){
            pipe(p);//创建对应于n的管道
            created = 1;
            int pid = fork();//创建子进程
            if(pid == 0){//子进程
                close(p[1]);
                prime(p[0]);//递归，判断子管道里的内容是不是质数
                return;
            }else{//当前进程
                close(p[0]);//关闭读取，允许写入
            }
        }
        if(num % n != 0){//如果num不是n的倍数，则它有可能是质数
            write(p[1], &num, 4);//把它写入子管道
        }
    }
    close(rd);
    close(p[1]);
    wait(0);//等待子进程结束
}

int
main(int argc, char *argv[]){
    int p[2];
    pipe(p);//创建管道p

    int pid = fork();
    if(pid != 0){
        //父进程
        close(p[0]);//关闭p的读取。只有读取端关闭，才能进行写入
        for(int i = 2; i <= 35; i++){
            write(p[1], &i, 4);//写入
        }
        close(p[1]);//关闭p的写入
        wait(0);//等待子进程结束
    }else{//子进程
        close(p[1]);
        prime(p[0]);
        close(p[0]);
    }
    exit(0);
}
