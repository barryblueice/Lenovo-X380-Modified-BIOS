/* SPDX-License-Identifier: GPL-2.0-only */
#include "platform.h"
#include <Library/BaseCryptLib.h>
#include "ec_flash.h"
#include "ec_image.h"
STATIC UINT8 ReadPort(VOID *Context,UINT16 Port) {(VOID)Context;return IoRead8(Port);}
STATIC VOID WritePort(VOID *Context,UINT16 Port,UINT8 Value) {(VOID)Context;IoWrite8(Port,Value);}
STATIC VOID Delay(VOID *Context,UINT32 Us) {(VOID)Context;MicroSecondDelay(Us);}
STATIC BOOLEAN ConfirmUpdate(VOID) {
  CONST CHAR16 *Expected=L"UPDATE";UINTN Pos=0;EFI_INPUT_KEY Key;
  Print(L"\nType UPDATE and Enter. Esc cancels.\n> ");
  while(!EFI_ERROR(ReadKey(&Key))) {
    if(Key.ScanCode==SCAN_ESC)return FALSE;
    if(Key.UnicodeChar==CHAR_CARRIAGE_RETURN){Print(L"\n");return Pos==6;}
    if(Pos>=6 || Key.UnicodeChar!=Expected[Pos])return FALSE;
    Print(L"%c",Key.UnicodeChar);Pos++;
  }return FALSE;
}
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;EFI_TPL OldTpl;BOOLEAN Interrupts,Apply=FALSE;
  EFI_INPUT_KEY Key;EC_RESULT Result;UINT8 Digest[32],*Current;int Ret;
  EC_IO Io={NULL,ReadPort,WritePort,Delay};
  (VOID)ImageHandle;(VOID)SystemTable;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"X380 EC firmware updater\n\nTarget: Lenovo R0SHT57W (EC 1.41)\n"
        L"Connect AC power and keep the battery connected.\n"
        L"Boot and saved-data regions are preserved. No automatic update.\n"
        );
  if(!CheckPlatform() || (PciRead32(PCI_LIB_ADDRESS(0,31,0,0x8c))&0x00fcfffd)!=0x00040701) {
    Print(L"Platform or EC port-decode check failed. No EC update.\n");Pause();return EFI_UNSUPPORTED;
  }
  if(!Sha256HashAll(mEcImage,sizeof(mEcImage),Digest) || CompareMem(Digest,mEcSha256,32)) {
    Print(L"Embedded EC image integrity check failed.\n");Pause();return EFI_CRC_ERROR;
  }
  Print(L"\nI: inspect compatibility without erase/program\nU: update and verify\nEsc: cancel\n");
  do {Status=ReadKey(&Key);if(EFI_ERROR(Status))return Status;
    if(Key.ScanCode==SCAN_ESC)return EFI_ABORTED;
  }while(Key.UnicodeChar!=L'i' && Key.UnicodeChar!=L'I' && Key.UnicodeChar!=L'u' && Key.UnicodeChar!=L'U');
  Apply=Key.UnicodeChar==L'u' || Key.UnicodeChar==L'U';
  if(Apply && !ConfirmUpdate())return EFI_ABORTED;
  Current=AllocatePool(EC_SIZE);if(!Current)return EFI_OUT_OF_RESOURCES;
  Status=PrepareExclusive();
  if(EFI_ERROR(Status)){Print(L"Cannot obtain exclusive EC access: %r\n",Status);FreePool(Current);Pause();return Status;}
  Status=gBS->SetWatchdogTimer(0,0,0,NULL);
  if(EFI_ERROR(Status)){FreePool(Current);Pause();return Status;}
  ZeroMem(&Result,sizeof(Result));
  Print(L"Working; screen and keyboard may pause for up to several minutes.\nDo not remove power.\n");
  OldTpl=gBS->RaiseTPL(TPL_HIGH_LEVEL);Interrupts=SaveAndDisableInterrupts();
  Ret=EcUpdate(&Io,mEcImage,sizeof(mEcImage),Current,Apply,&Result);
  SetInterruptState(Interrupts);gBS->RestoreTPL(OldTpl);
  Print(L"\nResult=%d; stage=%u; address=%05x; changed rows=%u; verified rows=%u\n"
        L"EC reset held=%u; quiesced=%u; cleanup=%d\n",Ret,Result.Stage,Result.Address,
        Result.ChangedRows,Result.ProgrammedRows,Result.HeldReset,Result.Quiesced,Result.CleanupResult);
  Print(L"entry step=%u last=%02x; probe=%04x expected=%02x actual=%02x\n"
        L"command gate before/enabled/restored=%02x/%02x/%02x held=%u\n"
        L"boot-code differences=%u; preserved-data differences=%u\n",
        Result.EntryStep,Result.LastStatus,Result.ProbeAddress,Result.ProbeExpected,Result.ProbeActual,
        Result.CommandGateBefore,Result.CommandGateEnabled,Result.CommandGateRestored,Result.CommandGateHeld,
        Result.BootMismatchCount,Result.PreservedDataDifferences);
  FreePool(Current);
  Print(L"halt step=%u port66=%02x F554=%02x waited_us=%u\n",
        Result.HaltStep,Result.HaltPortStatus,Result.HaltHandshake,Result.HaltWaitUs);
  Print(L"read passes=%u prepare active=%u\n",Result.ReadPasses,Result.PrepareActive);
  if(Ret) {
    Print(L"Update NOT completed. Photograph the values above.\n"
          L"1=image/argument, 2=timeout, 3=platform/handshake, 4=power, 5=verification, 6=boot-code mismatch.\n");
    if(Result.HeldReset || Result.Quiesced || Result.PrepareActive || Result.CommandGateHeld) {
      Print(L"EC has not returned to normal operation. Keep power connected.\n"
            L"No automatic retry or reboot. Firmware is halted for diagnosis.\n");CpuDeadLoop();
    }
    Pause();return EFI_DEVICE_ERROR;
  }
  if(!Apply)Print(L"Inspection passed. No flash erase/program was issued.\n");
  else if(!Result.ChangedRows)Print(L"EC code already matches target. Nothing programmed.\n");
  else Print(L"SUCCESS: EC code written and full 128 KiB readback checked.\n"
             L"Preserved boot/data regions also match their original contents.\n"
             L"Power off normally, then start again to test EC operation.\n");
  Pause();return EFI_SUCCESS;
}
