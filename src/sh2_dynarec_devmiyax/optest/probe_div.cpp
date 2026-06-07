#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void run(u32 r1,u32 r3,u32 r4,u32 sr){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[1]=r1; p->GetGenRegPtr()[3]=r3; p->GetGenRegPtr()[4]=r4; p->SET_SR(sr);
  p->GetGenRegPtr()[15]=0x06010000; // stack for rts
  u32 a=0x06000000;
  for(int i=0;i<32;i++){memSetWord(a,0x3344);a+=2;memSetWord(a,0x4124);a+=2;} // div1 r4,r3; rotcl r1
  memSetWord(a,0x000b);a+=2; memSetWord(a,0x0009);
  p->SET_PC(0x06000000);
  for(int k=0;k<40 && p->GET_PC()>=0x06000000 && p->GET_PC()<a;k++) p->Execute();
  printf("in r1=%08x r3=%08x r4=%08x sr=%08x -> r1=%08x r3=%08x sr=%08x (pc=%08x)\n",
         r1,r3,r4,sr,p->GetGenRegPtr()[1],p->GetGenRegPtr()[3],p->GET_SR(),p->GET_PC());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  run(0x00010000,0,0x00000028,0);
  run(0x12345678,0,0x00000007,0);
  run(0xFFFF0000,0xFFFFFFFF,0x00000100,0x00000001);
  run(0x0000d680,0,0x00000453,0);
  return 0; }
