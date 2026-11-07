/* SPDX-License-Identifier: GPL-2.0-only
 * Reconstructed from R0SET57W Kb902xFlashDxe and PlatformFlashDxe.
 * See evidence/EC-PORT.txt for offsets, scope and untested hardware limits.
 */
#include "ec_flash.h"
#define POLLS 20000U
#define TRY(x) do { int r_=(x); if(r_) return r_; } while(0)
static UINT8 in(CONST EC_IO *i, UINT16 p) {return i->Read(i->Context,p);}
static VOID out(CONST EC_IO *i, UINT16 p, UINT8 v) {i->Write(i->Context,p,v);}
static VOID delay(CONST EC_IO *i, UINT32 us) {i->Delay(i->Context,us);}
static UINT8 rd(CONST EC_IO *i, UINT16 a) {
  out(i,0x705,(UINT8)(a>>8));out(i,0x706,(UINT8)a);return in(i,0x707);
}
static VOID wr(CONST EC_IO *i, UINT16 a, UINT8 v) {
  out(i,0x705,(UINT8)(a>>8));out(i,0x706,(UINT8)a);out(i,0x707,v);
}
static int wait_reg(CONST EC_IO *i, UINT16 a, UINT8 mask, UINT8 value) {
  UINT32 n;
  for(n=0;n<POLLS;n++){if((rd(i,a)&mask)==value)return EC_OK;delay(i,30);}
  return EC_TIMEOUT;
}
static int wait_port(CONST EC_IO *i, UINT16 p, UINT8 mask, UINT8 value) {
  UINT32 n;UINT8 v;
  for(n=0;n<POLLS;n++){v=in(i,p);if(v==0xff)return EC_PLATFORM;
    if((v&mask)==value)return EC_OK;
    delay(i,30);}
  return EC_TIMEOUT;
}
static int command(CONST EC_IO *i, UINT8 cmd, UINT8 data) {
  TRY(wait_port(i,0x66,2,0));out(i,0x66,cmd);
  TRY(wait_port(i,0x66,2,0));out(i,0x62,data);
  return wait_port(i,0x66,2,0);
}
static int acpi_read(CONST EC_IO *i, UINT8 a, UINT8 *value) {
  UINT32 n;
  for(n=0;n<256 && (in(i,0x66)&1);n++)in(i,0x62);
  if(n==256)return EC_PLATFORM;
  TRY(command(i,0x80,a));TRY(wait_port(i,0x66,1,1));*value=in(i,0x62);
  return EC_OK;
}
static int busy(CONST EC_IO *i) {return wait_reg(i,0xfead,2,0);}
/* FEAD[3] gates writes to the FEAC command port, including READ (03).
 * Vendor erase 0x6ac sets this before issuing commands. Standalone inspection
 * cannot rely on an earlier erase to leave it set. Restore only this bit,
 * preserving all other live status/config bits. No erase is used to enable it.
 * Confirmed in ENE KB9012 rev 0.9 p135; KB902x hardware validation pending. */
static int command_gate_open(CONST EC_IO *i,EC_RESULT *r) {
  UINT8 v;TRY(busy(i));v=rd(i,0xfead);if(v==0xff)return EC_PLATFORM;
  r->CommandGateBefore=v;r->CommandGateHeld=TRUE;
  wr(i,0xfead,v|8);r->CommandGateEnabled=rd(i,0xfead);
  if(r->CommandGateEnabled==0xff || !(r->CommandGateEnabled&8))return EC_PLATFORM;
  return busy(i);
}
static int command_gate_close(CONST EC_IO *i,EC_RESULT *r) {
  UINT8 v;if(!r->CommandGateHeld)return EC_OK;
  TRY(busy(i));v=rd(i,0xfead);if(v==0xff)return EC_PLATFORM;
  wr(i,0xfead,(v&~8)|(r->CommandGateBefore&8));
  r->CommandGateRestored=rd(i,0xfead);
  if(r->CommandGateRestored==0xff ||
     (r->CommandGateRestored&8)!=(r->CommandGateBefore&8))return EC_PLATFORM;
  r->CommandGateHeld=FALSE;return EC_OK;
}
#ifndef X380_EC_INSPECT_ONLY
static VOID address(CONST EC_IO *i, UINT32 a) {
  wr(i,0xfeaa,(UINT8)(a>>16));wr(i,0xfea9,(UINT8)(a>>8));wr(i,0xfea8,(UINT8)a);
}
#endif
static BOOLEAN same(CONST UINT8 *a,CONST UINT8 *b,UINTN n) {
  UINTN k;for(k=0;k<n;k++)if(a[k]!=b[k])return FALSE;return TRUE;
}
/* Original read 0x808; erase 0x6ac; program 0x904. Row-aligned interface. */
int EcReadRow(CONST EC_IO *i, UINT32 a, UINT8 *b) {
  UINT32 k;if(a>=EC_SIZE || a%EC_ROW || !b)return EC_INVALID;
  TRY(busy(i));wr(i,0xfeaa,(UINT8)(a>>16));wr(i,0xfea9,(UINT8)(a>>8));
  for(k=0;k<EC_ROW;k++){
    wr(i,0xfea8,(UINT8)(a+k));wr(i,0xfeac,3);TRY(busy(i));b[k]=rd(i,0xfeab);
  }return EC_OK;
}
#ifndef X380_EC_INSPECT_ONLY
static VOID timing(CONST EC_IO *i,UINT8 v) {
  wr(i,0xff1d,(rd(i,0xff1d)&0xfc)|4);wr(i,0xff1e,v);
}
static VOID timing_end(CONST EC_IO *i) {
  wr(i,0xff1e,0x1f);wr(i,0xff1d,rd(i,0xff1d)&0xf8);
}
#endif
int EcEraseRow(CONST EC_IO *i, UINT32 a) {
#ifdef X380_EC_INSPECT_ONLY
  (VOID)i;(VOID)a;return EC_INVALID;
#else
  BOOLEAN adjust;
  if(a<EC_START || a>=EC_END || a%EC_ROW)return EC_INVALID;
  wr(i,0xfead,rd(i,0xfead)|8);TRY(busy(i));
  adjust=(rd(i,0xfe9e)&0x10)!=0;if(adjust)timing(i,0x30);
  address(i,a);wr(i,0xfeac,0x20);TRY(busy(i));
  if(adjust)timing_end(i);
  return EC_OK;
#endif
}
int EcProgramRow(CONST EC_IO *i, UINT32 a, CONST UINT8 *b) {
#ifdef X380_EC_INSPECT_ONLY
  (VOID)i;(VOID)a;(VOID)b;return EC_INVALID;
#else
  BOOLEAN adjust;UINT32 k;
  if(a<EC_START || a>=EC_END || a%EC_ROW || !b)return EC_INVALID;
  TRY(busy(i));adjust=(rd(i,0xfe9e)&0x10)!=0;if(adjust)timing(i,0x0f);
  wr(i,0xfeac,0x80);TRY(busy(i));
  wr(i,0xfeaa,(UINT8)(a>>16));wr(i,0xfea9,(UINT8)(a>>8));
  for(k=0;k<EC_ROW;k++){
    wr(i,0xfea8,(UINT8)(a+k));wr(i,0xfeab,b[k]);wr(i,0xfeac,2);TRY(busy(i));
  }
  wr(i,0xfeac,0x70);TRY(busy(i));if(adjust)timing_end(i);return EC_OK;
#endif
}
/* Narrow allow-list from this X380's repeated physical captures, not a
 * generic KB90xx identity test. Do not infer that arbitrary FF data is valid.
 * No indexed data writes until every selector echo and config value matches.
 * Vendor PlatformFlashDxe 0x758..0x791 accepts FF80=FF as a value. */
static int probe_index(CONST EC_IO *i,EC_RESULT *r) {
  static CONST UINT16 addresses[]={0xfe90,0xfe92,0xfe93,0xfe95,0xff00,0xff02};
  static CONST UINT8 expected[]={0x20,0x07,0x00,0x7d,0xc0,0x2f};
  UINT32 pass,n,k;UINT8 value;
  for(pass=0;pass<2;pass++)for(n=0;n<6;n++) {
    k=pass ? 5-n : n;r->ProbeAddress=addresses[k];
    out(i,0x705,(UINT8)(addresses[k]>>8));out(i,0x706,(UINT8)addresses[k]);
    r->ProbeExpected=(UINT8)(addresses[k]>>8);r->ProbeActual=in(i,0x705);
    if(r->ProbeActual!=r->ProbeExpected)return EC_PLATFORM;
    r->ProbeExpected=(UINT8)addresses[k];r->ProbeActual=in(i,0x706);
    if(r->ProbeActual!=r->ProbeExpected)return EC_PLATFORM;
    r->ProbeExpected=expected[k];r->ProbeActual=in(i,0x707);
    if(r->ProbeActual!=r->ProbeExpected)return EC_PLATFORM;
  }
  r->ProbeAddress=0xfead;r->ProbeExpected=0;r->ProbeActual=rd(i,0xfead);
  if(r->ProbeActual==0xff || (r->ProbeActual&2))return EC_PLATFORM;
  /* Only attempt entry from the idle state seen in both physical captures. */
  r->ProbeAddress=0xf554;r->ProbeExpected=0;r->ProbeActual=rd(i,0xf554);
  if(r->ProbeActual)return EC_PLATFORM;
  r->ProbeAddress=0xff14;r->ProbeExpected=0;r->ProbeActual=rd(i,0xff14);
  if(r->ProbeActual==0xff || (r->ProbeActual&1))return EC_PLATFORM;
  value=rd(i,0xff80);r->LastStatus=value;
  return EC_OK;
}
static int quiet(CONST EC_IO *i,EC_RESULT *r,UINT8 *ff80) {
  r->EntryStep=1;
  TRY(wait_port(i,0x64,2,0));
  /* Refuse a pending keyboard byte instead of discarding/reinjecting it. */
  if(in(i,0x64)&1)return EC_PLATFORM;
  r->EntryStep=2;TRY(probe_index(i,r));
  *ff80=r->LastStatus;
  r->EntryStep=3;
  r->Quiesced=TRUE;wr(i,0xff80,*ff80|0x30);
  TRY(wait_port(i,0x64,2,0));out(i,0x60,0xad);
  r->EntryStep=4;TRY(wait_port(i,0x64,2,0));TRY(command(i,0x59,0xf1));
  r->EntryStep=5;
  {int ret=wait_reg(i,0xf554,0xff,0x33);r->LastStatus=rd(i,0xf554);if(ret)return ret;}
  r->EntryStep=6;delay(i,100);return EC_OK;
}
static int resume_quiet(CONST EC_IO *i,EC_RESULT *r,UINT8 ff80) {
  if(!r->Quiesced && !r->PrepareActive)return EC_OK;
  wr(i,0xff10,0xff);delay(i,100);TRY(wait_reg(i,0xf554,0xff,0));
  TRY(wait_port(i,0x64,2,0));out(i,0x60,0xae);TRY(wait_port(i,0x64,2,0));
  wr(i,0xff80,ff80);r->Quiesced=FALSE;r->PrepareActive=FALSE;return EC_OK;
}
static int halt_wait_port(CONST EC_IO *i,EC_RESULT *r,UINT32 step) {
  UINT32 n;r->HaltStep=step;r->HaltWaitUs=0;
  for(n=0;n<POLLS;n++){
    r->HaltPortStatus=in(i,0x66);
    if(r->HaltPortStatus==0xff)return EC_PLATFORM;
    if(!(r->HaltPortStatus&2))return EC_OK;
    delay(i,30);r->HaltWaitUs+=30;
  }
  return EC_TIMEOUT;
}
static int halt(CONST EC_IO *i,EC_RESULT *r) {
  UINT32 n;
  /* PlatformFlashDxe 0xa17..0xa84: four input-buffer waits, then
   * wait on F554 immediately after sending F2. No fifth IBF wait. */
  TRY(halt_wait_port(i,r,1));out(i,0x66,0x59);
  TRY(halt_wait_port(i,r,2));out(i,0x62,0x77);
  TRY(halt_wait_port(i,r,3));out(i,0x66,0x59);
  TRY(halt_wait_port(i,r,4));out(i,0x62,0xf2);
  r->HaltStep=5;r->HaltWaitUs=0;
  for(n=0;n<POLLS;n++){
    r->HaltHandshake=rd(i,0xf554);r->LastStatus=r->HaltHandshake;
    if(r->HaltHandshake==0xa5)break;
    delay(i,30);r->HaltWaitUs+=30;
  }
  if(n==POLLS){r->HaltPortStatus=in(i,0x66);return EC_TIMEOUT;}
  r->HaltStep=6;
  wr(i,0xff14,rd(i,0xff14)|1);r->HeldReset=TRUE;
  delay(i,10000000);
  /* Vendor retries reset if handshake changes; fail closed instead. */
  r->HaltStep=7;r->LastStatus=rd(i,0xf554);r->HaltHandshake=r->LastStatus;
  return r->LastStatus==0xa5 ? EC_OK : EC_PLATFORM;
}
static int restart(CONST EC_IO *i,EC_RESULT *r) {
  if(!r->HeldReset)return EC_OK;
  wr(i,0xff14,rd(i,0xff14)&0xfe);r->HeldReset=FALSE;delay(i,100);
  return wait_reg(i,0xf554,0xff,0);
}
int EcUpdate(CONST EC_IO *i, CONST UINT8 *image, UINTN bytes, UINT8 *current,
             BOOLEAN apply, EC_RESULT *r) {
  UINT32 a;UINT8 ac,bat,ff80=0,check[EC_ROW];int ret=EC_OK;
#ifdef X380_EC_INSPECT_ONLY
  if(apply)return EC_INVALID;
#endif
  if(!i || !i->Read || !i->Write || !i->Delay || !r || !current || !image ||
     bytes!=EC_SIZE || !same(image+8,(CONST UINT8 *)"ENE Haru",8))return EC_INVALID;
  /* This exception is specific to the verified R0SHT57W layout: the update
   * file has FF fill here; actual flash contains SER#/USR#/CON# data.
   * Never treat non-FF code from a different target image as optional. */
  for(a=EC_DATA_START;a<EC_DATA_END;a++)if(image[a]!=0xff)return EC_INVALID;
  r->Stage=ST_IDLE;r->Address=0;r->ChangedRows=0;r->ProgrammedRows=0;
  r->EraseStarted=FALSE;r->HeldReset=FALSE;r->Quiesced=FALSE;r->CleanupResult=0;r->LastStatus=0;
  r->EntryStep=0;r->ProbeAddress=0;r->ProbeExpected=0;r->ProbeActual=0;
  r->CompatFlags=0;r->BootMismatchCount=0;r->FirstBootMismatch=0xffffffffU;
  r->ZeroBytes=0;r->FfBytes=0;r->HalfDiffBytes=0;r->FirstExpected=0;r->FirstActual=0;
  r->CommandGateBefore=0;r->CommandGateEnabled=0;r->CommandGateRestored=0;r->CommandGateHeld=FALSE;
  r->PreservedDataDifferences=0;
  r->HaltStep=0;r->HaltWaitUs=0;r->HaltPortStatus=0;r->HaltHandshake=0;
  r->PrepareActive=FALSE;r->ReadPasses=0;
  r->Stage=ST_POWER;
  ret=acpi_read(i,0x46,&ac);if(ret)return ret;
  ret=acpi_read(i,0x38,&bat);if(ret)return ret;
  if(ac==0xff || bat==0xff || !(ac&0x10) || !(bat&0x80))return EC_POWER;
  r->Stage=ST_QUIESCE;ret=quiet(i,r,&ff80);if(ret)goto cleanup;
read_current:
  r->Stage=ST_READ;
  ret=command_gate_open(i,r);if(ret)goto cleanup;
  for(a=0;a<EC_SIZE;a+=EC_ROW){r->Address=a;ret=EcReadRow(i,a,current+a);if(ret)goto cleanup;}
  r->ReadPasses++;
  r->Stage=ST_COMPAT;
  r->CompatFlags=0;r->BootMismatchCount=0;r->FirstBootMismatch=0xffffffffU;
  r->FirstExpected=0;r->FirstActual=0;r->ZeroBytes=0;r->FfBytes=0;r->HalfDiffBytes=0;
  r->PreservedDataDifferences=0;r->ChangedRows=0;
  /* Preserve the entire first and last 4 KiB. Compare all first-4-KiB bytes
   * outside the target's verified FF data slot, including f80..fff.
   * The device's 128-KiB readback proved all those bytes match exactly.
   * Data-slot differences are counted separately, never copied from target. */
  for(a=0;a<EC_SIZE;a++){
    if(current[a]==0)r->ZeroBytes++;
    if(current[a]==0xff)r->FfBytes++;
    if(a<0x10000 && current[a]!=current[a+0x10000])r->HalfDiffBytes++;
    if(a>=EC_DATA_START && a<EC_DATA_END && current[a]!=image[a])r->PreservedDataDifferences++;
    if(a<EC_START && !(a>=EC_DATA_START && a<EC_DATA_END) && current[a]!=image[a]){
      if(!r->BootMismatchCount){r->FirstBootMismatch=a;r->FirstExpected=image[a];r->FirstActual=current[a];}
      r->BootMismatchCount++;
    }
  }
  if(r->BootMismatchCount)r->CompatFlags|=1;
  if(!r->HalfDiffBytes)r->CompatFlags|=2;
  if(r->CompatFlags){
    ret=EC_BOOTBLOCK;goto cleanup;
  }
  for(a=EC_START;a<EC_END;a+=EC_ROW)if(!same(current+a,image+a,EC_ROW))r->ChangedRows++;
  if(!apply || !r->ChangedRows)goto cleanup;
  if(!r->HeldReset){
    /* 59/F1 read mode no longer consumes normal EC commands on this machine.
     * Finish that transaction before attempting the separate 59/77,F2 path.
     * All later comparisons use a fresh snapshot taken with reset held. */
    r->Stage=ST_HALT;
    ret=command_gate_close(i,r);if(ret)goto cleanup;
    ret=resume_quiet(i,r,ff80);if(ret)goto cleanup;
    r->PrepareActive=TRUE;ret=halt(i,r);if(ret)goto cleanup;
    goto read_current;
  }
  /* Only changed rows, never protected boot/state areas, never retry after
   * erase. Every changed row verified, followed by complete-chip comparison. */
  for(a=EC_START;a<EC_END;a+=EC_ROW){
    if(same(current+a,image+a,EC_ROW))continue;
    r->Address=a;r->Stage=ST_ERASE;r->EraseStarted=TRUE;
    ret=EcEraseRow(i,a);if(ret)goto cleanup;
    r->Stage=ST_PROGRAM;ret=EcProgramRow(i,a,image+a);if(ret)goto cleanup;
    r->Stage=ST_VERIFY;ret=EcReadRow(i,a,check);if(ret)goto cleanup;
    if(!same(check,image+a,EC_ROW)){ret=EC_VERIFY;goto cleanup;}r->ProgrammedRows++;
  }
  for(a=0;a<EC_SIZE;a+=EC_ROW){
    CONST UINT8 *expected=(a<EC_START || a>=EC_END)? current+a:image+a;
    r->Address=a;ret=EcReadRow(i,a,check);if(ret)goto cleanup;
    if(!same(check,expected,EC_ROW)){ret=EC_VERIFY;goto cleanup;}
  }
cleanup:
  if(ret && r->EraseStarted)return ret; /* Leave reset asserted on uncertain flash. */
  r->CleanupResult=command_gate_close(i,r);
  if(!r->CleanupResult)r->CleanupResult=restart(i,r);
  if(!r->CleanupResult)r->CleanupResult=resume_quiet(i,r,ff80);
  if(!ret)ret=r->CleanupResult;
  if(!ret)r->Stage=ST_DONE;
  return ret;
}
