/* X380 factory battery algorithms with a guarded transport fallback.
 * MMIO must pass its decode and identity checks. If it does not, select
 * the standard ACPI EC command transport only after _REG(3, 1) and an
 * independent R0SH identity check. Never interpret failed MMIO as absence.
 * BMOD: 0 unavailable, 1 MMIO, 2 standard ACPI EC; reset on _REG changes.
 */
Name (ERDY, Zero)
Name (BMOD, Zero)
OperationRegion (ECOR, SystemMemory, 0xff000100, 0x100)
OperationRegion (BLCF, PCI_Config, 0x98, 4)
Field (BLCF, DWordAcc, NoLock, Preserve) { BLGM, 32 }
OperationRegion (BDCF, SystemMemory, 0xfdef2740, 4)
Field (BDCF, DWordAcc, NoLock, Preserve) { BDGM, 32 }
Field (ECOR, ByteAcc, NoLock, Preserve)
{
    Offset (0x14), M000, 8,
    Offset (0x2b), M001, 8, M002, 8,
    Offset (0x38), M003, 7, M004, 1, M005, 7, M006, 1,
    Offset (0x46), , 4, M007, 1,
    Offset (0x7a), M008, 8, M009, 8,
    Offset (0x81), M00A, 8,
    Offset (0xf0), M00B, 32
}
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M00C,   16,
                M00D,   16,
                M00E,   16,
                M00F,   16,
                M010,   16,
                M011,   16,
                M012,   16,
                M013,   16
            }
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M014,   16,
                M015,   16,
                M016,   16
            }
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M017,   16,
                M018,   16,
                M019,   16,
                M01A,   16,
                M01B,   16,
                M01C,   16
            }
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M01D,   32
            }
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M01E,   128
            }
Field (ECOR, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                M01F,   128
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
{
    Offset (0x14), C000, 8,
    Offset (0x2b), C001, 8, C002, 8,
    Offset (0x38), C003, 7, C004, 1, C005, 7, C006, 1,
    Offset (0x46), , 4, C007, 1,
    Offset (0x7a), C008, 8, C009, 8,
    Offset (0x81), C00A, 8,
    Offset (0xf0), C00B, 32
}
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C00C,   16,
                C00D,   16,
                C00E,   16,
                C00F,   16,
                C010,   16,
                C011,   16,
                C012,   16,
                C013,   16
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C014,   16,
                C015,   16,
                C016,   16
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C017,   16,
                C018,   16,
                C019,   16,
                C01A,   16,
                C01B,   16,
                C01C,   16
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C01D,   32
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C01E,   128
            }
Field (ERAM, ByteAcc, NoLock, Preserve)
            {
                Offset (0xA0),
                C01F,   128
            }
Method (BMOK, 0, NotSerialized)
{
    If (!ERDY) { Return (Zero) }
    If (BMOD) { Return (One) }
    If (BLGM == 0xff000001) {
        If (BDGM == 0xff000001) {
            If (M00B == 0x48533052) { BMOD = One Return (One) }
        }
    }
    If (C00B == 0x48533052) { BMOD = 2 Return (One) }
    Return (Zero)
}
Method (G000, 0, NotSerialized)
{
    If (BMOD == One) { Return (M000) }
    Return (C000)
}
Method (W000, 1, NotSerialized)
{
    If (BMOD == One) { M000 = Arg0 }
    Else { C000 = Arg0 }
}
Method (G001, 0, NotSerialized)
{
    If (BMOD == One) { Return (M001) }
    Return (C001)
}
Method (W001, 1, NotSerialized)
{
    If (BMOD == One) { M001 = Arg0 }
    Else { C001 = Arg0 }
}
Method (G002, 0, NotSerialized)
{
    If (BMOD == One) { Return (M002) }
    Return (C002)
}
Method (W002, 1, NotSerialized)
{
    If (BMOD == One) { M002 = Arg0 }
    Else { C002 = Arg0 }
}
Method (G003, 0, NotSerialized)
{
    If (BMOD == One) { Return (M003) }
    Return (C003)
}
Method (G004, 0, NotSerialized)
{
    If (BMOD == One) { Return (M004) }
    Return (C004)
}
Method (G005, 0, NotSerialized)
{
    If (BMOD == One) { Return (M005) }
    Return (C005)
}
Method (G006, 0, NotSerialized)
{
    If (BMOD == One) { Return (M006) }
    Return (C006)
}
Method (G007, 0, NotSerialized)
{
    If (BMOD == One) { Return (M007) }
    Return (C007)
}
Method (G008, 0, NotSerialized)
{
    If (BMOD == One) { Return (M008) }
    Return (C008)
}
Method (G009, 0, NotSerialized)
{
    If (BMOD == One) { Return (M009) }
    Return (C009)
}
Method (W009, 1, NotSerialized)
{
    If (BMOD == One) { M009 = Arg0 }
    Else { C009 = Arg0 }
}
Method (G00A, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00A) }
    Return (C00A)
}
Method (W00A, 1, NotSerialized)
{
    If (BMOD == One) { M00A = Arg0 }
    Else { C00A = Arg0 }
}
Method (G00B, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00B) }
    Return (C00B)
}
Method (G00C, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00C) }
    Return (C00C)
}
Method (G00D, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00D) }
    Return (C00D)
}
Method (G00E, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00E) }
    Return (C00E)
}
Method (G00F, 0, NotSerialized)
{
    If (BMOD == One) { Return (M00F) }
    Return (C00F)
}
Method (G010, 0, NotSerialized)
{
    If (BMOD == One) { Return (M010) }
    Return (C010)
}
Method (G011, 0, NotSerialized)
{
    If (BMOD == One) { Return (M011) }
    Return (C011)
}
Method (G012, 0, NotSerialized)
{
    If (BMOD == One) { Return (M012) }
    Return (C012)
}
Method (G013, 0, NotSerialized)
{
    If (BMOD == One) { Return (M013) }
    Return (C013)
}
Method (G014, 0, NotSerialized)
{
    If (BMOD == One) { Return (M014) }
    Return (C014)
}
Method (G015, 0, NotSerialized)
{
    If (BMOD == One) { Return (M015) }
    Return (C015)
}
Method (G016, 0, NotSerialized)
{
    If (BMOD == One) { Return (M016) }
    Return (C016)
}
Method (G017, 0, NotSerialized)
{
    If (BMOD == One) { Return (M017) }
    Return (C017)
}
Method (G018, 0, NotSerialized)
{
    If (BMOD == One) { Return (M018) }
    Return (C018)
}
Method (G019, 0, NotSerialized)
{
    If (BMOD == One) { Return (M019) }
    Return (C019)
}
Method (G01A, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01A) }
    Return (C01A)
}
Method (G01B, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01B) }
    Return (C01B)
}
Method (G01C, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01C) }
    Return (C01C)
}
Method (G01D, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01D) }
    Return (C01D)
}
Method (G01E, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01E) }
    Return (C01E)
}
Method (G01F, 0, NotSerialized)
{
    If (BMOD == One) { Return (M01F) }
    Return (C01F)
}
Device (AC)
{
    Name (_HID, "ACPI0003")
    Name (_UID, Zero)
    Name (_PCL, Package () { \_SB })
    Method (_PSR, 0, NotSerialized)
    {
        If (!BMOK ()) { Return (\PWRS) }
        \PWRS = G007 ()
        Return (G007 ())
    }
    Method (_STA, 0, NotSerialized) { Return (0x0f) }
}
            

            

            

            

            

            

Name (B0I0, Zero)
Name (B0I1, Zero)
Name (B0I2, Zero)
Name (B0I3, Zero)
Name (B1I0, Zero)
Name (B1I1, Zero)
Name (B1I2, Zero)
Name (B1I3, Zero)
Name (BSWR, Zero)
Name (BSWA, Zero)
Method (BIUL, 1, NotSerialized)
            {
                Local0 = 0x0A
                While (Local0)
                {
                    Local1 = G008 ()
                    If ((Arg0 == Local1))
                    {
                        Break
                    }

                    Local0--
                    If ((Local0 == 0x00))
                    {
                        W009 (Arg0)
                    }

                    Sleep (0x14)
                }
            }

Method (GBIX, 3, NotSerialized)
            {
                If (!BMOK ()) { Arg2 = Zero }
                Acquire (ECLK, 0xFFFF)
                If (Arg2)
                {
                    W00A ((Arg0 | 0x01))
                    Sleep (0x14)
                    BIUL (G00A ())
                    Local7 = G016 ()
                    Arg1 [0x08] = Local7
                    Local7 = G014 ()
                    Local7 >>= 0x0F
                    Arg1 [0x01] = (Local7 ^ 0x01)
                    W00A (Arg0)
                    Sleep (0x14)
                    BIUL (G00A ())
                    If (Local7)
                    {
                        Local1 = (G00D () * 0x0A)
                    }
                    Else
                    {
                        Local1 = G00D ()
                    }

                    Arg1 [0x03] = Local1
                    W00A ((Arg0 | 0x02))
                    Sleep (0x14)
                    BIUL (G00A ())
                    If (Local7)
                    {
                        Local0 = (G017 () * 0x0A)
                    }
                    Else
                    {
                        Local0 = G017 ()
                    }

                    Arg1 [0x02] = Local0
                    Divide (Local1, 0x14, Local2, Arg1 [0x06])
                    If (Local7)
                    {
                        Arg1 [0x07] = 0x0640
                    }
                    ElseIf (G018 ())
                    {
                        Divide (0x00186A00, G018 (), Local2, Arg1 [0x07])
                    }
                    Else
                    {
                        Arg1 [0x07] = 0x00
                    }

                    Arg1 [0x05] = G018 ()
                    Local0 = G01C ()
                    Name (SERN, Buffer (0x06)
                    {
                        "     "
                    })
                    Local2 = 0x04
                    While (Local0)
                    {
                        Divide (Local0, 0x0A, Local1, Local0)
                        SERN [Local2] = (Local1 + 0x30)
                        Local2--
                    }

                    Arg1 [0x11] = SERN
                    W00A ((Arg0 | 0x06))
                    Sleep (0x14)
                    BIUL (G00A ())
                    Arg1 [0x10] = G01F ()
                    W00A ((Arg0 | 0x04))
                    Sleep (0x14)
                    BIUL (G00A ())
                    Name (BTYP, Buffer (0x05)
                    {
                         0x00, 0x00, 0x00, 0x00, 0x00
                    })
                    BTYP = G01D ()
                    Arg1 [0x12] = BTYP
                    W00A ((Arg0 | 0x05))
                    Sleep (0x14)
                    BIUL (G00A ())
                    Arg1 [0x13] = G01E ()
                }
                Else
                {
                    Arg1 [0x02] = 0xFFFFFFFF
                    Arg1 [0x06] = 0x00
                    Arg1 [0x07] = 0x00
                    Arg1 [0x03] = 0xFFFFFFFF
                }

                Release (ECLK)
                Return (Arg1)
            }

Method (GBST, 4, NotSerialized)
            {
                If (!BMOK ()) { Return (Arg3) }
                Acquire (ECLK, 0xFFFF)
                If ((Arg1 & 0x20))
                {
                    Local0 = 0x02
                }
                ElseIf ((Arg1 & 0x40))
                {
                    Local0 = 0x01
                }
                Else
                {
                    Local0 = 0x00
                }

                If ((Arg1 & 0x07)){}
                Else
                {
                    Local0 |= 0x04
                }

                If (((Arg1 & 0x07) == 0x07))
                {
                    Local0 = 0x04
                    Local1 = 0x00
                    Local2 = 0x00
                    Local3 = 0x00
                }
                Else
                {
                    Sleep (0x32)
                    W00A (Arg0)
                    Sleep (0x32)
                    Local6 = G00A ()
                    If ((Arg0 != Local6))
                    {
                        Release (ECLK)
                        Return (Arg3)
                    }

                    Local3 = G011 ()
                    If (Arg2)
                    {
                        Local2 = (G00C () * 0x0A)
                    }
                    Else
                    {
                        Local2 = G00C ()
                    }

                    Local1 = G010 ()
                    If ((Local1 >= 0x8000))
                    {
                        If ((Local0 & 0x01))
                        {
                            Local1 = (0x00010000 - Local1)
                        }
                        Else
                        {
                            Local1 = 0x00
                        }
                    }
                    ElseIf (!(Local0 & 0x02))
                    {
                        Local1 = 0x00
                    }

                    If (Arg2)
                    {
                        Local1 *= Local3
                        Divide (Local1, 0x03E8, , Local1)
                    }
                }

                Local5 = (0x01 << (Arg0 >> 0x04))
                BSWA |= BSWR
                If (((BSWA & Local5) == 0x00))
                {
                    Arg3 [0x00] = Local0
                    Arg3 [0x01] = Local1
                    Arg3 [0x02] = Local2
                    Arg3 [0x03] = Local3
                    If ((Arg0 == 0x00))
                    {
                        B0I0 = Local0
                        B0I1 = Local1
                        B0I2 = Local2
                        B0I3 = Local3
                    }
                    Else
                    {
                        B1I0 = Local0
                        B1I1 = Local1
                        B1I2 = Local2
                        B1I3 = Local3
                    }
                }
                Else
                {
                    If (\_SB.PCI0.LPCB.EC.AC._PSR ())
                    {
                        If ((Arg0 == 0x00))
                        {
                            Arg3 [0x00] = B0I0
                            Arg3 [0x01] = B0I1
                            Arg3 [0x02] = B0I2
                            Arg3 [0x03] = B0I3
                        }
                        Else
                        {
                            Arg3 [0x00] = B1I0
                            Arg3 [0x01] = B1I1
                            Arg3 [0x02] = B1I2
                            Arg3 [0x03] = B1I3
                        }
                    }
                    Else
                    {
                        Arg3 [0x00] = Local0
                        Arg3 [0x01] = Local1
                        Arg3 [0x02] = Local2
                        Arg3 [0x03] = Local3
                    }

                    If ((((Local0 & 0x04) == 0x00) && ((Local2 > 0x00) &&
                        (Local3 > 0x00))))
                    {
                        BSWA &= ~Local5
                        Arg3 [0x00] = Local0
                        Arg3 [0x01] = Local1
                        Arg3 [0x02] = Local2
                        Arg3 [0x03] = Local3
                    }
                }

                Release (ECLK)
                Return (Arg3)
            }

Method (AJTP, 3, NotSerialized)
            {
                Local0 = Arg1
                Acquire (ECLK, 0xFFFF)
                W00A (Arg0)
                Local1 = G00C ()
                Release (ECLK)
                If ((Arg0 == 0x00))
                {
                    Local2 = G003 ()
                }
                Else
                {
                    Local2 = G005 ()
                }

                If ((Local2 & 0x20))
                {
                    If ((Arg2 > 0x00))
                    {
                        Local0 += 0x01
                    }

                    If ((Local0 <= Local1))
                    {
                        Local0 = (Local1 + 0x01)
                    }
                }
                ElseIf ((Local2 & 0x40))
                {
                    If ((Local0 >= Local1))
                    {
                        Local0 = (Local1 - 0x01)
                    }
                }

                Return (Local0)
            }
Device (BAT0)
            {
                Name (_HID, EisaId ("PNP0C0A") )
                Name (_UID, 0x00)
                Name (_PCL, Package (0x01)
                {
                    \_SB
                })
                Name (B0ST, 0x00)
                Name (BT0I, Package (0x0D)
                {
                    0x00,
                    0xFFFFFFFF,
                    0xFFFFFFFF,
                    0x01,
                    0x2A30,
                    0x00,
                    0x00,
                    0x01,
                    0x01,
                    "",
                    "",
                    "",
                    ""
                })
                Name (BX0I, Package (0x15)
                {
                    0x01,
                    0x00,
                    0xFFFFFFFF,
                    0xFFFFFFFF,
                    0x01,
                    0xFFFFFFFF,
                    0x00,
                    0x00,
                    0xFFFFFFFF,
                    0x00017318,
                    0xFFFFFFFF,
                    0xFFFFFFFF,
                    0x03E8,
                    0x01F4,
                    0xFFFFFFFF,
                    0xFFFFFFFF,
                    "",
                    "",
                    "",
                    "",
                    0x00
                })
                Name (BT0P, Package (0x04) { Zero, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF })
                Method (_STA, 0, NotSerialized)
                {
                    If (!BMOK ()) { Return (0x0F) }
                    B0ST = G004 ()
                    If (B0ST) { Return (0x1F) }
                    Return (0x0F)
                }

                Method (_BIF, 0, NotSerialized)
                {
                    Acquire (ECLK, 0xFFFF)
                    Local7 = 0x00
                    Local6 = 0x0A
                    If (!BMOK ()) { Local6 = Zero }
                    While ((!Local7 && Local6))
                    {
                        If (G004 ())
                        {
                            If (((G003 () & 0x07) == 0x07))
                            {
                                Sleep (0x03E8)
                                Local6--
                            }
                            Else
                            {
                                Local7 = 0x01
                            }
                        }
                        Else
                        {
                            Local6 = 0x00
                        }
                    }

                    GBIX (0x00, BX0I, Local7)
                    BT0I [0x00] = DerefOf (BX0I [0x01])
                    BT0I [0x01] = DerefOf (BX0I [0x02])
                    BT0I [0x02] = DerefOf (BX0I [0x03])
                    BT0I [0x03] = DerefOf (BX0I [0x04])
                    BT0I [0x04] = DerefOf (BX0I [0x05])
                    BT0I [0x05] = DerefOf (BX0I [0x06])
                    BT0I [0x06] = DerefOf (BX0I [0x07])
                    BT0I [0x07] = DerefOf (BX0I [0x0E])
                    BT0I [0x08] = DerefOf (BX0I [0x0F])
                    BT0I [0x09] = DerefOf (BX0I [0x10])
                    BT0I [0x0A] = DerefOf (BX0I [0x11])
                    BT0I [0x0B] = DerefOf (BX0I [0x12])
                    BT0I [0x0C] = DerefOf (BX0I [0x13])
                    Release (ECLK)
                    Return (BT0I)
                }

                Method (_BIX, 0, NotSerialized)
                {
                    Acquire (ECLK, 0xFFFF)
                    Local7 = 0x00
                    Local6 = 0x0A
                    If (!BMOK ()) { Local6 = Zero }
                    While ((!Local7 && Local6))
                    {
                        If (G004 ())
                        {
                            If (((G003 () & 0x07) == 0x07))
                            {
                                Sleep (0x03E8)
                                Local6--
                            }
                            Else
                            {
                                Local7 = 0x01
                            }
                        }
                        Else
                        {
                            Local6 = 0x00
                        }
                    }

                    Local0 = GBIX (0x00, BX0I, Local7)
                    Release (ECLK)
                    Return (Local0)
                }

                Method (_BST, 0, NotSerialized)
                {
                    Acquire (ECLK, 0xFFFF)
                    If (!BMOK ()) { Release (ECLK) Return (BT0P) }
                    Local0 = (DerefOf (BX0I [0x01]) ^ 0x01)
                    Local1 = GBST (0x00, G003 (), Local0, BT0P)
                    Release (ECLK)
                    Return (Local1)
                }

                Method (_BTP, 1, NotSerialized)
                {
                    Acquire (ECLK, 0xFFFF)
                    If (!BMOK ()) { Release (ECLK) Return }
                    W000 (G000 () & (0xEF))
                    If (Arg0)
                    {
                        Local0 = 0x00
                        Local1 = Arg0
                        If (!DerefOf (BX0I [0x01]))
                        {
                            Divide (Local1, 0x0A, Local0, Local1)
                        }

                        Local1 = AJTP (0x00, Local1, Local0)
                        W002 ((Local1 & 0xFF))
                        W001 (((Local1 >> 0x08) & 0xFF))
                        W000 (G000 () | (0x10))
                    }
                    Release (ECLK)
                }
            }

/* Vendor CLPM / CPU power callbacks are bridged to coreboot PNOT. */
Method (_Q22, 0, NotSerialized)
{
    \PNOT ()
    If (BMOK ()) { If (G004 ()) { Notify (BAT0, 0x80) } }
}
Method (_Q4A, 0, NotSerialized) { \PNOT () Notify (BAT0, 0x81) }
Method (_Q4B, 0, NotSerialized) { \PNOT () Notify (BAT0, 0x80) }
Method (_Q24, 0, NotSerialized) { \PNOT () Notify (BAT0, 0x80) }
Method (_Q26, 0, NotSerialized)
{ Sleep (500) Notify (AC, 0x80) \PWRS = One \PNOT () }
Method (_Q27, 0, NotSerialized)
{ Sleep (500) Notify (AC, 0x80) \PWRS = Zero \PNOT () }
