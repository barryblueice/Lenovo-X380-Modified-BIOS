/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X380_EC_FLASH_H
#define X380_EC_FLASH_H
#include <Base.h>
#define EC_SIZE 0x20000U
#define EC_START 0x1000U
#define EC_END 0x1f000U
#define EC_ROW 128U
#define EC_DATA_START 0x200U
#define EC_DATA_END 0x880U
#define EC_BOOT_COMPARE_BYTES (EC_START-(EC_DATA_END-EC_DATA_START))
enum { EC_OK, EC_INVALID, EC_TIMEOUT, EC_PLATFORM, EC_POWER, EC_VERIFY, EC_BOOTBLOCK };
enum { ST_IDLE, ST_POWER, ST_QUIESCE, ST_READ, ST_COMPAT, ST_HALT,
       ST_ERASE, ST_PROGRAM, ST_VERIFY, ST_RESTART, ST_DONE };
typedef struct {
  VOID *Context;
  UINT8 (*Read)(VOID *, UINT16);
  VOID (*Write)(VOID *, UINT16, UINT8);
  VOID (*Delay)(VOID *, UINT32);
} EC_IO;
typedef struct {
  UINT32 Stage, Address, ChangedRows, ProgrammedRows;
  UINT8 LastStatus;
  BOOLEAN EraseStarted, HeldReset, Quiesced;
  int CleanupResult;
  UINT32 EntryStep;
  UINT16 ProbeAddress;
  UINT8 ProbeExpected, ProbeActual;
  UINT32 CompatFlags, BootMismatchCount, FirstBootMismatch;
  UINT32 ZeroBytes, FfBytes, HalfDiffBytes;
  UINT8 FirstExpected, FirstActual;
  UINT8 CommandGateBefore, CommandGateEnabled, CommandGateRestored;
  BOOLEAN CommandGateHeld;
  UINT32 PreservedDataDifferences;
  UINT32 HaltStep, HaltWaitUs;
  UINT8 HaltPortStatus, HaltHandshake;
  BOOLEAN PrepareActive;
  UINT32 ReadPasses;
} EC_RESULT;
/* Current is caller-owned EC_SIZE bytes, kept in RAM only. Read-only mode
 * still enters/leaves the vendor's quiet mode, but never erases/programs. */
int EcUpdate(CONST EC_IO *, CONST UINT8 *, UINTN, UINT8 *, BOOLEAN, EC_RESULT *);
int EcReadRow(CONST EC_IO *, UINT32, UINT8 *);
int EcEraseRow(CONST EC_IO *, UINT32);
int EcProgramRow(CONST EC_IO *, UINT32, CONST UINT8 *);
#endif
