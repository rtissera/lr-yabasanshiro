#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void run(u32 memval){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[0]=0x06000200; p->GetGenRegPtr()[15]=0x06010000;
  p->SET_SR(0x000000F0);
  memSetLong(0x06000200, memval);
  memSetWord(0x06000000,0x4007);  // ldc.l @r0+,SR
  memSetWord(0x06000002,0x000b);  // rts
  memSetWord(0x06000004,0x0009);
  p->SET_PC(0x06000000);
  for(int k=0;k<6 && p->GET_PC()>=0x06000000 && p->GET_PC()<0x06000006;k++) p->Execute();
  printf("ldc.l mem=%08x -> SR=%08x R0=%08x\n", memval, p->GET_SR(), p->GetGenRegPtr()[0]);
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  run(0x000003F3); run(0xFFFFFFFF); run(0x12345678); run(0x000000bc);
  return 0; }
