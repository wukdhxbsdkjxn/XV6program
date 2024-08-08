#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])//argc表示参数个数，argv是参数数组
{
  if(argc < 2){//如果参数不足2，说明函数使用错误
    fprintf(2, "Usage: sleep [time]\n");//2表示标准错误流
    exit(1);
  }

  int time = atoi(argv[1]);//提取要停顿的时间
  sleep(time);
  exit(0);//注意是exit而不是return
}

