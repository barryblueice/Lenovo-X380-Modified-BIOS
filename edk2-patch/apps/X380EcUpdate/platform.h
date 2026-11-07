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
                           "CBET4000 X380-Coreboot") == 0;
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

