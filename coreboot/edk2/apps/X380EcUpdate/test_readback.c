#define main model_tests
#include "test_ec.c"
#undef main
#include "readback_fixture.h"
static VOID actual_fixture(VOID){
  fixture();copy(flash,mReadback,EC_SIZE);copy(before,flash,EC_SIZE);reg[0xfe9e]=0;
}
int main(void){
  EC_RESULT r;UINT32 k;
  actual_fixture();
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_OK);
  CHECK(!r.BootMismatchCount && r.PreservedDataDifferences==1024 && r.ChangedRows==3);
  CHECK(r.ReadPasses==1);
  CHECK(!erases && !commits && !othercommands && !r.Quiesced && !r.CommandGateHeld);
  CHECK(eq(flash,mReadback,EC_SIZE));
  actual_fixture();
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK);
  CHECK(erases==3 && commits==3 && r.ProgrammedRows==3 && !violations);
  CHECK(r.ReadPasses==2 && !r.PrepareActive);
  CHECK(eq(flash,mReadback,EC_START) && eq(flash+EC_END,mReadback+EC_END,EC_SIZE-EC_END));
  CHECK(eq(flash+EC_START,mEcImage+EC_START,EC_END-EC_START));
  CHECK(!r.HeldReset && !r.Quiesced && !r.CommandGateHeld && !reg[0xfead]);
  erases=commits=0;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK && !r.ChangedRows && !erases && !commits);
  actual_fixture();change_on_resume=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK);
  CHECK(flash[0x1f123]==(UINT8)(mReadback[0x1f123]^1) && r.ReadPasses==2);
  actual_fixture();change_on_resume=2;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_BOOTBLOCK);
  CHECK(!erases && !commits && !r.HeldReset && !r.PrepareActive && r.ReadPasses==2);
  for(k=0;k<3;k++){
    CONST UINT32 codeaddr[]={0x10,0x880,0xf90};
    actual_fixture();flash[codeaddr[k]]^=1;
    CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_BOOTBLOCK);
    CHECK(r.BootMismatchCount==1 && !erases && !commits);
  }
  actual_fixture();corrupt=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_VERIFY && r.HeldReset && r.EraseStarted);
  CHECK(erases==1 && commits==1 && !r.ProgrammedRows);
  puts("PASS: real EC dump: inspect accepts preserved data; exactly three rows updated; complete reference matches; re-run skips; boot code corruption rejected; verify failure holds reset");
  return 0;
}
