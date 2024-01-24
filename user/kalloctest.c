#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define NCHILD 2
#define N 100000
#define SZ 4096

void test1(void);
void test2(void);
char buf[SZ];

int
main(int argc, char *argv[])
{
  test1();
  test2();
  exit(0);
}
/*检索和解析通过statistics()函数获得的系统统计信息。
具体来说，它在缓冲区buf中搜索等号'='字符，然后尝试从找到的等号的下一个位置解析出一个整数值。
这个整数值表示的是系统统计数据中某项特定信息的值。*/
int ntas(int print)
{
  int n;
  char *c;

  if (statistics(buf, SZ) <= 0) {
    fprintf(2, "ntas: no stats\n");
  }
  c = strchr(buf, '=');
  n = atoi(c+2);
  if(print)
    printf("%s", buf);
  return n;
}

/*创建了两个子进程，每一个子进程尝试执行N次内存分配和回收。
每次分配的内存大小为4096字节。在所有子进程结束后，
检查内存管理的统计数据是否与预期的增加的次数匹配。
如果统计的数据符合预期，它将打印“test1 OK”，否则，它将打印“test1 FAIL”*/
void test1(void)
{
  void *a, *a1;
  int n, m;
  printf("start test1\n");  
  m = ntas(0);
  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed");
      exit(-1);
    }
    if(pid == 0){
      for(i = 0; i < N; i++) {
        a = sbrk(4096);
        *(int *)(a+4) = 1;
        a1 = sbrk(-4096);
        if (a1 != a + 4096) {
          printf("wrong sbrk\n");
          exit(-1);
        }
      }
      exit(-1);
    }
  }

  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }
  printf("test1 results:\n");
  n = ntas(1);
  if(n-m < 10) 
    printf("test1 OK\n");
  else
    printf("test1 FAIL\n");
}

//
// countfree() from usertests.c
//
int
countfree()
{
  uint64 sz0 = (uint64)sbrk(0);
  int n = 0;

  while(1){
    uint64 a = (uint64) sbrk(4096);
    if(a == 0xffffffffffffffff){
      break;
    }
    // modify the memory to make sure it's really allocated.
    *(char *)(a + 4096 - 1) = 1;
    n += 1;
  }
  sbrk(-((uint64)sbrk(0) - sz0));
  return n;
}

/*确定计算机是否可以分配所有可用的物理内存，
并确保在连续分配和释放内存后，已经释放的内存数量保持不变。
如果测试通过，则打印 "test2 OK"，否则打印"test2 FAIL"*/
void test2() {
  int free0 = countfree();
  int free1;
  int n = (PHYSTOP-KERNBASE)/PGSIZE;
  printf("start test2\n");  
  printf("total free number of pages: %d (out of %d)\n", free0, n);
  if(n - free0 > 1000) {
    printf("test2 FAILED: cannot allocate enough memory");
    exit(-1);
  }
  for (int i = 0; i < 50; i++) {
    free1 = countfree();
    if(i % 10 == 9)
      printf(".");
    if(free1 != free0) {
      printf("test2 FAIL: losing pages\n");
      exit(-1);
    }
  }
  printf("\ntest2 OK\n");  
}


