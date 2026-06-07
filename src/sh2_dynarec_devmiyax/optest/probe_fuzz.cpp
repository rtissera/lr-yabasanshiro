#include <cstdio>
#include <cstring>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
extern yabsys_struct yabsys;
// Run one opcode with a known distinct register file; dump architectural state.
static void snap(u16 op, unsigned long long *out){
  initMemory(); DynarecSh2*p=new DynarecSh2(); p->SetCurrentContext();
  u32 *g=p->GetGenRegPtr();
  for(int i=0;i<16;i++) g[i]=0x06000100 + i*0x40;   // distinct, WRAM-ish, aligned-ish
  g[15]=0x06008000;                                  // stack
  p->SET_SR(0x000000F1); p->SET_GBR(0x06000200); p->SET_VBR(0);
  p->SET_MACH(0x89ABCDEF); p->SET_MACL(0x12345678); p->SET_PR(0x06007000);
  memSetWord(0x06000000, op);
  memSetWord(0x06000002, 0x0009); // nop
  memSetWord(0x06000004, 0x0009);
  // scratch data area for mem ops
  for(u32 a=0x06000100;a<0x06000900;a+=4) memSetLong(a,0xA5A50000+a);
  p->SET_PC(0x06000000);
  p->Execute();                       // run the single block (one opcode + nop/nop)
  int k=0;
  for(int i=0;i<16;i++) out[k++]=g[i];
  out[k++]=p->GET_SR(); out[k++]=p->GET_MACH(); out[k++]=p->GET_MACL(); out[k++]=p->GET_PR();
  // also a window of scratch memory (catch stores)
  u32 c; for(u32 a=0x06000100;a<0x06000300;a+=4) out[k++]=MappedMemoryReadLong(a,&c);
  delete p;
}
int main(int argc,char**argv){
  yabsys.use_sh2_cache=0;
  // ranges with two-register / flag-bearing opcodes
  unsigned ranges[][2]={{0x0000,0x1000},{0x2000,0x7000},{0x7000,0x10000}};
  for(auto&r:ranges) for(unsigned op=r[0];op<r[1];op++){
    unsigned long long o[148]; snap((u16)op,o);
    // print as one line: op + all values
    printf("%04x",op); for(int i=0;i<148;i++) printf(" %llx",o[i]); printf("\n");
  }
  return 0;
}
