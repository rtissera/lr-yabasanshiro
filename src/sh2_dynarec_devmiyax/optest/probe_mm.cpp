#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
int main(){ yabsys.use_sh2_cache=0;
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  u32*g=p->GetGenRegPtr();
  g[1]=0x06000200; g[2]=0x06000300; g[3]=0x06000400; g[4]=0x06000500; g[5]=0;
  g[6]=0x06000600; g[15]=0x06010000;
  for(u32 a=0x06000200;a<0x06000700;a+=4) memSetLong(a,0xC0DE0000+a);
  // one block, many memory accessors -> exercises overrideMemFunc patching many movabs
  u32 a=0x06000000;
  memSetWord(a,0x6112);a+=2; // mov.l @r1,r1
  memSetWord(a,0x6221);a+=2; // mov.l @r2,r2 (6n2m? 0x62m2 = mov.l @r2,r2) actually 0x6221=mov.l @r2,r1? use simple
  memSetWord(a,0x5403);a+=2; // mov.l @(3,r0?),r4 -> 0x54n3 mov.l @(3*4,r0)?? skip
  memSetWord(a,0x2412);a+=2; // mov.l r1,@r4
  memSetWord(a,0x6532);a+=2; // mov.l @r3,r5
  memSetWord(a,0x2652);a+=2; // mov.l r5,@r6
  memSetWord(a,0x60f2);a+=2; // mov.l @r15,r0
  memSetWord(a,0x000b);a+=2; // rts
  memSetWord(a,0x0009);
  p->SET_PC(0x06000000);
  for(int k=0;k<12 && p->GET_PC()>=0x06000000 && p->GET_PC()<a;k++) p->Execute();
  printf("MM r1=%08x r2=%08x r4=%08x r5=%08x r0=%08x | m200=%08x m500=%08x m600=%08x\n",
    g[1],g[2],g[4],g[5],g[0],memGetLong(0x06000200),memGetLong(0x06000500),memGetLong(0x06000600));
  return 0; }
