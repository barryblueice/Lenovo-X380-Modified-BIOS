#define main full_engine_tests
#include "test_ec.c"
#undef main
int main(void){
  EC_RESULT r;
  fixture();
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_INVALID && !writes && !reads);
  CHECK(EcEraseRow(&io,EC_START)==EC_INVALID && !writes && !reads);
  CHECK(EcProgramRow(&io,EC_START,mEcImage)==EC_INVALID && !writes && !reads);
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_OK);
  CHECK(!erases && !commits && !resetheld && !r.Quiesced && r.ChangedRows==1 && eq(flash,before,EC_SIZE));
  CHECK(!ignoredcommands && !othercommands && readcommands==EC_SIZE);
  CHECK(r.CommandGateEnabled==8 && r.CommandGateRestored==0 && !r.CommandGateHeld);
  fixture();failquiet=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_TIMEOUT);
  CHECK(!erases && !commits && !resetheld && !r.Quiesced && r.EntryStep==5);
  puts("PASS: inspection build rejects apply and direct erase/program with zero I/O; normal and failed inspection never change flash");
  return 0;
}
