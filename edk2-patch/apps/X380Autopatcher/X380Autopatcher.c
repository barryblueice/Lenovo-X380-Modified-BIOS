/* SPDX-License-Identifier: GPL-2.0-only */
#include <Uefi.h>
#include <PiDxe.h>
#include <IndustryStandard/SmBios.h>
#include <Guid/SmBios.h>
#include <Protocol/MpService.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/TimerLib.h>
#include <Library/PrintLib.h>
#include "autopatcher.h"

STATIC EFI_GUID mBackupGuid = {0xc140da7e,0x722b,0x4c81,{0xa3,0x83,0x58,0x23,0x80,0xa0,0x57,0x02}};
STATIC UINT8 mOriginal[X380_AP_BANK_SIZE];
STATIC BOOLEAN mWriteStarted;

/* This app is embedded only in the audited X380-v10 build. Its SMM image has
 * no EC port users. TPL_HIGH_LEVEL + interrupts off prevent DXE timer/ACPI
 * battery polling; synchronous MP rendezvous drains outstanding AP work.
 * No UEFI service, console or variable call is made in the critical section.
 */
STATIC EFI_STATUS ReadKey(EFI_INPUT_KEY *Key) {
  EFI_STATUS Status;
  UINTN Index;
  do {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, Key);
    if (Status == EFI_NOT_READY) {
      Status = gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
      if (EFI_ERROR(Status)) return Status;
      Status = EFI_NOT_READY;
    }
  } while (Status == EFI_NOT_READY);
  return Status;
}

STATIC VOID Pause(VOID) {
  EFI_INPUT_KEY Key;
  Print(L"\nPress any key to return to the firmware menu.\n");
  ReadKey(&Key);
}

STATIC BOOLEAN Confirm(VOID) {
  STATIC CONST CHAR16 Expected[] = L"APPLY";
  EFI_INPUT_KEY Key;
  UINTN Pos = 0;
  Print(L"\nType APPLY and press Enter to proceed. Esc cancels.\n> ");
  while (!EFI_ERROR(ReadKey(&Key))) {
    if (Key.ScanCode == SCAN_ESC) return FALSE;
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print(L"\n");
      return Pos == 5;
    }
    if (Pos < 5 && Key.UnicodeChar == Expected[Pos]) {
      Print(L"%c", Key.UnicodeChar);
      Pos++;
    } else {
      Print(L"\nCancelled: confirmation did not match.\n");
      return FALSE;
    }
  }
  return FALSE;
}

STATIC CONST CHAR8 *SmbiosString(UINT8 *Record, UINTN RecordBytes, UINT8 Index) {
  SMBIOS_STRUCTURE *Header = (SMBIOS_STRUCTURE *)Record;
  UINTN Offset = Header->Length;
  if (!Index) return "";
  while (Offset < RecordBytes) {
    CONST CHAR8 *Start = (CONST CHAR8 *)Record + Offset;
    while (Offset < RecordBytes && Record[Offset]) Offset++;
    if (Offset >= RecordBytes) return "";
    if (--Index == 0) return Start;
    Offset++;
  }
  return "";
}

STATIC BOOLEAN CheckPlatform(VOID) {
  SMBIOS_TABLE_3_0_ENTRY_POINT *Entry3;
  SMBIOS_TABLE_ENTRY_POINT *Entry2;
  UINT8 *Table;
  UINTN Length, Offset = 0;
  BOOLEAN Board = FALSE, Version = FALSE;
  if (!EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiSmbios3TableGuid, (VOID **)&Entry3))) {
    Table = (UINT8 *)(UINTN)Entry3->TableAddress;
    Length = Entry3->TableMaximumSize;
  } else if (!EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiSmbiosTableGuid, (VOID **)&Entry2))) {
    Table = (UINT8 *)(UINTN)Entry2->TableAddress;
    Length = Entry2->TableLength;
  } else return FALSE;
  if (!Table || Length > 1024 * 1024) return FALSE;
  while (Offset + sizeof(SMBIOS_STRUCTURE) <= Length) {
    SMBIOS_STRUCTURE *Header = (SMBIOS_STRUCTURE *)(Table + Offset);
    UINTN End = Offset + Header->Length;
    if (Header->Length < sizeof(*Header) || End > Length) return FALSE;
    while (End + 1 < Length && (Table[End] || Table[End + 1])) End++;
    if (End + 1 >= Length) return FALSE;
    End += 2;
    if (Header->Type == SMBIOS_TYPE_BASEBOARD_INFORMATION && Header->Length >= 6) {
      Print(L"Detected board: %a / %a\n",
            SmbiosString(Table + Offset, End - Offset, Table[Offset+4]),
            SmbiosString(Table + Offset, End - Offset, Table[Offset+5]));
      Board = AsciiStrCmp(SmbiosString(Table + Offset, End - Offset, Table[Offset+4]), "LENOVO") == 0 &&
              AsciiStrCmp(SmbiosString(Table + Offset, End - Offset, Table[Offset+5]), "X380 Yoga") == 0;
    }
    if (Header->Type == SMBIOS_TYPE_BIOS_INFORMATION && Header->Length >= 6) {
      Print(L"Detected BIOS: %a\n", SmbiosString(Table + Offset, End - Offset, Table[Offset+5]));
      Version = AsciiStrCmp(SmbiosString(Table + Offset, End - Offset, Table[Offset+5]),
                           "CBET4000 X380-Release-v10") == 0;
    }
    if (Header->Type == SMBIOS_TYPE_END_OF_TABLE) break;
    Offset = End;
  }
  {
    /* 9d4e belongs to LPC at function 0; the SPI function 5 is 9d24. */
    UINT32 LpcId = PciRead32(PCI_LIB_ADDRESS(0,31,0,0));
    UINT32 SpiId = PciRead32(PCI_LIB_ADDRESS(0,31,5,0));
    /* This EC transport uses LPC ports 0x66/0x62, not the SPI controller.
     * The target reports an inaccessible SPI PCI function (all ones) under
     * this payload. Do not require that unrelated function to be visible.
     * Still reject an unexpected visible device and require exact LPC,
     * board and audited firmware matches. Do not change SPI visibility.
     */
    BOOLEAN SpiUnavailable = SpiId == 0xffffffff;
    BOOLEAN Chipset = LpcId == 0x9d4e8086 &&
                      (SpiId == 0x9d248086 || SpiUnavailable);
    Print(L"Detected LPC 00:1f.0: %08x; SPI 00:1f.5: %08x\n", LpcId, SpiId);
    if (SpiUnavailable)
      Print(L"SPI PCI function is not visible; EC access uses LPC ports.\n");
    Print(L"Platform checks: board=%u, version=%u, chipset=%u\n", Board, Version, Chipset);
    return Board && Version && Chipset;
  }
}

STATIC VOID EFIAPI ApRendezvous(VOID *Arg) { (VOID)Arg; }

STATIC EFI_STATUS PrepareExclusive(VOID) {
  EFI_MP_SERVICES_PROTOCOL *Mp;
  EFI_STATUS Status;
  UINT16 PmBase = PciRead16(PCI_LIB_ADDRESS(0,31,2,0x40)) & 0xff80;
  if (PmBase != 0x1800 || (IoRead16(PmBase + 4) & 1)) return EFI_ACCESS_DENIED;
  Status = gBS->LocateProtocol(&gEfiMpServiceProtocolGuid, NULL, (VOID **)&Mp);
  if (EFI_ERROR(Status)) return Status;
  Status = Mp->StartupAllAPs(Mp, ApRendezvous, FALSE, NULL, 1000000, NULL, NULL);
  return Status == EFI_NOT_STARTED ? EFI_SUCCESS : Status;
}

STATIC int GuardedTransfer(void *Context, uint8_t Command, const uint8_t *Tx,
                          size_t TxSize, uint8_t *Rx, size_t RxSize) {
  const struct x380_ap_io *Native = x380_ap_native_io();
  int Result;
  (VOID)Context;
  /* Only bank 57 is writable from this maintenance application. */
  if (!Tx || TxSize < 2 || Tx[0] < 6 || Tx[0] > 7 || Tx[1] >= 128)
    return X380_AP_INVALID;
  Result = Native->transfer(Native->context, Command, Tx, TxSize, Rx, RxSize);
  if (Result) return Result;
  if (Command == 0x5f) mWriteStarted = TRUE;
  if (Command == 0x5e && !mWriteStarted && Rx && RxSize == 1 &&
      *Rx != mOriginal[(Tx[0] - 6) * 128 + Tx[1]]) return X380_AP_VERIFY;
  return X380_AP_OK;
}

STATIC EFI_STATUS SaveOriginal(CHAR16 *Name, UINTN NameBytes) {
  UINT8 Readback[X380_AP_BANK_SIZE];
  UINTN Bytes = sizeof(Readback);
  EFI_STATUS Status;
  UnicodeSPrint(Name, NameBytes, L"Bank57-%016lx", GetPerformanceCounter());
  /* Never overwrite an existing backup, including on counter collision. */
  Status = gRT->GetVariable(Name, &mBackupGuid, NULL, &Bytes, Readback);
  if (Status != EFI_NOT_FOUND) return EFI_ALREADY_STARTED;
  Status = gRT->SetVariable(Name, &mBackupGuid,
                           EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                           sizeof(mOriginal), mOriginal);
  if (EFI_ERROR(Status)) return Status;
  Bytes = sizeof(Readback);
  Status = gRT->GetVariable(Name, &mBackupGuid, NULL, &Bytes, Readback);
  if (EFI_ERROR(Status)) return Status;
  if (Bytes != sizeof(mOriginal) || CompareMem(Readback, mOriginal, Bytes)) return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status;
  EFI_TPL OldTpl;
  BOOLEAN Interrupts;
  UINT8 Backup[X380_AP_BANK_SIZE];
  CHAR16 BackupName[40];
  size_t Completed = 0;
  UINTN Index;
  int Result = X380_AP_OK;
  const struct x380_ap_io Guard = {NULL, GuardedTransfer};
  (VOID)ImageHandle; (VOID)SystemTable;
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"X380 Autopatcher - manual maintenance\n\n");
  Print(L"Reconstructed original patch: replaces EC bank 57 (256 bytes).\n"
        L"This clears Supervisor Password records and changes other bank fields.\n"
        L"It does not replace the EC firmware. Keep AC power connected.\n"
        L"The original bank is saved in firmware NVRAM before any EC write.\n"
        L"This function has not yet been validated on this physical EC.\n");
  if (!CheckPlatform()) {
    Print(L"\nRefused: platform check failed. See detected values above. No EC write.\n");
    Pause(); return EFI_UNSUPPORTED;
  }
  if (!Confirm()) return EFI_ABORTED;
  Status = PrepareExclusive();
  if (EFI_ERROR(Status)) {
    Print(L"Cannot obtain exclusive pre-OS EC access: %r. No EC write.\n", Status);
    Pause(); return Status;
  }
  Print(L"Reading original bank...\n");
  OldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  Interrupts = SaveAndDisableInterrupts();
  for (Index = 0; Index < sizeof(mOriginal); Index++) {
    Result = x380_ap_read(x380_ap_native_io(), 0x57, (unsigned int)Index, &mOriginal[Index]);
    if (Result) break;
  }
  SetInterruptState(Interrupts);
  gBS->RestoreTPL(OldTpl);
  if (Result) {
    Print(L"EC read failed (%d) at byte %u. No EC write.\n", Result, Index);
    Pause(); return EFI_DEVICE_ERROR;
  }
  {
    BOOLEAN AllZero = TRUE, AllFF = TRUE;
    for (Index = 0; Index < sizeof(mOriginal); Index++) {
      if (mOriginal[Index] != 0) AllZero = FALSE;
      if (mOriginal[Index] != 0xff) AllFF = FALSE;
    }
    if (AllZero || AllFF) {
      Print(L"Refused: uniform EC bank data; transport or compatibility is uncertain. No EC write.\n");
      Pause(); return EFI_DEVICE_ERROR;
    }
  }
  Status = SaveOriginal(BackupName, sizeof(BackupName));
  if (EFI_ERROR(Status)) {
    Print(L"Cannot preserve original bank: %r. No EC write.\n", Status);
    Pause(); return Status;
  }
  Print(L"Original bank saved as NVRAM variable %s.\n", BackupName);
  Status = PrepareExclusive();
  if (EFI_ERROR(Status)) {
    Print(L"EC access unavailable: %r. No EC write.\n", Status);
    Pause(); return Status;
  }
  Print(L"Applying template and checking all 256 bytes...\n");
  mWriteStarted = FALSE;
  OldTpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
  Interrupts = SaveAndDisableInterrupts();
  Result = x380_ap_apply_template(&Guard, Backup, &Completed);
  SetInterruptState(Interrupts);
  gBS->RestoreTPL(OldTpl);
  if (Result) {
    Print(L"FAILED (%d), completed write transfers: %u/256.\n"
          L"Partial changes are possible. No automatic retry or rollback.\n"
          L"Original bank remains in NVRAM: %s\n", Result, Completed, BackupName);
    Pause(); return EFI_DEVICE_ERROR;
  }
  Print(L"SUCCESS: all 256 bytes match the original patch template.\n"
        L"Persistence after power-off still requires a hardware check.\n"
        L"No automatic reboot and no automatic execution on later boots.\n");
  Pause(); return EFI_SUCCESS;
}
