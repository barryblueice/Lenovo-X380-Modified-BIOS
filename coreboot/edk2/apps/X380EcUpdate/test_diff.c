#include "ec_diff.h"
extern int puts(const char *);
static UINT8 Current[0x20000],Target[0x20000];
int main(void){
  EC_DIFF D;UINT32 A;
  for(A=0;A<0x20000;A++)Current[A]=Target[A]=(UINT8)(A&255);
  for(A=0x200;A<0x800;A++)Current[A]=Target[A]=0xff;
  for(A=0x200;A<0x600;A++)Current[A]=0;
  EcDescribeRange(Current,Target,0,0xf80,&D);
  if(D.Count!=1024 || D.First!=0x200 || D.Last!=0x5ff || D.FfToZero!=1024 || D.Other)return 1;
  EcDescribeRange(Current,Target,0x600,0x800,&D);
  if(D.Count || D.Zero || D.Ff!=512 || D.First!=0xffffffffU)return 2;
  Current[0x1000]^=1;Current[0x1efff]^=1;Current[0x1f000]^=1;
  EcDescribeRange(Current,Target,0x1000,0x1f000,&D);
  if(D.Count!=2 || D.First!=0x1000 || D.Last!=0x1efff || D.Other!=2)return 3;
  puts("PASS: difference map identifies FF-to-zero padding separately from code changes; range endpoints and no-difference sentinel");
  return 0;
}
