/* Pure inspection statistics, never a write-eligibility decision. */
#ifndef X380_EC_DIFF_H
#define X380_EC_DIFF_H
#include <Base.h>
typedef struct {
  UINT32 Count,First,Last,Zero,Ff,FfToZero,Other;
} EC_DIFF;
STATIC VOID EcDescribeRange(CONST UINT8 *Current,CONST UINT8 *Target,
                            UINT32 Begin,UINT32 End,EC_DIFF *D) {
  UINT32 A;
  D->Count=D->Zero=D->Ff=D->FfToZero=D->Other=0;
  D->First=D->Last=0xffffffffU;
  for(A=Begin;A<End;A++){
    if(Current[A]==0)D->Zero++;
    if(Current[A]==0xff)D->Ff++;
    if(Current[A]==Target[A])continue;
    if(!D->Count)D->First=A;
    D->Last=A;D->Count++;
    if(Target[A]==0xff && Current[A]==0)D->FfToZero++;
    else D->Other++;
  }
}
#endif
