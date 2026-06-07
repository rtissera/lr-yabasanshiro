#include <cstdio>
#include <core.h>
#include "sh2core.h"
#include "debug.h"
#include "yabause.h"
#include "memory_for_test.h"
#include "DynarecSh2.h"
int main(){
  initMemory();
  DynarecSh2* p=new DynarecSh2(); p->SetCurrentContext();
  p->GetGenRegPtr()[0]=0x0600024C; p->GetGenRegPtr()[1]=0xDEADCAFE;
  memSetWord(0x06000246,0x6100);   // MOV.B @R0,R1
  memSetWord(0x06000248,0x000b);   // rts
  memSetWord(0x0600024A,0x0009);   // nop
  memSetLong(0x0600024C,0xFEADCADE);
  printf("memGetByte(24C)=%02x  24D=%02x  24E=%02x  24F=%02x\n",
         memGetByte(0x0600024C),memGetByte(0x0600024D),
         memGetByte(0x0600024E),memGetByte(0x0600024F));
  p->SET_PC(0x06000246); p->Execute();
  printf("R1=%08x (expect FFFFFFAD)\n", p->GetGenRegPtr()[1]);
  return 0;
}
