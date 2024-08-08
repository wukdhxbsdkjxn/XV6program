#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]){
    int p2c[2];
    int c2p[2];//存储管道的读写文件描述符
    //创建两个管道。pipe 函数会创建一个管道，成功时返回0，失败时返回-1
    if(pipe(p2c) < 0){
        printf("pipe");//如果创建管道失败，程序会打印 "pipe" 并退出。
        exit(-1);
    }
    if(pipe(c2p) < 0){
        printf("pipe");
        exit(-1);
    }
    int pid = fork();
    if(pid == 0){
        // child
        char buf[10];
        read(p2c[0], buf, 10);//从管道 p2c 读到buf
        printf("%d: received ping\n", getpid());
        write(c2p[1], "o", 2);//将字符串 "o" 写入到管道 c2p 中
    }else if(pid > 0){
        // parent
        write(p2c[1], "p", 2);//将字符串 "p" 写入到管道 p2c 中
        char buf[10];
        read(c2p[0], buf, 10);
        printf("%d: received pong\n", getpid());
    }
    //关闭管道的读写文件描述符,释放系统资源
    close(p2c[0]);
    close(p2c[1]);
    close(c2p[0]);
    close(c2p[1]);
    exit(0);
}
