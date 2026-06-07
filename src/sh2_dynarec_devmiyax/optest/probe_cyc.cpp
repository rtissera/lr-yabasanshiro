#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
// run a fixed straight-line sequence ending in rts; report cycles charged (GET_COUNT)
static void run(const char* nm, const u16* ops, int n){
  initMemory(); DynarecSh2* p=new DynarecSh2(); p->SetCurrentContext();
  u32*g=p->GetGenRegPtr(); for(int i=0;i<16;i++) g[i]=0x06000200+i*4; g[15]=0x06010000;
  u32 a=0x06000000; for(int i=0;i<n;i++){memSetWord(a,ops[i]);a+=2;}
  p->SET_PC(0x06000000); p->SET_COUNT(0);
  for(int k=0;k<40 && p->GET_PC()>=0x06000000 && p->GET_PC()<a;k++) p->Execute();
  printf("%-10s count=%u pc=%08x\n", nm, p->GET_COUNT(), p->GET_PC());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  // straight line: 4x nop, rts, nop
  u16 a1[]={0x0009,0x0009,0x0009,0x0009,0x000b,0x0009}; run("4nop",a1,6);
  // adds + mov.l loads (memory cycles) + rts
  u16 a2[]={0x7101,0x7201,0x6132,0x6232,0x000b,0x0009}; run("add+movl",a2,6);
  // a taken-branch loop: mov #3,r0; (dt r0; bf -) ; rts   -> dt=0x4010, bf back
  u16 a3[]={0xe003,0x4010,0x8bfd,0x000b,0x0009}; run("dtloop",a3,5);
  return 0; }
