/* Host-only tests; not included in the firmware INF. No hardware I/O. */
#include "X380Autopatcher.c"
#define CHECK(x) do { if (!(x)) { puts("FAIL: " #x); return 1; } } while (0)
extern int puts(const char *);
EFI_SYSTEM_TABLE *gST;
EFI_BOOT_SERVICES *gBS;
EFI_RUNTIME_SERVICES *gRT;
static EFI_SYSTEM_TABLE St;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
static const CHAR16 *Keys;
static unsigned KeyPos, Calls, Reads, Writes, FailAt;
static UINT8 Ec[256];
EFI_GUID gEfiSmbios3TableGuid, gEfiSmbiosTableGuid;
static SMBIOS_TABLE_3_0_ENTRY_POINT TableEntry;
static UINT8 TableData[256];
static UINT32 LpcId, SpiId;
static unsigned LpcReads, SpiReads;

RETURN_STATUS EFIAPI EfiGetSystemConfigurationTable(EFI_GUID *Guid, VOID **Table) {
  if (Guid != &gEfiSmbios3TableGuid) return EFI_NOT_FOUND;
  *Table = &TableEntry; return EFI_SUCCESS;
}
INTN EFIAPI AsciiStrCmp(CONST CHAR8 *A, CONST CHAR8 *B) {
  while (*A && *A == *B) { A++; B++; }
  return (UINT8)*A - (UINT8)*B;
}
UINT32 EFIAPI PciRead32(UINTN Address) {
  if (Address == PCI_LIB_ADDRESS(0,31,0,0)) { LpcReads++; return LpcId; }
  if (Address == PCI_LIB_ADDRESS(0,31,5,0)) { SpiReads++; return SpiId; }
  return 0xffffffff;
}
static unsigned AppendString(unsigned At, const CHAR8 *Value) {
  do { TableData[At++] = (UINT8)*Value; } while (*Value++);
  return At;
}
static void PlatformFixture(const CHAR8 *Version, const CHAR8 *Model) {
  unsigned I, At;
  for (I=0;I<sizeof(TableData);I++) TableData[I]=0;
  TableData[0]=0; TableData[1]=6; TableData[5]=1;
  At=AppendString(6,Version); TableData[At++]=0;
  TableData[At]=2; TableData[At+1]=6; TableData[At+4]=1; TableData[At+5]=2;
  At+=6; At=AppendString(At,"LENOVO"); At=AppendString(At,Model); TableData[At++]=0;
  TableData[At]=127; TableData[At+1]=4; At+=6;
  TableEntry.TableAddress=(UINT64)(UINTN)TableData;
  TableEntry.TableMaximumSize=At;
  LpcId=0x9d4e8086; SpiId=0x9d248086; LpcReads=SpiReads=0;
}

UINTN EFIAPI Print(CONST CHAR16 *Format, ...) { (VOID)Format; return 0; }
static EFI_STATUS EFIAPI Key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *K) {
  (VOID)This;
  K->UnicodeChar=Keys[KeyPos++];
  K->ScanCode=K->UnicodeChar==27 ? SCAN_ESC : 0;
  return EFI_SUCCESS;
}
static int Transfer(void *Context, uint8_t Command, const uint8_t *Tx,
                    size_t TxSize, uint8_t *Rx, size_t RxSize) {
  unsigned At=(Tx[0]-6)*128+Tx[1];
  (VOID)Context;(VOID)TxSize;(VOID)RxSize;
  if (++Calls==FailAt) return X380_AP_TIMEOUT;
  if (Command==0x5e) { Reads++; *Rx=Ec[At]; }
  else { Writes++; Ec[At]=Tx[2]; }
  return 0;
}
const struct x380_ap_io *x380_ap_native_io(void) {
  static const struct x380_ap_io Io={0,Transfer}; return &Io;
}
static void Reset(void) {
  unsigned I;
  for(I=0;I<256;I++) Ec[I]=mOriginal[I]=0xa5;
  Calls=Reads=Writes=FailAt=0;mWriteStarted=FALSE;
}
int main(void) {
  UINT8 Backup[256], Value, Tx[2]={6,0};
  size_t Done;
  int Ret;
  const struct x380_ap_io Guard={0,GuardedTransfer};
  gST=&St; St.ConIn=&Input; Input.ReadKeyStroke=Key;
  PlatformFixture("CBET4000 X380-Release-v10","X380 Yoga");
  CHECK(CheckPlatform() && LpcReads==1 && SpiReads==1);
  /* Regression: actual v9 machine reported LPC=9d4e8086, SPI=ffffffff. */
  SpiId=0xffffffff;
  CHECK(CheckPlatform() && Calls==0);
  LpcId=0xffffffff; CHECK(!CheckPlatform() && Calls==0);
  LpcId=0x9d438086; CHECK(!CheckPlatform() && Calls==0);
  PlatformFixture("CBET4000 X380-Release-v10","Other Board");
  SpiId=0xffffffff; CHECK(!CheckPlatform() && Calls==0);
  PlatformFixture("CBET4000 X380-Release-v9","X380 Yoga");
  SpiId=0xffffffff; CHECK(!CheckPlatform() && Calls==0);
  PlatformFixture("CBET4000 X380-Release-v10","X380 Yoga");
  SpiId=0; CHECK(!CheckPlatform() && Calls==0);
  SpiId=0x12348086; CHECK(!CheckPlatform() && Calls==0);
  LpcId=0x9d248086; SpiId=0x9d4e8086;
  CHECK(!CheckPlatform());
  PlatformFixture("CBET4000 X380-Release-v8","X380 Yoga"); CHECK(!CheckPlatform());
  PlatformFixture("CBET4000 X380-Release-v10","Other Board"); CHECK(!CheckPlatform());
  PlatformFixture("CBET4000 X380-Release-v10","X380 Yoga");
  TableData[1]=2; CHECK(!CheckPlatform());
  Keys=L"APPLY\r";KeyPos=0;CHECK(Confirm());
  Keys=L"\r";KeyPos=0;CHECK(!Confirm());
  Keys=L"APPX";KeyPos=0;CHECK(!Confirm());
  Keys=L"\x1b";KeyPos=0;CHECK(!Confirm());
  Keys=L"APPLYX";KeyPos=0;CHECK(!Confirm());
  Reset();Tx[0]=5;
  CHECK(GuardedTransfer(0,0x5e,Tx,2,&Value,1)==X380_AP_INVALID && Calls==0);
  Reset();Ec[0]=0x99;
  CHECK(x380_ap_apply_template(&Guard,Backup,&Done)==X380_AP_VERIFY && Writes==0);
  Reset();
  CHECK(x380_ap_apply_template(&Guard,Backup,&Done)==0 && Done==256 && Writes==256 && Reads==512);
  CHECK(Ec[0x30]==0x8b && Ec[0x50]==1 && Ec[0x10]==0 && Ec[0x20]==0);
  Reset();FailAt=256+8;
  Ret=x380_ap_apply_template(&Guard,Backup,&Done);
  CHECK(Ret==X380_AP_TIMEOUT && Done==7 && Writes==7 && Calls==264);
  puts("PASS: real-machine unavailable SPI regression, missing/wrong LPC and wrong board/version rejected with unavailable SPI, unexpected visible SPI rejected, no EC I/O in platform checks, realistic X380 LPC/SPI IDs, swapped IDs rejected, old version/wrong board/malformed SMBIOS rejected, manual confirmation, cancellation, bank restriction, snapshot mismatch abort, complete template, partial failure no retry");
  return 0;
}
