/* SPDX-License-Identifier: GPL-2.0-only */
#include "platform.h"
#include <Library/BaseCryptLib.h>
#include "ec_flash.h"
#include "ec_image.h"
#include "ec_diff.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
STATIC EFI_STATUS SaveReadback(EFI_HANDLE Handle,CONST UINT8 *Data,CHAR16 *Name) {
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root=NULL,*File=NULL;EFI_STATUS Status;UINTN Size=EC_SIZE;
  UnicodeSPrint(Name,80,L"ec-read-%016lx.bin",GetPerformanceCounter());
  Status=gBS->HandleProtocol(Handle,&gEfiLoadedImageProtocolGuid,(VOID **)&Loaded);
  if(EFI_ERROR(Status))return Status;
  Status=gBS->HandleProtocol(Loaded->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(VOID **)&Fs);
  if(EFI_ERROR(Status))return Status;
  Status=Fs->OpenVolume(Fs,&Root);if(EFI_ERROR(Status))return Status;
  Status=Root->Open(Root,&File,Name,EFI_FILE_MODE_READ,0);
  if(!EFI_ERROR(Status)){File->Close(File);Root->Close(Root);return EFI_ALREADY_STARTED;}
  if(Status!=EFI_NOT_FOUND){Root->Close(Root);return Status;}
  Status=Root->Open(Root,&File,Name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(!EFI_ERROR(Status)){
    Status=File->Write(File,&Size,(VOID *)Data);
    if(!EFI_ERROR(Status) && Size!=EC_SIZE)Status=EFI_DEVICE_ERROR;
    if(!EFI_ERROR(Status))Status=File->Flush(File);
    File->Close(File);
  }
  Root->Close(Root);return Status;
}
STATIC VOID ShowDifferenceMap(CONST UINT8 *Current,CONST EC_RESULT *Result,
                              EFI_STATUS Saved,CONST CHAR16 *Name) {
  STATIC CONST UINT32 Bounds[]={0,0x200,0x600,0x800,0xf80,0x1000,0x1f000,0x20000};
  UINTN K;EC_DIFF D;UINT8 Hash[32];
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"X380 EC difference map (no update)\nTarget: R0SHT57W 1.41\n\n");
  Print(L"gate before/enabled/restored=%02x/%02x/%02x; cleanup=%d\n",
        Result->CommandGateBefore,Result->CommandGateEnabled,Result->CommandGateRestored,Result->CleanupResult);
  Print(L"Range inclusive   Differences   First    Last\n");
  for(K=0;K+1<sizeof(Bounds)/sizeof(Bounds[0]);K++){
    EcDescribeRange(Current,mEcImage,Bounds[K],Bounds[K+1],&D);
    if(D.Count)Print(L"%05x-%05x       %6u       %05x    %05x\n",Bounds[K],Bounds[K+1]-1,D.Count,D.First,D.Last);
    else Print(L"%05x-%05x            0       -----    -----\n",Bounds[K],Bounds[K+1]-1);
  }
  EcDescribeRange(Current,mEcImage,0,0xf80,&D);
  Print(L"Boot FF->00 differences=%u; other differences=%u\n",D.FfToZero,D.Other);
  EcDescribeRange(Current,mEcImage,0x200,0x800,&D);
  Print(L"0200-07FF read zero=%u FF=%u (1536 bytes total)\n",D.Zero,D.Ff);
  if(Sha256HashAll(Current,EC_SIZE,Hash)){
    Print(L"Read SHA256:\n");for(K=0;K<32;K++)Print(L"%02x",Hash[K]);Print(L"\n");
  }
  if(EFI_ERROR(Saved))Print(L"Readback save: %r (NTFS is read-only).\n",Saved);
  else Print(L"Readback saved on launch volume: %s\n",Name);
  Print(L"No compatibility guard bypassed; no flash erase/program.\nPhotograph this difference map.\n");
}
STATIC UINT8 ReadPort(VOID *C,UINT16 P){(VOID)C;return IoRead8(P);}
STATIC VOID WritePort(VOID *C,UINT16 P,UINT8 V){(VOID)C;IoWrite8(P,V);}
STATIC VOID Delay(VOID *C,UINT32 Us){(VOID)C;MicroSecondDelay(Us);}
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable) {
  EC_IO Io={NULL,ReadPort,WritePort,Delay};EC_RESULT Result;
  EFI_STATUS Status;EFI_TPL OldTpl;EFI_INPUT_KEY Key;BOOLEAN Interrupts;
  UINT8 Digest[32],*Current;int Ret;UINTN N;BOOLEAN HaveDigest=FALSE;
  CHAR16 ReadName[40];EFI_STATUS SaveStatus;
  (VOID)ImageHandle;(VOID)SystemTable;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"X380 EC USB compatibility inspection\nTarget: R0SHT57W (EC 1.41)\n"
        L"Keep AC and battery connected.\n"
        L"Temporarily enters EC maintenance mode and reads firmware.\n"
        L"Flash erase/program are disabled in this inspection build.\n");
  if(!CheckPlatform() || (PciRead32(PCI_LIB_ADDRESS(0,31,0,0x8c))&0x00fcfffd)!=0x00040701){
    Print(L"Platform/decode mismatch.\n");Pause();return EFI_UNSUPPORTED;
  }
  if(!Sha256HashAll(mEcImage,sizeof(mEcImage),Digest) || CompareMem(Digest,mEcSha256,32)){
    Print(L"Target image integrity failure.\n");Pause();return EFI_CRC_ERROR;
  }
  Print(L"Press I to inspect, Esc to cancel.\n");
  do {
    Status=ReadKey(&Key);if(EFI_ERROR(Status))return Status;
    if(Key.ScanCode==SCAN_ESC)return EFI_ABORTED;
  }while(Key.UnicodeChar!=L'i' && Key.UnicodeChar!=L'I');
  Current=AllocatePool(EC_SIZE);if(!Current)return EFI_OUT_OF_RESOURCES;
  Status=PrepareExclusive();
  if(EFI_ERROR(Status)){Print(L"Exclusive access failed: %r\n",Status);FreePool(Current);Pause();return Status;}
  Status=gBS->SetWatchdogTimer(0,0,0,NULL);
  if(EFI_ERROR(Status)){FreePool(Current);Pause();return Status;}
  ZeroMem(&Result,sizeof(Result));
  Print(L"Inspecting; keyboard pauses during the operation. Keep power connected.\n");
  OldTpl=gBS->RaiseTPL(TPL_HIGH_LEVEL);Interrupts=SaveAndDisableInterrupts();
  Ret=EcUpdate(&Io,mEcImage,sizeof(mEcImage),Current,FALSE,&Result);
  SetInterruptState(Interrupts);gBS->RestoreTPL(OldTpl);
  if(Result.Stage>=ST_COMPAT)HaveDigest=Sha256HashAll(Current,EC_SIZE,Digest);
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"X380 EC USB compatibility result (detailed)\nTarget: R0SHT57W (EC 1.41); flash writes disabled\n\n");
  Print(L"Result=%d stage=%u address=%05x changed rows=%u\n"
        L"entry step=%u last status=%02x\n"
        L"probe=%04x expected=%02x actual=%02x\n"
        L"reset held=%u quiesced=%u cleanup=%d\n",
        Ret,Result.Stage,Result.Address,Result.ChangedRows,Result.EntryStep,Result.LastStatus,
        Result.ProbeAddress,Result.ProbeExpected,Result.ProbeActual,
        Result.HeldReset,Result.Quiesced,Result.CleanupResult);
  Print(L"command gate: before=%02x enabled=%02x restored=%02x held=%u\n",
        Result.CommandGateBefore,Result.CommandGateEnabled,Result.CommandGateRestored,Result.CommandGateHeld);
  if(Result.Stage>=ST_COMPAT){
    Print(L"compat flags=%u (1=boot mismatch, 2=identical halves)\n"
          L"boot mismatches=%u / 2432; differing half-bytes=%u / 65536\n"
          L"read counts: zero=%u FF=%u total=131072\n",
          Result.CompatFlags,Result.BootMismatchCount,Result.HalfDiffBytes,Result.ZeroBytes,Result.FfBytes);
    Print(L"Preserved data-slot differences=%u (not written)\n",Result.PreservedDataDifferences);
    if(Result.BootMismatchCount)Print(L"first boot mismatch @%05x expected=%02x actual=%02x\n",
          Result.FirstBootMismatch,Result.FirstExpected,Result.FirstActual);
    Print(L"read 00000:");for(N=0;N<16;N++)Print(L" %02x",Current[N]);Print(L"\n");
    Print(L"read 10000:");for(N=0;N<16;N++)Print(L" %02x",Current[0x10000+N]);Print(L"\n");
    if(HaveDigest){Print(L"read SHA256:\n");for(N=0;N<32;N++)Print(L"%02x",Digest[N]);Print(L"\n");}
  }
  if(Result.HeldReset || Result.Quiesced || Result.PrepareActive || Result.CommandGateHeld){
    FreePool(Current);
    Print(L"EC did not return to normal mode. Keep power connected and photograph this screen.\n"
          L"Stopped; no automatic retry or reboot.\n");CpuDeadLoop();
  }
  if(Ret)Print(L"Inspection failed. Photograph the values above.\n");
  else Print(L"Inspection passed; EC firmware was NOT updated.\n");
  Print(L"No flash erase/program was issued. Photograph this result.\n");
  if(Result.Stage>=ST_COMPAT){
    SaveStatus=SaveReadback(ImageHandle,Current,ReadName);
    Print(L"Press any key for the difference map.\n");
    Status=ReadKey(&Key);
    if(!EFI_ERROR(Status))ShowDifferenceMap(Current,&Result,SaveStatus,ReadName);
  }
  FreePool(Current);
  Pause();return Ret?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
