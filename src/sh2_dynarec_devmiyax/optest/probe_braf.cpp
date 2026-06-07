#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void run(u32 r1,u32 r4){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[1]=r1; p->GetGenRegPtr()[4]=r4; p->GetGenRegPtr()[15]=0x06010000;
  u32 a=0x06000000;
  memSetWord(a,0x0423); a+=2;          // braf r4   (target = a+4 + r4)
  memSetWord(a,0x0009); a+=2;          // nop (delay slot)
  for(int i=0;i<14;i++){ memSetWord(a,0x4121); a+=2; } // shar r1 sled
  memSetWord(a,0x000b); a+=2;          // rts
  memSetWord(a,0x0009);                // nop
  p->SET_PC(0x06000000);
  for(int k=0;k<10 && p->GET_PC()>=0x06000000 && p->GET_PC()<a;k++) p->Execute();
  printf("braf r1=%08x r4=%u -> r1=%08x pc=%08x\n", r1,r4,p->GetGenRegPtr()[1],p->GET_PC());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  run(0x10000000, 0);
  run(0x10000000, 8);
  run(0x10000000, 16);
  run(0x7fffffff, 12);
  return 0; }
