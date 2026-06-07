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
  // at 0x06000000: mov.l @(0x02,pc),r0 ; rts ; nop ; <pad> ; const@0x06000010
  // addr = (0x06000000+4)&~3 + 2*4 = 0x06000004+8 = 0x0600000C
  memSetWord(0x06000000,0xD002);   // mov.l @(2,pc),r0
  memSetWord(0x06000002,0x000b);   // rts
  memSetWord(0x06000004,0x0009);   // nop
  memSetLong(0x0600000C,0x00010000);
  p->GetGenRegPtr()[15]=0x06010000;
  p->SET_PC(0x06000000);
  for(int k=0;k<8 && p->GET_PC()>=0x06000000 && p->GET_PC()<0x06000008;k++) p->Execute();
  printf("MOVLI r0=%08x (expect 00010000) pc=%08x\n", p->GetGenRegPtr()[0], p->GET_PC());
  // also MOVWI: mov.w @(disp,pc),r0 sign-extended
  initMemory(); DynarecSh2*q=new DynarecSh2(); q->SetCurrentContext();
  memSetWord(0x06000000,0x9002);   // mov.w @(2,pc),r0 ; addr=(0x4)+2*2=0x06000008
  memSetWord(0x06000002,0x000b);
  memSetWord(0x06000004,0x0009);
  memSetWord(0x06000008,0x8123);
  q->GetGenRegPtr()[15]=0x06010000; q->SET_PC(0x06000000);
  for(int k=0;k<8 && q->GET_PC()>=0x06000000 && q->GET_PC()<0x06000008;k++) q->Execute();
  printf("MOVWI r0=%08x (expect ffff8123) pc=%08x\n", q->GetGenRegPtr()[0], q->GET_PC());
  return 0; }
