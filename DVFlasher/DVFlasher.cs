/****************************************************************
 *  TI DM644x Serial Boot/Flash Host Program                     
 *  (C) 2006,2007, Texas Instruments, Inc.                           
 *                                                              
 * Author:  Daniel Allred
 * History: 09/19/2006 - Version 0.1 - Initial Release          
 *          10/2/2006 - Added UART UBL as embedded resource
 *          10/19/2006 - Internal release
 *          1/22/2007 - v1.00 release
 *          2/01/2007 - v1.10 release
 *              Revised so that the UBL can be used in CCS flashing
 *              tool.  Required revising this code so that the first
 *              256 bytes of the UBL binary was trimmed for the NAND
 *              and UART UBLs.
 *                                                              
 ****************************************************************/

using System;
using System.Text;
using System.IO;
using System.IO.Ports;
using System.Reflection;
using System.Threading;
using System.Globalization;

namespace DVFlasher
{
    /// <summary>
    /// Enumeration for Magic Flags that the UBL expects to see
    /// </summary>
    public enum MagicFlags : uint
    {
        MAGIC_NUMBER_VALID = 0xA1ACED00,
        MAGIC_NUMBER_INVALID = 0xFFFFFFFF,
        UBL_MAGIC_SAFE = 0xA1ACED00,
        UBL_MAGIC_DMA = 0xA1ACED11,	                /* DMA boot mode */
        UBL_MAGIC_IC = 0xA1ACED22,	                /* I Cache boot mode */
        UBL_MAGIC_FAST = 0xA1ACED33,	            /* Fast EMIF boot mode */
        UBL_MAGIC_DMA_IC = 0xA1ACED44,	            /* DMA + ICache boot mode */
        UBL_MAGIC_DMA_IC_FAST = 0xA1ACED55,	        /* DMA + ICache + Fast EMIF boot mode */
        UBL_MAGIC_BIN_IMG = 0xA1ACED66,             /* Describes the application image in Flash - indicates that it is binary*/
        UBL_MAGIC_NOR_RESTORE = 0xA1ACED77,         /* Download via UART & Restore NOR with binary data */
        UBL_MAGIC_NOR_SREC_BURN = 0xA1ACED88,       /* Download via UART & Burn NOR with UBL readable header and SREC data*/
        UBL_MAGIC_NOR_BIN_BURN = 0xA1ACED99,        /* Download via UART & Burn NOR with UBL readable header and BIN data */
        UBL_MAGIC_NOR_GLOBAL_ERASE = 0xA1ACEDAA,    /* Download via UART & Global erase the NOR Flash */
        UBL_MAGIC_NAND_SREC_BURN = 0xA1ACEDBB,   /* Download via UART & Burn NAND - Image is S-record */
        UBL_MAGIC_NAND_BIN_BURN = 0xA1ACEDCC,   /* Download via UART & Burn NAND - Image is binary */
        UBL_MAGIC_NAND_GLOBAL_ERASE = 0xA1ACEDDD	/* Download via UART & Global erase the NAND Flash */
    };
    
    /// <summary>
    /// Enumeration of flash types
    /// </summary>
    public enum FlashType : uint
    {
        NONE,
        NOR,
        NAND
    };

    /// <summary>
    /// Structure to hold command parameters
    /// </summary>
    struct ProgramCmdParams
    {
        /// <summary>
        /// Flag to indicate if command line is valid
        /// </summary>
        public Boolean Valid;

        /// <summary>
        /// Boolean to control the verbosity of output
        /// </summary>
        public Boolean Verbose;

        /// <summary>
        /// Name of serial port used for communications
        /// </summary>
        public String SerialPortName;

        /// <summary>
        /// MagicFlag which is the command to of what the UART UBL will do.
        /// This should be transmitted alone in response to the BOOTPSP.
        /// </summary>
        public MagicFlags CMDMagicFlag;

        /// <summary>
        /// Flag to inidicate whether to use internal (TI) UBL or externally provided one
        /// </summary>
        public Boolean useEmbeddedUBL;

        /// <summary>
        /// Address at which UART UBL will begin execution (must be 0x100 or greater)
        /// </summary>
        public UInt32 UARTUBLExecAddr;

        /// <summary>
        /// Boolean to indicate whether or not to send UART UBL via RBL interfacing.
        /// This is set by the -skipRBL command line option.
        /// </summary>
        public Boolean UARTUBLUsed;

        /// <summary>
        /// Type of flash that the application and UBL are targeted to use.  Selects
        /// which embedded UBL to use.
        /// </summary>
        public FlashType UBLFlashType;

        /// <summary>
        /// Entry point addresses for the NAND and NOR UBL embedded binary images.
        /// </summary>
        public UInt32 NORUBLExecAddr;
        public UInt32 NANDUBLExecAddr;

        /// <summary>
        /// Global varibale to hold the desired magic flag
        /// </summary>
        public MagicFlags FLASHUBLMagicFlag;

        /// <summary>
        /// String containing filename of FLASH UBL file (only needed for flashing)
        /// </summary>
        public String FLASHUBLFileName;

        /// <summary>
        /// Address where the flash ubl file should be decoded to and 
        /// run from (if appropriate).
        /// </summary>
        public UInt32 FLASHUBLLoadAddr;

        /// <summary>
        /// Address at which the Flash UBL will begin execution (must be 0x100 or greater)
        /// This will either be the same as UARTUBLExecAddr (id embedded UBL is put into flash)
        /// or provided on the command line in case a separate flash UBL is provided.
        /// </summary>
        public UInt32 FLASHUBLExecAddr;

        /// <summary>
        /// Magic Flag for the application data
        /// </summary>
        public MagicFlags APPMagicFlag;

        /// <summary>
        /// String containing filename of Application file
        /// </summary>
        public String APPFileName;

        /// <summary>
        /// Address where the app should be decoded to and 
        /// run from (if appropriate).
        /// </summary>
        public UInt32 APPLoadAddr;
        /// <summary>
        /// Address where the app begin execution 
        /// </summary>
        public UInt32 APPEntryPoint;
    }
    
    /// <summary>
    /// Main program Class
    /// </summary>
    class Program
    {
        //**********************************************************************************
        #region Class variables and members

        /// <summary>
        /// Global main Serial Port Object
        /// </summary>
        public static SerialPort MySP;
                
        /// <summary>
        /// The main thread used to actually execute everything
        /// </summary>
        public static Thread workerThread;

        /// <summary>
        /// Global boolean to indicate successful completion of workerThread
        /// </summary>
        public static Boolean workerThreadSucceeded = false;

        /// <summary>
        /// Public variable to hold needed command line and program parameters
        /// </summary>
        public static ProgramCmdParams cmdParams;

        /// <summary>
        /// String to hold the summary of operation program will attempt.
        /// </summary>
        public static String cmdString;

        #endregion
        //**********************************************************************************


        //**********************************************************************************
        #region Code for Main thread

        /// <summary>
        /// Help Display Function
        /// </summary>
        private static void DispHelp()
        {
            Console.Write("Usage:");
            Console.Write("\n\tDVFlasher <Option>");
            Console.Write("\n\t\t" + "<Option> can be the following:");
            Console.Write("\n\t\t\t" + "-enor\tDo a global erase of the NOR flash.");
            Console.Write("\n\t\t\t" + "-enand\tDo a global erase of the NAND flash.");
            Console.WriteLine();

            Console.Write("\n\tDVFlasher <Option> <Application File>");
            Console.Write("\n\t\t" + "<Option> can be any of the following:");
            Console.Write("\n\t\t\t" + "-b\tBoot and run application code from the DDR RAM (max 16MB)." );
            Console.Write("\n\t\t\t" + "-r\tRestore NOR Flash with bootable application (typically u-boot).");
            Console.WriteLine();

            Console.Write("\n\tDVFlasher <Option> <Application File>");
            Console.Write("\n\tDVFlasher <Option> -useMyUBL <Flash UBL File> <Application File>");
            Console.Write("\n\t\t" + "<Option> can be any of the following:" +
                          "\n\t\t\t" + "-fnorbin\tFlash the NOR Flash with bootable UBL and binary application image." +
                          "\n\t\t\t" + "-fnorsrec\tFlash the NOR Flash with bootable UBL and S-record application image." +
                          "\n\t\t\t" + "-fnandbin\tFlash the NAND Flash with bootable UBL and binary application image." +
                          "\n\t\t\t" + "-fnandsrec\tFlash the NAND Flash with bootable UBL and S-record application image.\n");
            Console.Write("\n\t\t"+"<Flash UBL File> is a maximum 14kB second stage bootloader that" +
                          "\n\t\t" + "is specifically tailored to sit in the NAND or NOR flash and load" + 
                          "\n\t\t" + "the application image stored there.\n");
            Console.Write("\n\t\t"+"<Application File> is an S-record or binary executable file that"+
                          "\n\t\t"+"is run after boot or programmed into flash for execution there. Often"+
                          "\n\t\t"+"this will be a third-stage bootloader like u-boot, which can be used to"+
                          "\n\t\t"+"boot linux.\n");
            Console.Write("\t" + "NOTE: Make sure switches and jumpers are set appropriately for Flash type.\n");

            Console.Write("\n\t" + "Other Options" +
                          "\n\t\t"+"-h                \tDisplay this help screen."+
                          "\n\t\t"+"-v                \tDisplay more verbose output returned from the DVEVM."+
                          "\n\t\t"+"-noRBL            \tUse when system is already running UBL, showing \"BOOTPSP\"." +
                          "\n\t\t"+"-useMyUBL         \tUse your own provided Flash UBL file instead of the internal UBL." +
                          "\n\t\t"+"                  \tExamples of this usage are shown above." +
                          "\n\t\t"+"-p \"<PortName>\" \tUse <PortName> as the serial port (e.g. COM2, /dev/ttyS1)."+
                          "\n\t\t"+"-s \"<StartAddr>\"\tUse <StartAddr>(hex) as the point of execution for the system");
        }   
 
        /// <summary>
        /// Parse the command line into the appropriate internal command structure
        /// </summary>
        /// <param name="args">The array of strings passed to the command line.</param>
        public static ProgramCmdParams ParseCmdLine(String[] args)
        {
            ProgramCmdParams myCmdParams =  new ProgramCmdParams();
            Boolean[] argsHandled = new Boolean[args.Length];
            Int32 numFiles = -1;
            UInt32 numUnhandledArgs,numHandledArgs=0;
            String s;

            if (args.Length == 0)
            {
                myCmdParams.Valid = false;
                return myCmdParams;
            }

            // Initialize array of handled argument booleans to false
            for (int i = 0; i < argsHandled.Length; i++ )
                argsHandled[i] = false;

            // Set Defualts for application
            myCmdParams.UBLFlashType = FlashType.NONE;
            
            myCmdParams.CMDMagicFlag = MagicFlags.MAGIC_NUMBER_INVALID;
            myCmdParams.Valid = true;
            myCmdParams.Verbose = false;
            myCmdParams.SerialPortName = null;
            myCmdParams.useEmbeddedUBL = true;

            myCmdParams.UARTUBLExecAddr = 0x0100;
            myCmdParams.UARTUBLUsed = true;
            
            myCmdParams.NORUBLExecAddr = 0x29e8;
            myCmdParams.NANDUBLExecAddr = 0x236c;

            myCmdParams.APPMagicFlag = MagicFlags.UBL_MAGIC_SAFE;
            myCmdParams.APPFileName = null;
            myCmdParams.APPLoadAddr = 0xFFFFFFFF;
            myCmdParams.APPEntryPoint = 0xFFFFFFFF;

            myCmdParams.FLASHUBLMagicFlag = MagicFlags.UBL_MAGIC_SAFE;
            myCmdParams.FLASHUBLFileName = null;
            myCmdParams.FLASHUBLLoadAddr = 0xFFFFFFFF;
            myCmdParams.FLASHUBLExecAddr = 0x0100;
            

            // For loop for all dash options
            for(int i = 0; i<args.Length; i++)
            {
                s = args[i];
                if (s.StartsWith("-"))
                {
                    switch (s.Substring(1).ToLower())
                    {
                        case "fnorbin":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NOR_BIN_BURN;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 2;
                            myCmdParams.UBLFlashType = FlashType.NOR;
                            cmdString = "Flashing NOR with ";
                            break;
                        case "fnorsrec":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NOR_SREC_BURN;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 2;
                            myCmdParams.UBLFlashType = FlashType.NOR;
                            cmdString = "Flashing NOR with ";
                            break;
                        case "fnandbin":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NAND_BIN_BURN;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 2;
                            myCmdParams.UBLFlashType = FlashType.NAND;
                            cmdString = "Flashing NAND with ";
                            break;
                        case "fnandsrec":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NAND_SREC_BURN;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 2;
                            myCmdParams.UBLFlashType = FlashType.NAND;
                            cmdString = "Flashing NAND with ";
                            break;
                        case "enor":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NOR_GLOBAL_ERASE;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 0;
                            myCmdParams.UBLFlashType = FlashType.NOR;
                            cmdString = "Globally erasing NOR flash.";
                            break;
                        case "enand":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NAND_GLOBAL_ERASE;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 0;
                            myCmdParams.UBLFlashType = FlashType.NAND;
                            cmdString = "Globally erasing NAND flash.";
                            break;
                        case "r":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_NOR_RESTORE;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 1;
                            myCmdParams.UBLFlashType = FlashType.NOR;
                            cmdString = "Restoring NOR flash with ";
                            break;
                        case "b":
                            if (myCmdParams.CMDMagicFlag == MagicFlags.MAGIC_NUMBER_INVALID)
                                myCmdParams.CMDMagicFlag = MagicFlags.UBL_MAGIC_SAFE;
                            else
                                myCmdParams.Valid = false;
                            numFiles = 1;
                            myCmdParams.UBLFlashType = FlashType.NOR;
                            cmdString = "Sending and running application found in ";
                            break;
                        case "s":
                            if (args[i + 1].Contains("0x"))
                                myCmdParams.APPEntryPoint = UInt32.Parse(args[i + 1].Replace("0x", ""), NumberStyles.AllowHexSpecifier);
                            else if (args[i + 1].Contains("0X"))
                                myCmdParams.APPEntryPoint = UInt32.Parse(args[i + 1].Replace("0X", ""), NumberStyles.AllowHexSpecifier);
                            else
                                myCmdParams.APPEntryPoint = UInt32.Parse(args[i + 1], NumberStyles.AllowHexSpecifier);
                            Console.WriteLine("Using {0:x8} as the execution address.", myCmdParams.APPEntryPoint);
                            argsHandled[i+1] = true;
                            numHandledArgs++;
                            break;
                        case "p":
                            myCmdParams.SerialPortName = args[i + 1];
                            argsHandled[i + 1] = true;
                            numHandledArgs++;
                            break;
                        case "norbl":
                            myCmdParams.UARTUBLUsed = false;
                            break;
                        case "usemyubl":
                            myCmdParams.useEmbeddedUBL = false;
                            break;
                        case "v":
                            myCmdParams.Verbose = true;
                            break;
                        default:
                            myCmdParams.Valid = false;
                            break;
                    }
                    argsHandled[i] = true;
                    numHandledArgs++;
                    //Console.WriteLine("Argument: {0}, Handled: {1}, Valid: {2}", args[i], argsHandled[i], myCmdParams.Valid);
                    if (!myCmdParams.Valid)
                        return myCmdParams;
                }
                              

            } // end of for loop for handling dash params

            // Check if we are using the embedded UBLs, if so adjust required file numbers
            if ((myCmdParams.useEmbeddedUBL) && (numFiles == 2))
            {
                numFiles = 1;
            }

            if (myCmdParams.UBLFlashType == FlashType.NOR)
            {
                
                myCmdParams.UARTUBLExecAddr = myCmdParams.NORUBLExecAddr;
            }
            else if (myCmdParams.UBLFlashType == FlashType.NAND)
            {
                myCmdParams.UARTUBLExecAddr = myCmdParams.NANDUBLExecAddr;
            }

            if (myCmdParams.useEmbeddedUBL)
            {
                myCmdParams.FLASHUBLExecAddr = myCmdParams.UARTUBLExecAddr;
            }
            

            // Verify that the number of unhandled arguments is equal to numFiles
            // If not, then there is a problem.
            numUnhandledArgs = (UInt32) (args.Length - numHandledArgs);
            if (numUnhandledArgs != numFiles)
            {
                myCmdParams.Valid = false;
                return myCmdParams;
            }
                      
            // This for loop handles all othe params (namely filenames)
            for (int i = 0; i < args.Length; i++)
            {
                if (!argsHandled[i])
                {
                    if (myCmdParams.useEmbeddedUBL)
                    {
                        if (numFiles == 0)
                            myCmdParams.Valid = false;
                        else if (myCmdParams.APPFileName == null)
                        {
                            myCmdParams.APPFileName = args[i];
                            cmdString += myCmdParams.APPFileName + ".";
                        }
                        else
                            myCmdParams.Valid = false;
                    }
                    else
                    {
                        switch (numFiles)
                        {
                            case 1:
                                if (myCmdParams.APPFileName == null)
                                {
                                    myCmdParams.APPFileName = args[i];
                                    cmdString += myCmdParams.APPFileName + ".";
                                }
                                else
                                    myCmdParams.Valid = false;
                                break;
                            case 2:
                                if (myCmdParams.FLASHUBLFileName == null)
                                {
                                    myCmdParams.FLASHUBLFileName = args[i];
                                    cmdString += myCmdParams.FLASHUBLFileName + " and ";
                                }
                                else if (myCmdParams.APPFileName == null)
                                {
                                    myCmdParams.APPFileName = args[i];
                                    cmdString += myCmdParams.APPFileName + ".";
                                }
                                else
                                    myCmdParams.Valid = false;
                                break;
                            default:
                                myCmdParams.Valid = false;
                                break;
                        }
                    }    
                    argsHandled[i] = true;
                    if (!myCmdParams.Valid)
                        return myCmdParams;
                }
            } // end of for loop for handling dash params
            
            // Set default binary execution address on target DVEVM
            if (myCmdParams.APPLoadAddr == 0xFFFFFFFF)
                myCmdParams.APPLoadAddr = 0x81080000;

            if (myCmdParams.APPEntryPoint == 0xFFFFFFFF)
                myCmdParams.APPEntryPoint = 0x81080000;

            if (myCmdParams.FLASHUBLLoadAddr == 0xFFFFFFFF)
                myCmdParams.FLASHUBLLoadAddr = 0x81070000;
            
            //Setup default serial port name
            if (myCmdParams.SerialPortName == null)
            {
                int p = (int)Environment.OSVersion.Platform;
                if ((p == 4) || (p == 128)) //Check for unix
                {
                    Console.WriteLine("Platform is Unix/Linux.");
                    myCmdParams.SerialPortName = "/dev/ttyS0";
                }
                else
                {
                    Console.WriteLine("Platform is Windows.");
                    myCmdParams.SerialPortName = "COM1";
                }
            }

            return myCmdParams;
        }

        /// <summary>
        /// Main entry point of application
        /// </summary>
        /// <param name="args">Array of command-line arguments</param>
        /// <returns>Return code: 0 for correct exit, -1 for unexpected exit</returns>
        static Int32 Main(String[] args)
        {
            // Begin main code
            Console.Clear();
            Console.WriteLine("-----------------------------------------------------");
            Console.WriteLine("   TI DVFlasher Host Program for DM644x              ");
            Console.WriteLine("   (C) 2007, Texas Instruments, Inc.                 ");
            Console.WriteLine("-----------------------------------------------------");
            Console.Write("\n\n");

            // Parse command line
            cmdParams = ParseCmdLine(args);
            if (!cmdParams.Valid)
            {
                DispHelp();
                return -1;
            }
            else
            {
                Console.Write(cmdString + "\n\n\n");
            }
                                   
            try
            {
                Console.WriteLine("Attempting to connect to device " + cmdParams.SerialPortName + "...");
                MySP = new SerialPort(cmdParams.SerialPortName, 115200, Parity.None, 8, StopBits.One);
                MySP.Encoding = Encoding.ASCII;
                MySP.Open();
            }
            catch(Exception e)
            {
                if (e is UnauthorizedAccessException)
                {
                    Console.WriteLine(e.Message);
                    Console.WriteLine("This application failed to open the COM port.");
                    Console.WriteLine("Most likely it is in use by some other application.");
                    return -1;
                }
                Console.WriteLine(e.Message);
                return -1;
            }


            Console.WriteLine("Press any key to end this program at any time.\n");
            
            // Setup the thread that will actually do all the work of interfacing to
            // the DM644x boot ROM.  Start that thread.
            workerThread = new Thread(new ThreadStart(Program.WorkerThreadStart));
            workerThread.Start();

            // Wait for a key to terminate the program
            while ((workerThread.IsAlive) && (!Console.KeyAvailable))
            {
                Thread.Sleep(1000);
            }
                       
            // If a key is pressed then abort the worker thread and close the serial port
            try
            {
                if (Console.KeyAvailable)
                {
                    Console.ReadKey();
                    Console.WriteLine("Aborting program...");
                    workerThread.Abort();
                }
                else if (workerThread.IsAlive)
                {
                    Console.WriteLine("Aborting program...");
                    workerThread.Abort();
                }
                
                while ((workerThread.ThreadState & ThreadState.Stopped) != ThreadState.Stopped){}
            }
            catch (Exception e)
            {
                Console.WriteLine("Abort thread error...");
                Console.WriteLine(e.GetType());
                Console.WriteLine(e.Message);
            }
            
            if (workerThreadSucceeded)
            {
                Console.WriteLine("\nOperation completed successfully.");
                return 0;
            }
            else
            {
                Console.WriteLine("\n\nInterfacing to the DVEVM via UART failed." +
                    "\nPlease reset or power-cycle the board and try again...");
                return -1;
            }
            
        }

        #endregion
        //**********************************************************************************
        

        //**********************************************************************************
        #region Code for UART interfacing thread

        /// <summary>
        /// The main fucntion of the thread where all the cool stuff happens
        /// to interface with the DVEVM
        /// </summary>
        public static void WorkerThreadStart()
        {
            // Try transmitting the first stage boot-loader (UBL) via the RBL
            try
            {
                if (cmdParams.UARTUBLUsed)
                    TransmitUARTUBL();
            }
            catch (Exception e)
            {
                if (e is ThreadAbortException)
                {
                    Thread.Sleep(1000);
                }
                else
                {
                    Console.WriteLine(e.Message);
                }
                return;
            }

            // Sleep in case we need to abort
            Thread.Sleep(200);
            
            // Code to perform specified command
            try
            {
                // Wait for the bootmode to be sent
                if (!waitForSequence("PSPBootMode = UART", "PSPBootMode = N", MySP, true))
                {
                    Console.WriteLine("\nWARNING! The DM644x is NOT in UART boot mode!");
                    Console.WriteLine("Only continue if you are sure of what you are doing.");
                    Console.Write("\n\tContinue (Y/N) ? ");
                    if (!String.Equals(
                            Console.ReadKey().Key.ToString(),
                            "y",
                            StringComparison.CurrentCultureIgnoreCase)
                        )
                    {
                        Console.WriteLine("\n\nCheck your switch and jumper settings, if appropriate.");
                        Thread.CurrentThread.Abort();
                    }
                    else
                        Console.WriteLine();
                }
                
                // Clear input buffer so we can start looking for BOOTPSP
                MySP.DiscardInBuffer();

                // Take appropriate action depending on command
                switch (cmdParams.CMDMagicFlag)
                {
                    case MagicFlags.UBL_MAGIC_NAND_BIN_BURN:
                        {
                            TransmitFLASHUBLandAPP();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NAND_SREC_BURN:
                        {
                            TransmitFLASHUBLandAPP();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NOR_BIN_BURN:
                        {
                            TransmitFLASHUBLandAPP();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NOR_SREC_BURN:
                        {
                            TransmitFLASHUBLandAPP();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NOR_GLOBAL_ERASE:
                        {
                            TransmitErase();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NAND_GLOBAL_ERASE:
                        {
                            TransmitErase();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_NOR_RESTORE:
                        {
                            TransmitAPP();
                            break;
                        }
                    case MagicFlags.UBL_MAGIC_SAFE:
                        {
                            TransmitAPP();
                            break;
                        }
                    default:
                        {
                            Console.WriteLine("Command not recognized!");
                            break;
                        }
                }
            }
            catch (Exception e)
            {
                if (e is ThreadAbortException)
                {
                    Thread.Sleep(1000);
                }
                else
                {
                    Console.WriteLine(e.Message);
                }
                return;
            }
            
            // Everything worked, so change boolean status
            workerThreadSucceeded = true;
        }

        /// <summary>
        /// Read the appropriate embedded UBL data (NAND or NOR) that
        /// will be transmitted to the DM644x.
        /// </summary>
        /// <returns>A Stream object of the UBL data</returns>
        private static Stream GetEmbeddedUBLStream()
        {
            String srchStr;
            Stream UBLstream;
            String UBLResourceName = "";

            Assembly executingAssembly = Assembly.GetExecutingAssembly();

            if (cmdParams.UBLFlashType == FlashType.NAND)
                srchStr = "ubl_davinci_nand.bin";
            else
                srchStr = "ubl_davinci_nor.bin";

            executingAssembly = Assembly.GetExecutingAssembly();
            foreach (String s in executingAssembly.GetManifestResourceNames())
            {
                if (s.Contains(srchStr))
                {
                    UBLResourceName = s;
                    break;
                }
            }
            try
            {
                UBLstream = executingAssembly.GetManifestResourceStream(UBLResourceName);
            }
            catch (FileNotFoundException e)
            {
                Console.WriteLine("The embedded UBL file was not found.");
                throw e;
            }
            return UBLstream;
        }

        /// <summary>
        /// Function to find, read, and convert to S-record (if needed) a data file
        /// </summary>
        /// <param name="filename">The name of the file to load</param>
        /// <param name="decAddr">The address to which the data should be loaded in memory
        /// on the DM644x device.
        /// </param>
        /// <returns></returns>
        private static Byte[] GetFileData(String filename, UInt32 decAddr, Boolean stripData)
        {
            FileStream fs;
            Byte[] data;

            if (!File.Exists(filename))
            {
                throw new FileNotFoundException("File " + filename + " is not present.");
            }

            // Open file and setup the binary stream reader
            fs = File.Open(cmdParams.APPFileName, FileMode.Open, FileAccess.Read);

            // Check to see if the file is an s-record file
            if (isFileSrec(fs))
            {
                data = readSrec(fs);
            }
            else //Assume the file is a binary file
            {
                data = bin2srec(fs, decAddr, stripData);
            }
            return data;
        }

        /// <summary>
        /// Function to Transmit the UBL via the DM644x ROM Serial boot
        /// </summary>
        private static void TransmitUARTUBL()
        {
            // Local Variables for reading UBL file
            Stream UBLstream;
            BinaryReader UBLbr;
            StringBuilder UBLsb;
            Byte[] UBLFileData;
            UInt32 UBLcrc;
            UInt32 data;          
            CRC32 MyCRC;

            // Access the appropriate embedded UBL
            UBLstream = GetEmbeddedUBLStream();
                        
            // Open UBL UART stream and setup the binary stream reader
            UBLbr = new BinaryReader(UBLstream);
            
            // Create the byte array and stringbuilder for holding UBL file data
            UBLFileData = new Byte[(UBLstream.Length-256)];
            UBLsb = new StringBuilder((UBLFileData.Length-256) * 2);

            // Skip the first 0x100 bytes since they contain slef-copy stuff
            // This is required to mimic the SDI flashwriter programs (Ugh!)
            UBLbr.BaseStream.Seek(256, SeekOrigin.Begin);
            
            // Read the data from the UBL file into the appropriate structures
            for (int i = 0; i < ((UBLstream.Length-256) / sizeof(UInt32)); i++)
            {
                data = UBLbr.ReadUInt32();
                System.BitConverter.GetBytes(data).CopyTo(UBLFileData, i * sizeof(UInt32));
                UBLsb.AppendFormat("{0:X8}", data);
            }

            // Create CRC object and use it to calculate the UBL file's CRC
            // Not that this CRC is not quite the standard CRC-32 algorithm
            // commonly is use since the final register value is not XOR'd
            // with 0xFFFFFFFF.  As a result the CRC value returned here
            // will be the bitwise inverse of the standard CRC-32 value.
            MyCRC = new CRC32(0x04C11DB7, 0xFFFFFFFF, 0x00000000, true, 1);
            UBLcrc = MyCRC.CalculateCRC(UBLFileData);

            try
            {
            BOOTMESEQ:
                Console.WriteLine("\nWaiting for DVEVM...");

                // Wait for the DVEVM to send the ^BOOTME/0 sequence
                if (waitForSequence(" BOOTME\0", " BOOTME\0", MySP))
                    Console.WriteLine("BOOTME commmand received. Returning ACK and header...");
                else
                    goto BOOTMESEQ;

                // Output 28 Bytes for the ACK sequence and header
                // 8 bytes acknowledge sequence = "    ACK\0"
                MySP.Write("    ACK\0");
                
                // 8 bytes of CRC data = ASCII string of 8 hex characters
                MySP.Write(UBLcrc.ToString("X8"));
                
                // 4 bytes of UBL data size = ASCII string of 4 hex characters (3800h = 14336d)
                MySP.Write((UBLstream.Length-256).ToString("X4"));
                
                // 4 bytes of start address = ASCII string of 4 hex characters (>=0100h)
                MySP.Write(cmdParams.UARTUBLExecAddr.ToString("X4"));
                
                // 4 bytes of constant zeros = "0000"
                MySP.Write("0000");
                Console.WriteLine("ACK command sent. Waiting for BEGIN command... ");

                // Wait for the BEGIN sequence
                if (waitForSequence("  BEGIN\0", " BOOTME\0", MySP,true))
                    Console.WriteLine("BEGIN commmand received. Sending CRC table...");
                else
                    goto BOOTMESEQ;

                // Send the 1024 byte (256 word) CRC table
                for (int i = 0; i < MyCRC.Length; i++)
                {
                    MySP.Write(MyCRC[i].ToString("x8"));
                //    Console.WriteLine(MyCRC[i].ToString("x8"));
                }
                Console.WriteLine("CRC table sent.  Waiting for DONE...");
                

                // Wait for the first DONE sequence
                if (waitForSequence("   DONE\0", " BOOTME\0", MySP))
                    Console.WriteLine("DONE received.  Sending the UART UBL file...");
                else
                    goto BOOTMESEQ;

                // Send the contents of the UBL file 
                MySP.Write(UBLsb.ToString());

                // Wait for the second DONE sequence
                if (waitForSequence("   DONE\0", " BOOTME\0", MySP))
                    Console.WriteLine("DONE received.  UART UBL file was accepted.");
                else
                    goto BOOTMESEQ;

                Console.WriteLine("UART UBL Transmitted successfully.\n");

            }
            catch (ObjectDisposedException e)
            {
                Console.WriteLine(e.StackTrace);
                throw e;
            }
        }

        /// <summary>
        /// Function to transmit the CMD and command over the UART (following BOOTPSP)
        /// </summary>
        private static Boolean TransmitCMDSuccessful()
        {
            try
            {
                // Clear input buffer so we can start looking for BOOTPSP
                MySP.DiscardInBuffer();

                Console.WriteLine("\nWaiting for UBL on DVEVM...");        
                
                // Wait for the UBL on the DVEVM to send the ^BOOTPSP\0 sequence
                if (waitForSequence("BOOTPSP\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("UBL's BOOTPSP commmand received. Returning CMD and command...");
                else
                    return false;

                // 8 bytes acknowledge sequence = "    CMD\0"
                MySP.Write("    CMD\0");
                // 8 bytes of magic number
                MySP.Write(((UInt32)cmdParams.CMDMagicFlag).ToString("X8"));
                
                Console.WriteLine("CMD value sent.");
            }
            catch (ObjectDisposedException e)
            {
                Console.WriteLine(e.StackTrace);
                throw e;
            }
            return true;
        }

        /// <summary>
        /// Send command and wait for erase response. (NOR and NAND global erase)
        /// </summary>
        private static void TransmitErase()
        {
            try
            {
            BOOTPSPSEQ1:
                // Send the UBL command
                if (!TransmitCMDSuccessful())
                    goto BOOTPSPSEQ1;

                if (!waitForSequence("   DONE\0", "BOOTPSP\0", MySP, true))
                    goto BOOTPSPSEQ1;

            }
            catch (ObjectDisposedException e)
            {
                Console.WriteLine(e.StackTrace);
                throw e;
            }
        }
        
        /// <summary>
        /// Function to transmit the application code via the UBL, which is now 
        /// running on the DM644x.  This code is specific to the TI supplied UBL.
        /// If the the TI supplied UBL is modified or a different boot loader is
        /// used, this code will need to be modified.
        /// </summary>
        private static void TransmitAPP()
        {
            // Local Variables for reading APP file
            Byte[] APPFileData;

            APPFileData = GetFileData(cmdParams.APPFileName, cmdParams.APPLoadAddr, false);

            try
            {
            BOOTPSPSEQ3:

                // Send the UBL command
                if (!TransmitCMDSuccessful())
                    goto BOOTPSPSEQ3;

                if (waitForSequence("SENDAPP\0", "BOOTPSP\0", MySP))
                {
                    Console.WriteLine("SENDAPP received. Returning ACK and header for application data...");
                }
                else
                {
                    goto BOOTPSPSEQ3;
                }

                // Output 36 Bytes for the ACK sequence and header
                // 8 bytes acknowledge sequence = "    ACK\0"
                MySP.Write("    ACK\0");
                // 8 bytes of magic number (in this case flash the nor
                MySP.Write(((UInt32)cmdParams.APPMagicFlag).ToString("X8"));
                // 8 bytes of binary execution address = ASCII string of 8 hex characters
                MySP.Write(String.Format("{0:X8}", cmdParams.APPEntryPoint));
                // 8 bytes of data size = ASCII string of 8 hex characters
                MySP.Write(((UInt32)APPFileData.Length).ToString("X8"));
                // 4 bytes of constant zeros = "0000"
                MySP.Write("0000");

                Console.WriteLine("ACK command sent. Waiting for BEGIN command... ");

                // Wait for the ^^BEGIN\0 sequence
                if (waitForSequence("  BEGIN\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("UBL's BEGIN commmand received. Sending the application code...");
                else
                    goto BOOTPSPSEQ3;

                // Send the application code (should be in S-record format)
                MySP.Write(APPFileData, 0, APPFileData.Length);
                Console.WriteLine("Application code sent.  Waiting for DONE...");

                // Wait for ^^^DONE\0
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  All bytes of application code received...");
                else
                    goto BOOTPSPSEQ3;

                // Wait for second ^^^DONE\0 to indicate the S-record decode worked
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  Application S-record decoded correctly.");
                else
                    goto BOOTPSPSEQ3;
                
                // Wait for third ^^^DONE that indicates booting
                if (!waitForSequence("   DONE\0", "BOOTPSP\0", MySP, true))
                    throw new Exception("Final DONE not returned.  Command failed on DM644x.");

            }
            catch (ObjectDisposedException e)
            {
                Console.WriteLine(e.StackTrace);
                throw e;
            }

        }

        /// <summary>
        /// Function to transmit the second UBL if it is needed
        /// </summary>
        private static void TransmitFLASHUBLandAPP()
        {         
            Byte[] APPFileData;
            Byte[] FLASHUBLData;

            // Get Application image data
            APPFileData = GetFileData(cmdParams.APPFileName, cmdParams.APPLoadAddr, false);

            // Get Flash UBL data (either embedded or from file)
            if (cmdParams.useEmbeddedUBL)
                FLASHUBLData = bin2srec(GetEmbeddedUBLStream(), cmdParams.FLASHUBLLoadAddr, (cmdParams.UBLFlashType == FlashType.NAND));
            else
                FLASHUBLData = GetFileData(cmdParams.FLASHUBLFileName, cmdParams.FLASHUBLLoadAddr, (cmdParams.UBLFlashType == FlashType.NAND));            
                              
            try
            {
            BOOTPSPSEQ2:
                
                // Send the UBL command
                if (!TransmitCMDSuccessful())
                    goto BOOTPSPSEQ2;

                if (waitForSequence("SENDUBL\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("SENDUBL received. Returning ACK and header for UBL data...");
                else
                    goto BOOTPSPSEQ2;

                // Output 36 Bytes for the ACK sequence and header
                // 8 bytes acknowledge sequence = "    ACK\0"
                MySP.Write("    ACK\0");
                // 8 bytes of magic number (in this case flash the nor
                MySP.Write(((UInt32)cmdParams.FLASHUBLMagicFlag).ToString("X8"));
                // 8 bytes of binary execution address = ASCII string of 8 hex characters
                MySP.Write("8000");
                MySP.Write(cmdParams.FLASHUBLExecAddr.ToString("X4"));
                // 8 bytes of data size = ASCII string of 8 hex characters
                MySP.Write(((UInt32)FLASHUBLData.Length).ToString("X8"));
                // 4 bytes of constant zeros = "0000"
                MySP.Write("0000");

                Console.WriteLine("ACK command sent. Waiting for BEGIN command... ");
                // Wait for the ^^BEGIN\0 sequence
                if (waitForSequence("  BEGIN\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("UART UBL's BEGIN commmand received. Sending the Flash UBL code...");
                else
                    goto BOOTPSPSEQ2;

                // Send the Flash UBL code (should be in S-record format)
                MySP.Write(FLASHUBLData, 0, FLASHUBLData.Length);
                Console.WriteLine("Flash UBL code sent.  Waiting for DONE...");

                // Wait for ^^^DONE\0
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  All bytes of Flash UBL code received...");
                else
                    goto BOOTPSPSEQ2;

                // Wait for second ^^^DONE\0 to indicate the S-record decode worked
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  Flash UBL S-record decoded correctly.");
                else
                    goto BOOTPSPSEQ2;

                // Now Send the Application file that will be written to flash
                if (waitForSequence("SENDAPP\0", "BOOTPSP\0", MySP,true))
                    Console.WriteLine("SENDAPP received. Returning ACK and header for application data...");
                else
                    goto BOOTPSPSEQ2;
                // Output 36 Bytes for the ACK sequence and header
                // 8 bytes acknowledge sequence = "    ACK\0"
                MySP.Write("    ACK\0");
                // 8 bytes of magic number (in this case flash the nor
                if ( (cmdParams.CMDMagicFlag == MagicFlags.UBL_MAGIC_NAND_BIN_BURN) ||
                     (cmdParams.CMDMagicFlag == MagicFlags.UBL_MAGIC_NOR_BIN_BURN) )
                    MySP.Write(((UInt32)MagicFlags.UBL_MAGIC_BIN_IMG).ToString("X8"));
                else 
                    MySP.Write(((UInt32)MagicFlags.UBL_MAGIC_SAFE).ToString("X8"));

                // 8 bytes of binary execution address = ASCII string of 8 hex characters
                MySP.Write(String.Format("{0:X8}", cmdParams.APPEntryPoint));
                // 8 bytes of data size = ASCII string of 8 hex characters
                MySP.Write(((UInt32)APPFileData.Length).ToString("X8"));
                // 4 bytes of constant zeros = "0000"
                MySP.Write("0000");

                Console.WriteLine("ACK command sent. Waiting for BEGIN command... ");
                // Wait for the ^^BEGIN\0 sequence
                if (waitForSequence("  BEGIN\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("UART UBL's BEGIN commmand received. Sending the Application code...");
                else
                    goto BOOTPSPSEQ2;

                // Send the application code (should be in S-record format)
                MySP.Write(APPFileData, 0, APPFileData.Length);
                Console.WriteLine("Application code sent.  Waiting for DONE...");

                // Wait for ^^^DONE\0
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  All bytes of Application code received...");
                else
                    goto BOOTPSPSEQ2;

                // Wait for second ^^^DONE\0 to indicate the S-record decode worked
                if (waitForSequence("   DONE\0", "BOOTPSP\0", MySP))
                    Console.WriteLine("DONE received.  Application S-record decoded correctly.");
                else
                    goto BOOTPSPSEQ2;

                // Wait for third ^^^DONE that indicates booting
                waitForSequence("   DONE\0", "BOOTPSP\0", MySP,true);
                                
            }
            catch (ObjectDisposedException e)
            {
                Console.WriteLine(e.StackTrace);
                throw e;
            }

        }

        /// <summary>
        /// Waitforsequence with option for verbosity
        /// </summary>
        /// <param name="str">String to look for</param>
        /// <param name="altStr">String to look for but don't want</param>
        /// <param name="sp">SerialPort object.</param>
        /// <param name="verbose">Boolean to indicate verbosity.</param>
        /// <returns>Boolean to indicate if str or altStr was found.</returns>
        private static Boolean waitForSequence(String str,String altStr,SerialPort sp,Boolean verbose)
        {
            Boolean strFound = false, altStrFound = false;
            Byte[] input = new Byte[256];
            String inputStr;
            Int32 i;

            while ((!strFound) && (!altStrFound))
            {

                i = 0;
                do
                {
                    input[i++] = (Byte)sp.ReadByte();
                    //Console.Write(input[i - 1] + " ");
                } while ( (input[i - 1] != 0) &&
                          (i < (input.Length - 1)) &&
                          (input[i - 1] != 0x0A) &&
                          (input[i - 1] != 0x0D) );

                // Convert to string for comparison
                if ((input[i-1] == 0x0A) || (input[i-1] == 0x0D))
                    inputStr = (new ASCIIEncoding()).GetString(input, 0, i-1);
                else
                    inputStr = (new ASCIIEncoding()).GetString(input, 0, i);

                if (inputStr.Length == 0)
                {
                    continue;
                }

                // Compare Strings to see what came back
                if (verbose)
                    Console.WriteLine("\tDVEVM:\t{0}", inputStr);
                if (inputStr.Contains(altStr))
                {
                    altStrFound = true;
                    if (String.Equals(str, altStr))
                    {
                        strFound = altStrFound;
                    }
                }
                else if (inputStr.Contains(str))
                {
                    strFound = true;
                }
                else
                {
                    strFound = false;
                }
            }
            return strFound;
        }


        /// <summary>
        /// Wait for null-terminated sequences from supplied serial port
        /// </summary>
        /// <param name="str">Expected character string.</param>
        /// <param name="altStr">Alternative string that if received, indicates failure
        /// This is "BOOTME\0" or "BOOTPSP\0"</param>
        /// <param name="sp">Serial port object to use to receive string.</param>
        /// <returns>Boolean ind4cating whether sequence was received.</returns>
        private static Boolean waitForSequence( String str,String altStr,SerialPort sp)
        {
            return waitForSequence(str, altStr, sp, cmdParams.Verbose);
        }
        
        #endregion 
        //**********************************************************************************


        //**********************************************************************************
        #region Code to manipulate binary file to Motorola S-record

        /// <summary>
        /// Function to convert the input filestream into an byte array in S-record format,
        /// so that it can be downloaded to the EVM board.
        /// </summary>
        /// <param name="inputFileStream">The input filestream that encapsulates the
        /// input application file.</param>
        /// <param name="startAddr">The starting address of the RAM location where the binary data
        /// encapsulated by the S-record will be stored.</param>
        /// <returns>A byte array of the file data.</returns>
        public static Byte[] bin2srec(Stream inputStream, UInt32 startAddr, Boolean stripSelfCopyData)
        {
            Int64 totalLen;
            BinaryReader fileBR = new BinaryReader(inputStream);
            StringBuilder fileSB;
            String fileName;
            String shortFileName;
            Byte[] currChar = new Byte[1];
            Byte[] currDataRecord;
            Int32 i, checksum8 = 0;
            Int32 recordSize = 16;
            UInt32 memAddr = startAddr;
            
            // Set the actual length
            if (stripSelfCopyData)
                totalLen = fileBR.BaseStream.Length - 256;
            else
                totalLen = fileBR.BaseStream.Length;
            fileSB = new StringBuilder(4 * (int)totalLen);
            
            // Set S-record filename (real name or fake)
            if (inputStream is FileStream)
                fileName = ((FileStream)inputStream).Name;
            else
                fileName = "ublDaVinci.bin";            
                        
            // Make sure we are at the right place in the stream
            if (stripSelfCopyData)
                fileBR.BaseStream.Seek(0x100, SeekOrigin.Begin);
            else
                fileBR.BaseStream.Seek(0x0, SeekOrigin.Begin);
            
            // Get filename (this is S-record module name)
            if (Path.HasExtension(fileName))
                shortFileName = Path.GetFileNameWithoutExtension(fileName) + ".srec";
            else
                shortFileName = Path.GetFileName(fileName) + ".srec";
                                    
            // Make sure S-record module name fits in 20 byte field
            if (shortFileName.Length > 20)
                shortFileName = shortFileName.Substring(0, 20);

            // Create first s-record (S0 record)
            fileSB.Append("S0");
            // Write length field
            fileSB.AppendFormat("{0:X2}",shortFileName.Length+3);
            checksum8 += (Byte)(shortFileName.Length + 3);
            // Write address field
            fileSB.Append("0000");
            // Write name field
            for (i = 0; i < shortFileName.Length; i++)
            {
                currChar = (new ASCIIEncoding()).GetBytes(shortFileName.Substring(i, 1));
                checksum8 += currChar[0];                
                fileSB.AppendFormat("{0:X2}",currChar[0]);
            }
            // Write Checksum field
            fileSB.AppendFormat("{0:X2}\x0A", ((checksum8&0xFF)^0xFF));

            // Write collection of S3 records (actual binary data)
            i = (Int32)totalLen;
            
            while (i >= recordSize)
            {
                checksum8 = 0;
                // Write S3 record label
                fileSB.Append("S3");
                // Write length field (4 address bytes + 16 data bytes + 1 checksum byte)
                fileSB.AppendFormat("{0:X2}", recordSize + 5);
                checksum8 += (recordSize + 5);
                
                // Write address field and update it
                fileSB.AppendFormat("{0:X8}", memAddr);
                currDataRecord = System.BitConverter.GetBytes(memAddr);
                for (int j = 0; j < 4; j++)
                {
                    checksum8 += currDataRecord[j];
                }
                
                // Write out the bytes of data
                currDataRecord = fileBR.ReadBytes(recordSize);
                for (int j = 0; j < recordSize; j++)
                {                    
                    fileSB.AppendFormat("{0:X2}", currDataRecord[j]);
                    checksum8 += currDataRecord[j];
                }
                //Write out checksum and linefeed character
                fileSB.AppendFormat("{0:X2}\x0A", ((checksum8&0xFF) ^ 0xFF));

                memAddr += (UInt32)recordSize; i -= recordSize;
            }

            // Finish out the record if anything is left over
            if (i > 0)
            {
                checksum8 = 0;
                // Write S3 record label
                fileSB.Append("S3");
                // Write length field (4 address bytes + 16 data bytes + 1 checksum byte)
                fileSB.AppendFormat("{0:X2}", i + 5);
                checksum8 += (i + 5);

                // Write address field and update it
                fileSB.AppendFormat("{0:X8}", memAddr);
                currDataRecord = System.BitConverter.GetBytes(memAddr);
                for (int j = 0; j < 4; j++)
                {
                    checksum8 += currDataRecord[j];
                }

                // Write out the bytes of data
                currDataRecord = fileBR.ReadBytes(i);
                for (int j = 0; j < i; j++)
                {
                    fileSB.AppendFormat("{0:X2}", currDataRecord[j]);
                    checksum8 += currDataRecord[j];
                }
                //Write out checksum and linefeed character
                fileSB.AppendFormat("{0:X2}\x0A", ((checksum8 & 0xFF) ^ 0xFF));

                memAddr += (UInt32)i; i = 0;

            }
            
            // Write out the final record (S7 record)
            checksum8 = 0;
            // Write S7 record label
            fileSB.Append("S7");
            // Write length field (4 address bytes + 1 checksum byte)
            fileSB.AppendFormat("{0:X2}", 5);
            checksum8 += 5;

            // Write execution start address field and update it
            fileSB.AppendFormat("{0:X8}", startAddr);
            currDataRecord = System.BitConverter.GetBytes(startAddr);
            for (int j = 0; j < 4; j++)
            {
                checksum8 += currDataRecord[j];
            }
            //Write out checksum and linefeed character
            fileSB.AppendFormat("{0:X2}\x0A", ((checksum8 & 0xFF) ^ 0xFF));

            return (new ASCIIEncoding()).GetBytes(fileSB.ToString());
        
        }

        /// <summary>
        /// Get the byte array if the file is already an S-record
        /// </summary>
        /// <param name="inputFileStream">The input filestream that encapsulates the
        /// input application file.</param>
        /// <returns>A byte array of the file data.</returns>
        public static Byte[] readSrec(FileStream inputFileStream)
        {
            inputFileStream.Position = 0;
            return (new BinaryReader(inputFileStream)).ReadBytes((Int32)inputFileStream.Length);
        }
        
        /// <summary>
        /// Check to see if filestream is an S-record by reading the first
        /// character of each line and seeing if it is 'S'
        /// </summary>
        /// <param name="inputFileStream">The input filestream that encapsulates the
        /// input application file.</param>
        /// <returns>a boolean representing whether the file is an s-record.</returns>
        public static Boolean isFileSrec(FileStream inputFileStream)
        {
            StreamReader sr = new StreamReader(inputFileStream, Encoding.ASCII);
            String text;

            inputFileStream.Position = 0;
            text = sr.ReadLine();
            while (!sr.EndOfStream)
            {
                if (!text.StartsWith("S"))
                {
                    return false;
                }
                text = sr.ReadLine();
            }
            
            return true;
        }
            
        #endregion
        //**********************************************************************************

    }

    
}
