#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char* readline() {
    char* buf = malloc(100);
    char* p = buf;//用p作为buf的写指针
    while(read(0, p, 1) != 0){//从标准输入里读一个字符，放进p
        if(*p == '\n' || *p == '\0'){//如果是\n或\0就结束读取
            *p = '\0';
            return buf;
        }
        p++;
    }
    if(p != buf) return buf;//如果用户输入不为空，就返回buf
    free(buf);
    return 0;
}

int
main(int argc, char *argv[]){
    if(argc < 2) {
        printf("Usage: xargs [command]\n");
        exit(-1);
    }
    char* mine_read;
    argv++;//即argv[1]
    char* nargv[16];//新建参数列表nargv
    char** pna = nargv;
    char** pa = argv;//把pa的内容复制到pna
    while(*pa != 0){
        *pna = *pa;
        pna++;
        pa++;
    }
    while((mine_read = readline()) != 0){//从用户输入读取，放入mine_read
        char* p = mine_read;
        char* buf = malloc(36);//缓冲区,用来存放单个参数
        char* bw = buf;//用bh来对buf进行写入
        int nargc = argc - 1;
        while(*p != 0){//遍历readline获得的所有内容
            if(*p == ' ' && buf != bw){//读到空格并且这个单词不为空
                *bw = 0;//结束当前单词
                nargv[nargc] = buf;//将其存储到 nargv 数组的第 nargc 个位置中，然后将 nargc 的值加 1，以便存储下一个命令行参数
                buf = malloc(36);
                bw = buf;
                nargc++;
            }else{
                *bw = *p;//根据p 往buf写入
                bw++;
            }
            p++;
        }
        if(buf != bw){//缓冲区非空
            nargv[nargc] = buf;//把最后一个内容存进nargv
            nargc++;
        }
        nargv[nargc] = 0;//用0标志nargv的结尾
        free(mine_read);//到此，readline获得的内容已处理完毕
        int pid = fork();
        if(pid == 0){//子进程
            exec(nargv[0], nargv);//函数名：nargv[0]，参数列表：nargv
        }else{
            wait(0);//父进程等待子进程结束
        }
    }
    exit(0);
}
