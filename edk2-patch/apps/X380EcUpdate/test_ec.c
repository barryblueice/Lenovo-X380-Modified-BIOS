/* Host-only model of the indexed flash controller, not included in INF. */
#include "ec_flash.h"
#include "ec_image.h"
extern int puts(const char *);
#define CHECK(x) do {if(!(x)){puts("FAIL: " #x);return 1;}}while(0)
static UINT8 flash[EC_SIZE],before[EC_SIZE],current[EC_SIZE],reg[65536],page[128];
static UINT16 indexreg;
static UINT32 reads,writes,erases,commits,pending_addr;
static UINT8 cmd,response,obf;
static int power,failquiet,failhalt,corrupt,stuck,alias,violations,failecho,deadbus,failresume;
static UINT32 indexedwrites;
static UINT32 ignoredcommands,readcommands,othercommands;
static int denyenable,denyrestore;
static int busy_after_f2,after_f2,block_halt_step,halt_phase;
static int quiet_mode,frozen_command,change_on_resume;
static BOOLEAN resetheld,programming;
static VOID copy(UINT8 *a,CONST UINT8 *b,UINTN n){UINTN k;for(k=0;k<n;k++)a[k]=b[k];}
static int eq(CONST UINT8 *a,CONST UINT8 *b,UINTN n){UINTN k;for(k=0;k<n;k++)if(a[k]!=b[k])return 0;return 1;}
static UINT32 addr(VOID){return ((UINT32)reg[0xfeaa]<<16)|((UINT32)reg[0xfea9]<<8)|reg[0xfea8];}
static UINT8 readport(VOID *c,UINT16 port){
  (VOID)c;reads++;
  if(port==0x64)return 0;
  if(port==0x66){
    if(frozen_command)return 2;
    if(busy_after_f2 && after_f2)return 2;
    if(block_halt_step && readcommands>=EC_SIZE && block_halt_step==halt_phase+1)return 2;
    return obf;
  }
  if(port==0x62){obf=0;return response;}
  if(port==0x705)return failecho?0xff:(UINT8)(indexreg>>8);
  if(port==0x706)return failecho?0xff:(UINT8)indexreg;
  if(port==0x707){if(deadbus)return deadbus==1?0xff:0;
    if(indexreg==0xfead && stuck && erases)return 2;return reg[indexreg];}
  return 0xff;
}
static VOID writeport(VOID *c,UINT16 port,UINT8 v){
  UINT32 a,k;(VOID)c;writes++;
  if(port==0x705){indexreg=(indexreg&0xff)|((UINT16)v<<8);return;}
  if(port==0x706){indexreg=(indexreg&0xff00)|v;return;}
  if(port==0x66){
    if(quiet_mode){frozen_command=1;return;}
    cmd=v;if(readcommands>=EC_SIZE)halt_phase++;return;
  }
  if(port==0x62){
    if(readcommands>=EC_SIZE && cmd==0x59)halt_phase++;
    if(cmd==0x59 && v==0xf2)after_f2=1;
    if(cmd==0x80){response=v==0x46?(power?0x10:0):0x80;obf=1;}
    else if(cmd==0x59 && v==0xf1 && !failquiet){reg[0xf554]=0x33;quiet_mode=1;}
    else if(cmd==0x59 && v==0xf2 && !failhalt)reg[0xf554]=0xa5;
    return;
  }
  if(port!=0x707)return;
  indexedwrites++;
  if(indexreg==0xfead){
    if(denyenable && (v&8))return;
    if(denyrestore && !(v&8) && (reg[indexreg]&8))return;
  }
  /* EFCFG bit 3 gates ALL EFCMD writes, including READ, not only erase.
   * Disabled commands leave the old EFDAT value available for host reads. */
  if(indexreg==0xfeac && !(reg[0xfead]&8)){ignoredcommands++;return;}
  reg[indexreg]=v;
  if(indexreg==0xff10 && v==0xff && !failresume){
    reg[0xf554]=0;after_f2=0;quiet_mode=0;frozen_command=0;
    if(change_on_resume){flash[change_on_resume==1?0x1f123:0x100]^=1;change_on_resume=0;}
  }
  if(indexreg==0xff14){resetheld=(v&1)!=0;if(!resetheld)reg[0xf554]=0;}
  if(indexreg!=0xfeac)return;
  a=addr();if(a>=EC_SIZE){violations++;return;}
  if(v==3){readcommands++;reg[0xfeab]=flash[alias?(a&0xffff):a];return;}
  othercommands++;
  if(v==0x80){programming=TRUE;for(k=0;k<128;k++)page[k]=0xff;return;}
  if(v==2){if(!programming)violations++;page[a&127]=reg[0xfeab];pending_addr=a&~127U;return;}
  if(v==0x20 || v==0x70){
    if(a<EC_START || a>=EC_END || !resetheld || !(reg[0xfead]&8))violations++;
    if(v==0x20){erases++;for(k=0;k<128;k++)flash[(a&~127U)+k]=0xff;}
    else {commits++;programming=FALSE;for(k=0;k<128;k++)flash[pending_addr+k]&=page[k];
      if(corrupt)flash[pending_addr]^=1;}
  }
}
static VOID delay(VOID *c,UINT32 us){(VOID)c;(VOID)us;}
static EC_IO io={0,readport,writeport,delay};
static VOID fixture(VOID){
  UINT32 k;for(k=0;k<65536;k++)reg[k]=0;
  copy(flash,mEcImage,EC_SIZE);flash[0x1234]^=0x80;
  /* Personal state must survive, even when vendor image contains ff. */
  for(k=0x1f000;k<EC_SIZE;k++)flash[k]=(UINT8)(k*7);
  copy(before,flash,EC_SIZE);indexreg=0;reads=writes=erases=commits=pending_addr=0;
  cmd=response=obf=0;power=1;failquiet=failhalt=corrupt=stuck=alias=violations=0;
  resetheld=programming=FALSE;reg[0xfe9e]=0x10;
  failecho=deadbus=failresume=0;indexedwrites=0;
  reg[0xfe90]=0x20;reg[0xfe92]=7;reg[0xfe93]=0;reg[0xfe95]=0x7d;
  reg[0xff00]=0xc0;reg[0xff02]=0x2f;reg[0xff80]=0xff;
  reg[0xfeab]=0x12;ignoredcommands=readcommands=othercommands=0;denyenable=denyrestore=0;
  busy_after_f2=after_f2=block_halt_step=halt_phase=0;
  quiet_mode=frozen_command=change_on_resume=0;
}
int main(void){
  EC_RESULT r;int ret;UINT8 row[128];UINT32 k;
  CONST UINT16 configs[]={0xfe90,0xfe92,0xfe93,0xfe95,0xff00,0xff02};
  fixture();writeport(0,0x66,0x59);writeport(0,0x62,0xf1);
  CHECK(quiet_mode && readport(0,0x66)==0);
  writeport(0,0x66,0x59);CHECK(frozen_command && readport(0,0x66)==2);
  /* Now every normal update is tested against that stricter read-mode model. */
  for(k=1;k<=4;k++){
    fixture();block_halt_step=k;
    CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_TIMEOUT);
    CHECK(r.HaltStep==k && r.HaltPortStatus==2 && r.HaltWaitUs==600000);
    CHECK(!erases && !commits && !r.HeldReset && !r.Quiesced && !r.CommandGateHeld);
  }
  fixture();busy_after_f2=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK);
  CHECK(r.HaltStep==7 && r.HaltHandshake==0xa5 && erases==1 && commits==1);
  fixture();busy_after_f2=1;failhalt=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_TIMEOUT);
  CHECK(r.HaltStep==5 && r.HaltHandshake==0 && r.HaltPortStatus==2);
  CHECK(!erases && !commits && !r.HeldReset && !r.Quiesced && !r.CommandGateHeld);
  /* Reproduce the photographed constant-data failure with the old sequence:
   * the row primitive alone does not enable the command gate. */
  fixture();
  for(k=0;k<EC_SIZE;k+=EC_ROW)CHECK(EcReadRow(&io,k,current+k)==EC_OK);
  for(k=0;k<EC_SIZE;k++)CHECK(current[k]==0x12);
  CHECK(ignoredcommands==EC_SIZE && !readcommands && !erases && !commits);
  fixture();denyenable=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM);
  CHECK(r.Stage==ST_READ && !r.CommandGateHeld && !r.Quiesced && !readcommands && !othercommands);
  fixture();denyrestore=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM);
  CHECK(r.CleanupResult==EC_PLATFORM && r.CommandGateHeld && r.Quiesced && !erases && !commits);
  fixture();reg[0xfead]=8;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_OK);
  CHECK(r.CommandGateBefore==8 && r.CommandGateEnabled==8 && r.CommandGateRestored==8 && reg[0xfead]==8 && !r.CommandGateHeld);
  for(k=0;k<6;k++){
    fixture();reg[configs[k]]^=1;
    CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM);
    CHECK(r.EntryStep==2 && r.ProbeAddress==configs[k] && !indexedwrites && !erases && !r.Quiesced);
  }
  fixture();failecho=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  for(k=1;k<=2;k++){
    fixture();deadbus=k;
    CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  }
  fixture();reg[0xfead]=0xff;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  fixture();reg[0xfead]=2;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  fixture();reg[0xf554]=0x33;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  fixture();reg[0xff14]=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_PLATFORM && !indexedwrites);
  fixture();failquiet=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_TIMEOUT);
  CHECK(r.EntryStep==5 && r.LastStatus==0 && !r.Quiesced && !erases && !commits && !resetheld && reg[0xff80]==0xff);
  fixture();failresume=1;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_TIMEOUT);
  CHECK(r.CleanupResult==EC_TIMEOUT && r.Quiesced && !erases && !commits && !resetheld);
  fixture();CHECK(EcUpdate(&io,mEcImage,1,current,TRUE,&r)==EC_INVALID && writes==0 && reads==0);
  CHECK(EcEraseRow(&io,0)==EC_INVALID && writes==0);
  CHECK(EcEraseRow(&io,EC_END)==EC_INVALID && writes==0);
  CHECK(EcProgramRow(&io,EC_END,mEcImage)==EC_INVALID && writes==0);
  CHECK(EcReadRow(&io,EC_SIZE,row)==EC_INVALID && writes==0);
  fixture();power=0;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_POWER && !erases && !r.Quiesced);
  fixture();failquiet=1;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_TIMEOUT && !erases && !r.Quiesced);
  fixture();flash[0x80]^=1;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_BOOTBLOCK && !erases && !r.Quiesced);
  CHECK(r.CompatFlags==1 && r.BootMismatchCount==1 && r.FirstBootMismatch==0x80);
  CHECK(r.FirstExpected==mEcImage[0x80] && r.FirstActual==(UINT8)(mEcImage[0x80]^1));
  fixture();alias=1;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_BOOTBLOCK && !erases);
  CHECK(r.CompatFlags==2 && !r.HalfDiffBytes && !r.BootMismatchCount);
  fixture();for(k=0;k<EC_SIZE;k++)flash[k]=0;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_BOOTBLOCK && !erases && !commits && !r.Quiesced);
  CHECK(r.CompatFlags==3 && r.ZeroBytes==EC_SIZE && !r.FfBytes && !r.HalfDiffBytes);
  fixture();for(k=0;k<EC_SIZE;k++)flash[k]=0xff;
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_BOOTBLOCK && !erases && !commits && !r.Quiesced);
  CHECK(r.CompatFlags==3 && r.FfBytes==EC_SIZE && !r.ZeroBytes && !r.HalfDiffBytes);
  fixture();CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,FALSE,&r)==EC_OK);
  CHECK(!erases && !commits && r.ChangedRows==1 && eq(flash,before,EC_SIZE) && !r.Quiesced);
  CHECK(r.EntryStep==6 && reg[0xff80]==0xff && !resetheld);
  CHECK(r.CommandGateBefore==0 && r.CommandGateEnabled==8 && r.CommandGateRestored==0 && !r.CommandGateHeld);
  CHECK(!reg[0xfead] && !ignoredcommands && readcommands==EC_SIZE && !othercommands);
  CHECK(!r.CompatFlags && !r.BootMismatchCount && r.FirstBootMismatch==0xffffffffU && r.HalfDiffBytes);
  fixture();copy(flash+EC_START,mEcImage+EC_START,EC_END-EC_START);
  CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK && !erases && !r.ChangedRows);
  fixture();failhalt=1;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_TIMEOUT && !erases && !resetheld);
  fixture();CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK);
  CHECK(erases==1 && commits==1 && !violations && r.ProgrammedRows==1 && !resetheld && !r.Quiesced);
  CHECK(eq(flash,before,EC_START) && eq(flash+EC_END,before+EC_END,EC_SIZE-EC_END));
  CHECK(eq(flash+EC_START,mEcImage+EC_START,EC_END-EC_START));
  fixture();reg[0xfe9e]=0;CHECK(EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r)==EC_OK && !violations);
  fixture();corrupt=1;ret=EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r);
  CHECK(ret==EC_VERIFY && erases==1 && commits==1 && r.HeldReset && r.EraseStarted && !r.ProgrammedRows);
  fixture();stuck=1;ret=EcUpdate(&io,mEcImage,EC_SIZE,current,TRUE,&r);
  CHECK(ret==EC_TIMEOUT && erases==1 && commits==0 && r.HeldReset && r.EraseStarted);
  puts("PASS: command gate reproduces full constant-12 read, enable/restore and their failures; photographed config; boot/alias/blank-data checks; protected ranges; power; timeouts; inspection without erase/program; update verification and failure holds");
  return 0;
}
