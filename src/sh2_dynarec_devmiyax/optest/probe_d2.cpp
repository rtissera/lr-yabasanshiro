#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void run(int n){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[1]=0x00010000; p->GetGenRegPtr()[3]=0; p->GetGenRegPtr()[4]=0x28; p->SET_SR(0);
  u32 a=0x06000000; for(int i=0;i<n;i++){memSetWord(a,0x3344);a+=2;memSetWord(a,0x4124);a+=2;}
  memSetWord(a,0x000b);a+=2; memSetWord(a,0x0009);
  p->SET_PC(0x06000000); p->Execute();
  printf("N=%2d r1=%08x r3=%08x sr=%x\n",n,p->GetGenRegPtr()[1],p->GetGenRegPtr()[3],p->GET_SR());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0; for(int n=7;n<=20;n++) run(n); return 0; }
