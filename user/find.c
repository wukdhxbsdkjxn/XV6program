#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void match(const char* path, const char* name){//判断path中是否有与name匹配的子串
    int pp = 0;//指向path
    int pn = 0;//指向name
    while(path[pp] != 0){
        pn = 0;//从name的开头开始比较
        int tmpp = pp;//从path的当前位置开始往后比较
        while(name[pn] != 0){
            if (name[pn] == path[tmpp]){
                pn++;
                tmpp++;
            }
            else//不断比较，直到name到最后或出现不匹配的字符
                break;
        }
        if(name[pn] == 0){//如果找到了与name完全一样的部分
            printf("%s\n", path);//把path输出
            return;
        }
        pp++;
    }
}

void find(char *path, char *name){
    char buf[512], *p;
    int fd;
    struct dirent de;//目录项结构体
    struct stat st;

    if((fd = open(path, 0)) < 0){//检测能否打开path，如果能，把文件描述符存在md
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }
    
    if(fstat(fd, &st) < 0){//检测能否获取文件的state，如果能，存在st
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);//如果读取状态失败，需关闭文件
        return;
    }
    switch(st.type){//判断文件类型
        case T_FILE://文件类型
            match(path,name);
            break;

        case T_DIR://文件夹
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);//把目录路径加入buf
            p = buf+strlen(buf);
            *p++ = '/';//在路径末尾加上'/'
            while(read(fd, &de, sizeof(de)) == sizeof(de)){//de包含了文件信息。
            //这一步是从打开的目录文件中读取文件信息，并将其保存在de变量中，sizeof(de)是文件大小，如果读取成功则==
                if(de.inum == 0)
                    continue;
                if(de.name[0] == '.' && de.name[1] == 0) continue;//如果是'.'或'..'目录，不用检查
                if(de.name[0] == '.' && de.name[1] == '.' && de.name[2] == 0) continue;
                memmove(p, de.name, DIRSIZ);//把文件名复制到p指针之后，得到了目录下的一个路径
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){//获取新路径的状态信息
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, name);//在新路径下继续寻找name
            }
            break;
    }
    close(fd);
}

int
main(int argc, char *argv[]){
    if (argc < 3){//检查参数设置是否正确
        printf("Usage: find [path] [filename]\n");
        exit(-1);
    }
    find(argv[1], argv[2]);//argv[0]存的是函数名称
    exit(0);
}
