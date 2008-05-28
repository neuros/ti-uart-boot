/****************************************************************
 *  TI DVEVM Serial Boot/Flash Host Program - CRC-32 code       *
 *  (C) 2006, Texas Instruments, Inc.                           *
 *                                                              *
 * Author:  Daniel Allred (d-allred@ti.com)                     *
 * History: 09/19/2006 - Version 0.1 - Initial Release          *
 *                                                              *
 ****************************************************************/

using System;
using System.Text;
using System.IO.Ports;
using System.Threading;

namespace DVFlasher
{
    public class CRC32
    {
        #region Data members

        private UInt32[] lut;
        private UInt32 poly = 0x04C11DB7; //Bit32 is 1 is always 1 and therefore not needed
        private UInt32 initReg = 0xFFFFFFFF;
        private UInt32 finalReg = 0xFFFFFFFF;
        private Boolean reflected = true; //follows hardware convention that receive bits in reverse order 
        private Int32 numBytesPerRegShift = 1;

        #endregion

        #region Indexer and Properties
        
        public UInt32 this[int index]
        {
            get{
                return lut[index];
            }
        }
        
        public Int32 Length
        {
            get{
                return lut.Length;
            }
        }
        
        private Int32 NumBytesPerRegShift
        {
            get { return numBytesPerRegShift; }
            set { numBytesPerRegShift = ((value < 3) && (value > 0)) ? value : 1; }
        }
        
        #endregion

        #region Constructors
        /// <summary>
        /// Basic Constructor to generate the standard CRC-32 (used in ethernet packets, Zip files, etc.)
        /// </summary>
        public CRC32()
        {
            poly = 0x04C11DB7; //Bit32 is 1 by default
            initReg = 0xFFFFFFFF;
            finalReg = 0xFFFFFFFF;
            reflected = true; //follows hardware convention that receive bits in reverse order 
            numBytesPerRegShift = 1;
            BuildTable();
        }

        /// <summary>
        /// Basic Constructor except with a different key (divisor) polynomial
        /// </summary>
        /// <param name="KeyPoly">The user-provided key polynomial (hex number of bits[31-0])</param>
        public CRC32(UInt32 KeyPoly)
        {
            poly = KeyPoly;
            BuildTable();
        }

        /// <summary>
        /// Constructor for custom 32-bit CRC object - Don't use this unless you really know
        /// what the heck you are doing.
        /// </summary>
        /// <param name="KeyPoly">Custom key polynomial</param>
        /// <param name="InitialRegVal">Initial register value</param>
        /// <param name="FinalizeXorVal">Value that is xor'd with final CRC</param>
        /// <param name="Reflected">Indicates whether the algorithm expects reflected input bytes</param>
        /// <param name="BytesShiftedPerCycle">How many bytes are handled at a time (1 or 2).
        /// The internal table size is determined by this parameter, 1 is most common (leads to 1kbyte table).</param>
        public CRC32(UInt32 KeyPoly,
            UInt32 InitialRegVal,
            UInt32 FinalizeXorVal,
            Boolean Reflected,
            Int32 BytesShiftedPerCycle)
        {
            poly = KeyPoly;
            initReg = InitialRegVal;
            finalReg = FinalizeXorVal;
            reflected = Reflected;
            NumBytesPerRegShift = BytesShiftedPerCycle;
            BuildTable();
        }
        #endregion

        #region Table building method used by Constructors
        /// <summary>
        /// Function to generate the table of values used in the CRC-32 calculation
        /// </summary>
        private void BuildTable()
        {
            Int32 NumBitsPerRegShift = NumBytesPerRegShift*8;
            Int32 tableLen = (Int32) Math.Pow(2.0,NumBitsPerRegShift);
            UInt32 crcAccum;
            lut = new UInt32[tableLen];

            if (reflected)
            {
                for (UInt32 i = 0; i < tableLen; i++)
                {
                    crcAccum = ReflectNum(i, NumBitsPerRegShift) << (32 - NumBitsPerRegShift);
                    for (Byte j = 0; j < NumBitsPerRegShift; j++)
                    {
                        if ((crcAccum & 0x80000000) != 0x00000000)
                        {
                            crcAccum = (crcAccum << 1) ^ poly;
                        }
                        else
                        {
                            crcAccum = (crcAccum << 1);
                        }
                        lut[i] = ReflectNum(crcAccum, 32);
                    }
                }
            }
            else
            {
                for (UInt32 i = 0; i < tableLen; i++)
                {
                    crcAccum = i << (32 - NumBitsPerRegShift);
                    for (Byte j = 0; j < NumBitsPerRegShift; j++)
                    {
                        if ((crcAccum & 0x80000000) != 0x00000000)
                        {
                            crcAccum = (crcAccum << 1) ^ poly;
                        }
                        else
                        {
                            crcAccum = (crcAccum << 1);
                        }
                        lut[i] = crcAccum;
                    }
                }
            }
        }
        #endregion
                                       
        #region Public Methods
        
        /// <summary>
        /// Calculate the CRC-32 checksum on the given array of bytes
        /// </summary>
        /// <param name="Data">Array of bytes of data.</param>
        /// <returns>The 32-bit CRC of the data.</returns>
        public UInt32 CalculateCRC(Byte[] Data)
        {
            UInt32 crc = initReg;
            Int32 len = Data.Length;
            Int32 NumBitsPerRegShift = NumBytesPerRegShift * 8;
            UInt32 Mask = (UInt32)(Math.Pow(2.0, NumBitsPerRegShift) - 1);

            // Perform the algorithm on each byte
            if (reflected)
            {
                if (NumBytesPerRegShift == 2)
                {
                    for (Int32 i = 0; i < (len / NumBytesPerRegShift); i++)
                    {
                        crc = (crc >> NumBitsPerRegShift) ^ lut[(crc & Mask) ^ ((Data[2 * i + 1] << 8) | Data[2 * i])];
                    }
                }
                else
                {
                    for (Int32 i = 0; i < len; i++)
                    {
                        crc = (crc >> NumBitsPerRegShift) ^ lut[(crc & Mask) ^ Data[i]];
                    }
                }
            }
            else
            {
                if (NumBytesPerRegShift == 2)
                {
                    for (Int32 i = 0; i < (len / NumBytesPerRegShift); i++)
                    {
                        crc = (crc << NumBitsPerRegShift) ^ lut[((crc >> (32 - NumBitsPerRegShift)) & Mask) ^ ((Data[2 * i] << 8) | Data[2 * i + 1])];
                    }
                }
                else
                {
                    for (Int32 i = 0; i < len; i++)
                    {
                        crc = (crc << NumBitsPerRegShift) ^ lut[((crc >> (32 - NumBitsPerRegShift)) & Mask) ^ Data[i]];
                    }
                }
            }
            // Exclusive OR the result with the specified value
            return (crc ^ finalReg);
        }

        /// <summary>
        /// Method to reflect a specified number of bits in the integer
        /// </summary>
        /// <param name="inVal">The unsigned integer value input</param>
        /// <param name="num">The number of lower bits to reflect.</param>
        /// <returns></returns>
        public UInt32 ReflectNum(UInt32 inVal, Int32 num)
        {
            UInt32 outVal = 0x0;

            for (Int32 i = 1; i < (num + 1); i++)
            {
                if ((inVal & 0x1) != 0x0)
                {
                    outVal |= (UInt32)(0x1 << (num - i));
                }
                inVal >>= 1;
            }
            return outVal;
        }
                
        #endregion


    }
}
