#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
static void dump(const char*nm, const u16*ops,int n){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  u32*g=p->GetGenRegPtr(); for(int i=0;i<16;i++) g[i]=0x100+i; g[15]=0x06010000;
  u32 a=0x06000000; for(int i=0;i<n;i++){memSetWord(a,ops[i]);a+=2;}
  p->SET_PC(0x06000000);
  for(int k=0;k<12 && p->GET_PC()>=0x06000000 && p->GET_PC()<0x06000000+n*2;k++) p->Execute();
  printf("%-8s r3=%x r5=%x r6=%x r7=%x pr=%08x pc=%08x\n",
    nm,g[3],g[5],g[6],g[7],p->GET_PR(),p->GET_PC());
  delete p;
}
int main(){ yabsys.use_sh2_cache=0;
  u16 a1[]={0x0008,0x8b01,0xe342,0x0009,0x0009,0x000b,0x0009}; dump("bfs",a1,7);
  u16 a2[]={0x0018,0x8901,0xe342,0x0009,0x0009,0x000b,0x0009}; dump("bts",a2,7);
  u16 a3[]={0xa002,0x7501,0x0009,0x0009,0x000b,0x0009,0x0009}; dump("bra_ds",a3,7);
  u16 a4[]={0xb002,0x7601,0x0009,0x0009,0x000b,0x0009,0x0009}; dump("bsr_ds",a4,7);
  u16 a5[]={0x0003,0x7701,0x0009,0x0009,0x000b,0x0009,0x0009}; dump("bsrf_ds",a5,7); // bsrf r0(delay add#1,r7)
  return 0; }
