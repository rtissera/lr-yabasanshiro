#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void run(u32 r1,u32 sr,int n,u16 op,const char*nm){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[1]=r1; p->SET_SR(sr);
  u32 a=0x06000000; for(int i=0;i<n;i++){memSetWord(a,op);a+=2;}
  memSetWord(a,0x000b);a+=2; memSetWord(a,0x0009);
  p->SET_PC(0x06000000); p->Execute();
  printf("%s x%d r1=%08x sr=%x -> r1=%08x sr=%x\n",nm,n,r1,sr,p->GetGenRegPtr()[1],p->GET_SR());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  run(0x80000001,0x00000000,1,0x4124,"rotcl");
  run(0x80000001,0x00000001,1,0x4124,"rotcl");
  run(0x12345678,0x00000000,4,0x4124,"rotcl");
  run(0x12345678,0x00000000,32,0x4124,"rotcl");
  return 0; }
