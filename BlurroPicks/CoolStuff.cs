using System;
using System.IO;
using System.Reflection;
using System.Diagnostics;
using System.Collections;
using System.Net;
using System.Security.Policy;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Security.Principal;

class Program
{
    static void Main(string[] args)
    {
        bool delete = true;
        bool wait = true;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "-nodelete")
            {
                delete = false;
            }
            if (args[i] == "-nowait")
            {
                wait = false;
            }
        }
        string preMsgCode = "\u001b[34m";
        string postMsgCode = "\u001b[37m";
        Assembly assembly = Assembly.GetExecutingAssembly();
        if (wait)
        {
            preMsgCode = "\u001b[26A\u001b[34m";
            using (Stream stream = assembly.GetManifestResourceStream("SwitchPhysicsConverter.logo.txt"))
            using (StreamReader reader = new StreamReader(stream))
            {
                string line;
                int lineNumber = 1;
                Console.WriteLine("\n\n\n");
                while ((line = reader.ReadLine()) != null)
                {
                    Console.WriteLine("\u001b[30m..................................................\u001b[34m" + line);
                    lineNumber++;
                }
            }
        }
        if (wait) { postMsgCode = "\u001b[30m.................................................................\u001b[90madd -nodelete arg to keep /temp/\u001b[37m"; }
        Console.WriteLine(preMsgCode + "Very epic mod converter" + postMsgCode);
        if (wait) { postMsgCode = "\u001b[30m................................................................\u001b[90m-nowait to exit on finish\u001b[37m"; }
        Console.WriteLine("Time to \u001b[94msw\u001b[90mit\u001b[91mch\u001b[37m up them physics!" + postMsgCode);

        // getting paths
        string exeDirectory = Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
        string pacPathInput = null;

        // check the right stuff is there
        if (!File.Exists(Path.Combine(exeDirectory, "HedgeArcPack.exe")))
        {
            Console.WriteLine("\u001b[31mCan't find HedgeArcPack.exe in this folder!\u001b[37m");
            if (wait)
            {
                Console.ReadKey(true);
            }
            Environment.Exit(0);
        }

        List<string> pacPathInputList = new List<string>();
        string folderInput = null;
        bool hasSciptGedit = false;

        for (int i = 0; i < args.Length; i++)
        {
            if (Directory.Exists(args[i]))
            {
                string[] pacFiles = Directory.GetFiles(args[i], "*.pac", SearchOption.AllDirectories);

                foreach (string pacPath in pacFiles)
                {
                    if (!pacPath.Contains("_trr_cmn.pac"))
                    {
                        pacPathInputList.Add(pacPath);
                    }
                    if (pacPath.Contains("\\script\\") || pacPath.Contains("\\gedit\\"))
                    {
                        hasSciptGedit = true;
                    }
                }
                pacPathInput = "p";
                folderInput = Directory.GetParent(args[i]).FullName;
                break;
            }
            if (File.Exists(args[i]) && (Path.GetExtension(args[i]) == ".pac"))
            {
                Console.WriteLine(Path.GetFileName(args[i]));
                pacPathInputList.Add(args[i]);
                pacPathInput = "p";
            }
        }

        //pacPathInput = "p";
        //pacPathInputList.Add("playercommonfaithful.pac");
        //pacPathInputList.Add("playercommon_PC_u2_Copy.pac");
        //pacPathInputList.Add("playercommonbeatz.pac");
        //wait = false;

        Console.WriteLine("\u001b[30mHEY! IF YOU CAN READ THIS, PLEASE TYPE 'Terminal settings' AND SET YOUR TERMINAL TO 'Windows Terminal'!!\u001b[37m\u001b[A");
        Console.WriteLine("\u001b[30mOR RUN THIS TOOL AS ADMIN!\u001b[37m\u001b[A");
        while (pacPathInput == null)
        {
            if (new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator))
            {
                Console.WriteLine("Admin detected! Put your .pac in this folder with tool, name it smth cool, type it below (like 'mycoolfile.pac')");
                if (!args.Contains("-terminal"))
                {
                    ProcessStartInfo startInfo = new ProcessStartInfo
                    {
                        FileName = "wt.exe",
                        Arguments = Assembly.GetEntryAssembly()?.Location + " -terminal"
                    };
                    Process.Start(startInfo);
                    Environment.Exit(0);
                }
                string readLine = Console.ReadLine();
                if (File.Exists(readLine) && (Path.GetExtension(readLine) == ".pac")) { pacPathInputList.Add(readLine); break; }
                else if (Directory.Exists(readLine))
                {
                    string[] pacFiles = Directory.GetFiles(readLine, "*.pac", SearchOption.AllDirectories);

                    foreach (string pacPath in pacFiles)
                    {
                        if (!pacPath.Contains("_trr_cmn.pac"))
                        {
                            pacPathInputList.Add(pacPath);
                        }
                        if (pacPath.Contains("\\script\\") || pacPath.Contains("\\gedit\\"))
                        {
                            hasSciptGedit = true;
                        }
                    }
                    pacPathInput = "p";
                    folderInput = Directory.GetParent(readLine).FullName;
                    break;
                }
            }
            else
            {
                Console.WriteLine("Drag and drop a playercommon.pac mod on this tool to start :D");
                Console.WriteLine("");
                Console.WriteLine("You can also drop a WHOLE MOD folder to convert :O");
                Console.WriteLine("(transfers the .rfl files to the switch .pacs)");
                if (wait)
                {
                    Console.ReadKey(true);
                }
                Environment.Exit(0);
            }
            Environment.Exit(0);
        }

        Console.WriteLine("");
        Directory.CreateDirectory(Path.Combine(exeDirectory, "unmodified_switchfiles"));
        int finishedFiles = 0;

        for (int p = 0; p < pacPathInputList.Count; p++)
        {
            bool isSwitchFixed = false;

            pacPathInput = pacPathInputList[p];
            string pacSwitchName = Path.ChangeExtension(Path.GetFileName(pacPathInput), null) + "_SWITCH.pac";

            Console.WriteLine("\u001b[34mLooking for " + Path.GetFileName(pacPathInput) + "...\u001b[37m");
            if (File.Exists(Path.Combine(exeDirectory, "unmodified_switchfiles\\" + pacSwitchName.Replace("_SWITCH", "_SWITCH_fix"))))
            {
                isSwitchFixed = true;
                pacSwitchName = pacSwitchName.Replace("_SWITCH", "_SWITCH_fix");
            }//for lut mod _fix checking

            if (!File.Exists(Path.Combine(exeDirectory, "unmodified_switchfiles\\" + pacSwitchName)))
            {
                bool doesUrlExist = DoesUrlExist("https://github.com/Blurro/hmmm/raw/main/SwitchFiles/" + pacSwitchName);
                isSwitchFixed = DoesUrlExist("https://github.com/Blurro/hmmm/raw/main/SwitchFiles/" + pacSwitchName.Replace("_SWITCH","_SWITCH_fix")); //for lut mods, just gets enabler that'll work instead of converting
                if (!doesUrlExist)
                {
                    if (isSwitchFixed == true)
                    {
                        pacSwitchName = pacSwitchName.Replace("_SWITCH", "_SWITCH_fix");
                        Console.WriteLine("Downloading LUT enabled switch pac...");
                        try
                        {
                            DownloadFile(pacSwitchName);
                        }
                        catch
                        {
                            Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                            Console.ReadKey(true);
                            Environment.Exit(0);
                        }
                    } else if (pacPathInputList.Count == 1)
                    {
                        Console.WriteLine("No match, assuming playercommon.pac");
                        // if we cant find the .pac file in unmodified_switchfiles or online, just assume its a randomly named playercommon
                        pacSwitchName = "playercommon_SWITCH.pac";
                    }
                    else
                    {
                        Console.WriteLine("No match, skipping to next file");
                        continue;
                    }
                }
                if (pacSwitchName != "playercommon_SWITCH.pac" && !isSwitchFixed)
                {
                    if (doesUrlExist || !File.Exists(Path.Combine(exeDirectory, "unmodified_switchfiles\\" + pacSwitchName)))
                    {
                        doesUrlExist = DoesUrlExist("https://github.com/Blurro/hmmm/raw/main/SwitchFiles/" + Path.ChangeExtension(pacSwitchName, null) + "_2.pac");

                        for (int i = 0; i < (doesUrlExist ? 1 : 0) + 1; i++)
                        {
                            string fileName = pacSwitchName;
                            if (i == 0)
                            {
                                Console.WriteLine("Downloading, please wait...");
                                try
                                {
                                    DownloadFile(fileName);
                                }
                                catch
                                {
                                    Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                    Console.ReadKey(true);
                                    Environment.Exit(0);
                                }
                            }
                            else
                            {
                                fileName = Path.ChangeExtension(pacSwitchName, null) + "_2.pac";
                                try
                                {
                                    DownloadFile(fileName);
                                }
                                catch
                                {
                                    Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                    Console.ReadKey(true);
                                    Environment.Exit(0);
                                }
                                // if we're on the 2nd iteration that means the split _2 was found, this joins it onto the other file. just bypasses github 25mb lol
                                JoinFiles(Path.Combine(exeDirectory, "unmodified_switchfiles", pacSwitchName), Path.Combine(exeDirectory, "unmodified_switchfiles", fileName));
                            }
                        }
                        Console.WriteLine("Downloaded to unmodified_switchfiles");
                    }
                    else
                    {
                        Console.WriteLine(pacSwitchName + " exists locally");
                    }
                }
            }
            else
            {
                Console.WriteLine(pacSwitchName + " exists locally");
                if (File.Exists(Path.Combine(exeDirectory, "unmodified_switchfiles", Path.ChangeExtension(pacSwitchName, null) + "_2.pac")))
                {
                    Console.WriteLine("Joining splits...");
                    JoinFiles(Path.Combine(exeDirectory, "unmodified_switchfiles", pacSwitchName), Path.Combine(exeDirectory, "unmodified_switchfiles", Path.ChangeExtension(pacSwitchName, null) + "_2.pac"));
                }
            }

            string pacPath1 = Path.Combine(exeDirectory, "unmodified_switchfiles", pacSwitchName);

            if (Directory.Exists(Path.Combine(exeDirectory, "temp")))
            {
                Directory.Delete(Path.Combine(exeDirectory, "temp"), true);
            }
            Directory.CreateDirectory(Path.Combine(exeDirectory, "temp"));

            string[] rflFiles = new string[0];
            string[] fileLines = new string[0];
            if (isSwitchFixed == true)
            {
                finishedFiles++;
            } else
            {
                File.WriteAllText(Path.Combine(exeDirectory, "temp\\00pos.txt"), string.Empty);
                fileLines = File.ReadAllLines(Path.Combine(exeDirectory, "temp\\00pos.txt"));

                // extracts pacs using other tool
                Console.WriteLine("Extracting files");
                RunHedgeArcPack(pacPathInput);

                if (pacSwitchName != "playercommon_SWITCH.pac")
                {
                    RunHedgeArcPack(pacPath1);
                }

                rflFiles = Directory.GetFiles(Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(Path.GetFileName(pacPathInput), null)), "*.rfl");
                if (rflFiles.Length == 0)
                {
                    Console.WriteLine("\u001b[31mInput pac file contains no .rfl\u001b[37m");
                    continue;
                }
            }
            
            for (int j = 0; j < rflFiles.Length; j++)
            {
                //Console.WriteLine(pacSwitchName + " -loopin");
                string filePath1 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(pacSwitchName, null), Path.GetFileName(rflFiles[j]));
                string filePath2 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(Path.GetFileName(pacPathInput), null), Path.GetFileName(rflFiles[j]));

                string rflFileName = "player_common.rfl";
                if (pacSwitchName != "playercommon_SWITCH.pac") // if not playercommon, get the rfl file. playercommon u3/u4 pac will be named differently and also enter this section, so we also check for _common rfls (other characters) and skip if not found, only skipping if this IS a playercommon pac anyway
                {
                    rflFileName = Path.GetFileName(rflFiles[j]);
                    if (pacSwitchName.Contains("playercommon") && (rflFileName == "player_common.rfl" || !rflFileName.Contains("_common")))
                    {
                        continue;
                    }
                }

                //Console.WriteLine(rflFileName);

                if (!rflFiles.Any(file => Path.GetFileName(file).Equals(rflFileName)))
                { // just checks for player_common.rfl
                    Console.WriteLine("\u001b[31mInput pac file doesn't contain " + rflFileName + "\u001b[37m");
                    if (wait)
                    {
                        Console.ReadKey(true);
                    }
                    if (delete)
                    {
                        Directory.Delete(Path.Combine(exeDirectory, "temp"), true);
                    }
                    Environment.Exit(0);
                }

                string filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles\\player_common_PC.rfl");
                filePath2 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(Path.GetFileName(pacPathInput), null), "player_common.rfl");
                // stuff for non playercommons
                if (pacSwitchName != "playercommon_SWITCH.pac")
                {
                    string rflRename = Path.ChangeExtension(rflFileName, null) + "_PC.rfl";
                    if (pacSwitchName == "playercommon_SWITCH_u4.pac") // u3 will get 'amy_common_PC.rfl' but u4 will get '..PC_u4.rfl'
                    {
                        rflRename = Path.ChangeExtension(rflFileName, null) + "_PC_u4.rfl";
                    }
                    filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles", rflRename);
                    filePath2 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(Path.GetFileName(pacPathInput), null), rflFileName);
                    filePath1 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(pacSwitchName, null), rflFileName);
                }

                if (rflFileName == "player_common.rfl")
                {
                    long fileSizeInBytes = new FileInfo(filePath2).Length;
                    if (fileSizeInBytes == 39700)
                    {
                        Console.WriteLine("\u001b[35mV1 detected (WONT WORK ON FRONTIERS V1.2+)\u001b[95m");
                        Console.WriteLine("Check out 'Playercommon Updater' to update!\u001b[37m");
                        filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles\\player_common_PC_OLD.rfl");
                        pacPath1 = Path.Combine(exeDirectory, "unmodified_switchfiles\\playercommon_SWITCH_OLD.pac");
                        pacSwitchName = "playercommon_SWITCH_OLD.pac";
                        if (!File.Exists(pacPath1))
                        {
                            string fileName = Path.GetFileName(pacPath1);
                            Console.WriteLine("Downloading playercommon_SWITCH V1");
                            try
                            {
                                DownloadFile(fileName);
                            }
                            catch
                            {
                                Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                Console.ReadKey(true);
                                Environment.Exit(0);
                            }
                        }
                        RunHedgeArcPack(pacPath1);
                    }
                    else if (fileSizeInBytes == 40004)
                    {
                        Console.WriteLine("\u001b[35mV1.2 detected (WONT WORK ON FRONTIERS V1/V1.3)\u001b[95m");
                        Console.WriteLine("Check out 'Playercommon Updater' to change!\u001b[37m");
                        if (!File.Exists(pacPath1))
                        {
                            string fileName = Path.GetFileName(pacPath1);
                            Console.WriteLine("Downloading playercommon_SWITCH V1.2");
                            try
                            {
                                DownloadFile(fileName);
                            }
                            catch
                            {
                                Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                Console.ReadKey(true);
                                Environment.Exit(0);
                            }
                        }
                        RunHedgeArcPack(pacPath1);
                    }
                    else if (fileSizeInBytes == 40808)
                    {
                        Console.WriteLine("\u001b[35mV1.3 detected (WONT WORK ON FRONTIERS <V1.3)\u001b[95m");
                        Console.WriteLine("Check out 'Playercommon Updater' to change!\u001b[37m");
                        filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles\\player_common_PC_u2.rfl");
                        pacPath1 = Path.Combine(exeDirectory, "unmodified_switchfiles\\playercommon_SWITCH_u2.pac");
                        pacSwitchName = "playercommon_SWITCH_u2.pac";
                        if (!File.Exists(pacPath1))
                        {
                            string fileName = Path.GetFileName(pacPath1);
                            Console.WriteLine("Downloading playercommon_SWITCH V1.3");
                            try
                            {
                                DownloadFile(fileName);
                            }
                            catch
                            {
                                Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                Console.ReadKey(true);
                                Environment.Exit(0);
                            }
                        }
                        RunHedgeArcPack(pacPath1);
                    }
                    else if (fileSizeInBytes == 46212)
                    {
                        Console.WriteLine("\u001b[35mV1.4 detected (LATEST VER IS 1.41)\u001b[95m");
                        Console.WriteLine("Check out 'Playercommon Updater' to change!\u001b[37m");
                        filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles\\player_common_PC_u3.rfl");
                        pacPath1 = Path.Combine(exeDirectory, "unmodified_switchfiles\\playercommon_SWITCH_u3.pac");
                        pacSwitchName = "playercommon_SWITCH_u3.pac";
                        if (!File.Exists(pacPath1))
                        {
                            string fileName = Path.GetFileName(pacPath1);
                            Console.WriteLine("Downloading playercommon_SWITCH V1.4");
                            try
                            {
                                DownloadFile(fileName);
                            }
                            catch
                            {
                                Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                Console.ReadKey(true);
                                Environment.Exit(0);
                            }
                        }
                        RunHedgeArcPack(pacPath1);
                    }
                    else if (fileSizeInBytes == 46564)
                    {
                        Console.WriteLine("\u001b[35mV1.41 detected\u001b[95m");
                        Console.WriteLine("This is the latest as of Dec 2023\u001b[37m");
                        filePathPC = Path.Combine(exeDirectory, "unmodified_switchfiles\\player_common_PC_u4.rfl");
                        pacPath1 = Path.Combine(exeDirectory, "unmodified_switchfiles\\playercommon_SWITCH_u4.pac");
                        pacSwitchName = "playercommon_SWITCH_u4.pac";
                        if (!File.Exists(pacPath1))
                        {
                            string fileName = Path.GetFileName(pacPath1);
                            Console.WriteLine("Downloading playercommon_SWITCH V1.41");
                            try
                            {
                                DownloadFile(fileName);
                            }
                            catch
                            {
                                Console.WriteLine("\u001b[31mDownload failed? Ping @blurro\u001b[37m");
                                Console.ReadKey(true);
                                Environment.Exit(0);
                            }
                        }
                        RunHedgeArcPack(pacPath1);
                    }
                    else
                    {
                        Console.WriteLine("Unknown playercommon, or Switch version file!");
                        if (wait)
                        {
                            Console.ReadKey(true);
                        }
                        if (delete)
                        {
                            Directory.Delete(Path.Combine(exeDirectory, "temp"), true);
                        }
                        Environment.Exit(0);
                    }
                } else
                {
                    Console.WriteLine("Scanning " + rflFileName);
                }
                if (rflFileName == "player_common.rfl") { filePath1 = Path.Combine(exeDirectory, "temp\\" + Path.ChangeExtension(pacSwitchName, null), "player_common.rfl"); }
                
                if (!File.Exists(filePathPC))
                {
                    try
                    {
                        DownloadFile(Path.GetFileName(filePathPC));
                        Console.WriteLine("Downloaded " + Path.GetFileName(filePathPC));
                    }
                    catch
                    { // if the url doesnt exist, we dont need to convert, just move instead
                        Console.WriteLine("Moving rfl unmodified");
                        File.Delete(filePath1);
                        File.Move(filePath2, filePath1);
                        continue;
                    }
                }

                // this function deletes all the random 00 groups that stop these files being the same length
                try
                {
                    if (File.Exists(filePath1 + "2")) { File.Delete(filePath1 + "2"); }
                    if (File.Exists(filePathPC + "2")) { File.Delete(filePathPC + "2"); }
                    File.Copy(filePath1, filePath1 + "2");
                    File.Copy(filePathPC, filePathPC + "2");
                    CompareAndModifyFiles(filePath1 + "2", filePathPC + "2", filePathPC);

                    //Console.ReadKey();

                    File.Delete(filePath1 + "2");
                    File.WriteAllText(Path.Combine(exeDirectory, "temp\\00pos.txt"), string.Empty);
                    File.Move(filePathPC + "2", Path.Combine(exeDirectory, "temp", Path.GetFileName(filePathPC)));

                    fileLines = File.ReadAllLines(Path.Combine(exeDirectory, "temp\\00pos.txt"));
                    if (fileLines.Length != 0)
                    {
                        Console.WriteLine("Moving random 00 byte groups");
                    }
                    CompareAndModifyFiles(filePath1, filePath2, filePathPC);

                    long fileSiz = new FileInfo(filePath1).Length;
                    long fileSizzle = new FileInfo(filePath2).Length;
                    if (fileSiz != fileSizzle)
                    {
                        Console.WriteLine();
                        Console.WriteLine("File sizes didn't match, tell @blurro about this error! (conversion may still work, but not perfectly)");
                        Console.WriteLine();
                    }
                    //Console.ReadKey();
                }
                catch (Exception ex)
                {
                    Console.WriteLine("\u001b[31mUh oh, error!\u001b[37m");
                    Console.Write(ex.ToString());
                    if (wait)
                    {
                        Console.ReadKey(true);
                    }
                    if (delete)
                    {
                        Directory.Delete(Path.Combine(exeDirectory, "temp"), true);
                    }
                    Environment.Exit(0);
                }

                // finds footer
                long chunkFromEnd = 0;
                byte[] searchBytes = new byte[] { 0x00, 0x00, 0x00, 0x00 };

                using (FileStream fileStream = File.OpenRead(filePath1))
                {
                    long fileSize = fileStream.Length;
                    int currentPosition = (Convert.ToInt32(fileSize) + 15) / 16 * 16 - 20;
                    while (currentPosition >= 0)
                    {
                        fileStream.Position = currentPosition;
                        byte[] buff = new byte[searchBytes.Length];
                        fileStream.Read(buff, 0, searchBytes.Length);
                        bool found = true;
                        for (int i = 0; i < searchBytes.Length; i++)
                        {
                            if (buff[i] != searchBytes[i])
                            {
                                found = false;
                                break;
                            }
                        }
                        if (found)
                        {
                            chunkFromEnd = currentPosition;
                            break;
                        }
                        currentPosition -= 16;
                    }
                }
                //Console.WriteLine(chunkFromEnd);

                List<string> moreData = new List<string>();
                List<int> positions = new List<int>();
                ScanFilesMore(Path.Combine(exeDirectory, "temp", Path.GetFileName(filePathPC)), filePath1, filePath2, ref moreData, ref positions, chunkFromEnd);
                byte[] buffer = File.ReadAllBytes(filePath1);
                // After scanning og switch file, copy all the data from the pc mod file into the switch file
                Console.WriteLine("Moving .rfl data");
                MoveRFLdata(filePath1, filePath2, 48, chunkFromEnd);
                byte[] buffer2 = File.ReadAllBytes(filePath1);
                List<string> skipList = new List<string>();
                for (int i = 0; i < positions.Count; i++)
                {
                    if (moreData[i] == "pick")
                    {
                        Array.Copy(buffer, positions[i], buffer2, positions[i], 4);
                    }
                    else
                    {
                        if (!(moreData.IndexOf(BitConverter.ToString(buffer2, positions[i], 4)) == -1))
                        {
                            Array.Copy(buffer, positions[moreData.IndexOf(BitConverter.ToString(buffer2, positions[i], 4))], buffer2, positions[i], 4);
                        }
                        else
                        {
                            if (skipList.Count == 0)
                            {
                                Console.WriteLine("Unrecognised bytes at:");
                                skipList.Add(positions[i].ToString("X"));
                            }
                            else
                            {
                                skipList.Add(positions[i].ToString("X"));
                            }
                        }
                    }
                }
                string hexPos = "";
                for (int i = 0; i < skipList.Count; i++)
                {
                    for (int o = 0; o < 9; o++)
                    {
                        hexPos = hexPos + skipList[i] + " ";
                        if (i + 1 == skipList.Count)
                        {
                            i++;
                            break;
                        }
                        i++;
                    }
                    i--;
                    Console.WriteLine(hexPos);
                    hexPos = "";
                }
                if (skipList.Count > 0)
                {
                    Console.WriteLine("(Mod creator likely disabled some cameras)");
                }

                File.WriteAllBytes(filePath1, buffer2);

                // Puts back all the deleted 00 groups
                if (fileLines.Length != 0)
                {
                    Console.WriteLine("Re-inserting all random 00 bytes");
                }

                string[] lines = File.ReadAllLines(Path.Combine(exeDirectory, "temp\\00pos.txt"));
                foreach (string line in lines)
                {
                    ModifyFile(filePath1, int.Parse(line), false, 4);
                }
                if (rflFileName == "player_common.rfl")
                {
                    if (pacSwitchName == "playercommon_SWITCH_u3.pac" || pacSwitchName == "playercommon_SWITCH_u4.pac")
                    {
                        Console.WriteLine();
                        j = -1;
                    } else
                    {
                        break;
                    }
                }

                continue;
            }

            if (!isSwitchFixed)
            {
                // run conversion tool for .pac and suppress console text appearing
                ProcessStartInfo startInfo4 = new ProcessStartInfo(Path.Combine(exeDirectory, "HedgeArcPack.exe"), "\"" + Path.Combine(exeDirectory, "temp", Path.ChangeExtension(pacSwitchName, null)) + "\"")
                {
                    WindowStyle = ProcessWindowStyle.Hidden,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false
                };
                Console.WriteLine("Creating new .pac for the Switch");
                Process process = Process.Start(startInfo4);
                process.StandardInput.WriteLine("frontiers");
                process.StandardInput.Flush();
                process.StandardInput.Close();
                process.StandardOutput.ReadToEnd();
                process.StandardError.ReadToEnd();
                process.WaitForExit();
            } else
            {
                Console.WriteLine("Copying to output");
                File.Copy(pacPath1, Path.Combine(exeDirectory, "temp", pacSwitchName));
            }

            string dest = Path.Combine(exeDirectory, "OUTPUT_FILES");
            Directory.CreateDirectory(dest);

            if (!(folderInput == null))
            {
                dest = Path.Combine(exeDirectory, "OUTPUT_FILES", Path.GetDirectoryName(pacPathInput.Replace(folderInput, "").TrimStart('\\')));
                Directory.CreateDirectory(dest);
                dest = Path.Combine(dest, Path.GetFileName(pacPathInput));
            } else
            {
                if (pacSwitchName.Contains("playercommon_SWITCH"))
                {
                    pacPathInput = "playercommon.pac";
                }
                dest = Path.Combine(dest, Path.GetFileName(pacPathInput));
            }
            if (File.Exists(dest))
            {
                File.Delete(dest);
            }
            MoveAfterHAP(Path.Combine(exeDirectory, "temp", pacSwitchName), dest, false);

            Console.WriteLine("");
            finishedFiles++;
        }

        if (delete && Directory.Exists(Path.Combine(exeDirectory, "temp")))
        {
            Directory.Delete(Path.Combine(exeDirectory, "temp"), true);
        }

        if (finishedFiles > 0)
        {
            Console.WriteLine("\u001b[92mCompleted!\u001b[37m");
            Console.WriteLine("Check out the OUTPUT_FILES folder!");
            if (!(folderInput == null))
            {
                Console.WriteLine("\nThis folder contains only all CONVERTED");
                Console.WriteLine("files - so replace all files over the");
                Console.WriteLine("original mod so you don't miss any!");
                
                if (hasSciptGedit)
                {
                    Console.WriteLine("\u001b[95mScript or gedit folder detected! These are\nmost likely incompatible with the Switch\nso I'd suggest leaving these out.\u001b[37m");
                }
            }
        } else
        {
            Console.WriteLine("Enter a valid file next time smh");
            Console.WriteLine("\u001b[95mThis tool is intended for .RFL (physics etc)\nconverting, or transferring .RFLs to their\nlower-res Switch version .pac files.\u001b[37m");
        }

        if (wait)
        {
            Console.ReadKey(true);
        }
    }

    static void CompareAndModifyFiles(string filePath1, string filePath2, string filePathPC)
    {
        byte[] fileBytes1 = File.ReadAllBytes(filePath1);
        byte[] fileBytesPC = File.ReadAllBytes(filePathPC);

        int groupSize = 4;
        string exeDirectory = Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
        int numGroups = fileBytes1.Length / groupSize;
        int numGroups2 = fileBytesPC.Length / groupSize;
        int num1 = 0;
        int num2 = 0;
        for (int groupIndex1 = 0, groupIndex2 = 0; groupIndex1 < numGroups && groupIndex2 < numGroups2; groupIndex1++, groupIndex2++)
        {
            byte[] group1 = new byte[groupSize];
            byte[] group2 = new byte[groupSize];

            Buffer.BlockCopy(fileBytes1, groupIndex1 * groupSize, group1, 0, groupSize);
            Buffer.BlockCopy(fileBytesPC, groupIndex2 * groupSize, group2, 0, groupSize);
            //Console.WriteLine(BitConverter.ToString(group1) + "   " + BitConverter.ToString(group2) + "   " + (groupIndex1*4).ToString("X"));

            if (!AreGroupsEqual(group1, group2))
            {
                if (IsGroupAllZeroes(group1))
                {
                    // we found 00 00 00 00 in group1 which isnt 0s in group2?
                    byte[] nextBigGroup1 = new byte[8];
                    byte[] nextBigGroupBackup = new byte[8];
                    byte[] BigGroup2 = new byte[8];
                    Buffer.BlockCopy(fileBytes1, (groupIndex1 + 1) * groupSize, nextBigGroup1, 0, groupSize * 2);
                    Buffer.BlockCopy(fileBytes1, (groupIndex1 + 2) * groupSize, nextBigGroupBackup, 0, groupSize * 2);
                    Buffer.BlockCopy(fileBytesPC, groupIndex2 * groupSize, BigGroup2, 0, groupSize * 2);
                    
                    //for (int i = 0; i < nextBigGroup1.Length; i++)
                    //{
                    //    Console.Write(nextBigGroup1[i].ToString("X2") + " ");
                    //}
                    //Console.WriteLine();
                    //for (int i = 0; i < BigGroup2.Length; i++)
                    //{
                    //    Console.Write(BigGroup2[i].ToString("X2") + " ");
                    //}
                    //Console.WriteLine();

                    //if (groupIndex1 * 4 > 31000)
                    //{
                    //    Console.ReadKey(true);
                    //}

                    int checkThenIterate = 0;
                    if (AreGroupsEqual(nextBigGroup1, BigGroup2))
                    {
                        checkThenIterate = 1;
                    } else if (AreGroupsEqual(nextBigGroupBackup, BigGroup2))
                    {
                        checkThenIterate = 2;
                        //Console.WriteLine("ayooo");
                        //Console.ReadKey(true);
                    }

                    if (checkThenIterate > 0)
                    {
                        for (int w = 0; w < checkThenIterate; w++)
                        {
                            //Console.WriteLine("equal innit");
                            using (StreamWriter writer = new StreamWriter(Path.Combine(exeDirectory, "temp\\00pos.txt"), true))
                            {
                                writer.WriteLine((groupIndex1 * groupSize).ToString());
                            }
                            ModifyFile(filePath1, (groupIndex1 - num1) * groupSize, true, 4);
                            groupIndex1++;
                            num1++;
                        }
                        continue;
                    } 
                }
                if (IsGroupAllZeroes(group2))
                {
                    // we found 00 00 00 00 in group2 which isnt 0s in group1?
                    byte[] nextBigGroup2 = new byte[8];
                    byte[] BigGroup1 = new byte[8];
                    byte[] extraCheck1 = new byte[4];
                    byte[] extraCheck2 = new byte[12];

                    // repeat 3 times, updating nextbiggroup2 up by a group
                    int i;
                    for (i = 1; i < 4; i++)
                    {
                        Buffer.BlockCopy(fileBytesPC, (groupIndex2 + i) * groupSize, nextBigGroup2, 0, 8);
                        Buffer.BlockCopy(fileBytes1, groupIndex1 * groupSize, BigGroup1, 0, 8);
                        //Console.WriteLine(BitConverter.ToString(nextBigGroup2) + "   " + BitConverter.ToString(BigGroup1));

                        if (AreGroupsEqual(nextBigGroup2, BigGroup1))
                        {
                            ModifyFile(filePath2, (groupIndex2 - num2) * groupSize, true, 4 * i);
                            groupIndex2 += i;
                            num2 += i;
                            break;
                        } else
                        { // since some values such as cameras are different this else statement checks if thats the case here
                            Buffer.BlockCopy(fileBytesPC, (groupIndex2 + i) * groupSize, extraCheck1, 0, 4);
                            Buffer.BlockCopy(fileBytesPC, (groupIndex2 + i + 1) * groupSize, extraCheck2, 0, 12);
                            string extraCheckBytes = BitConverter.ToString(extraCheck1);
                            if (extraCheckBytes != "00-00-00-00" && BitConverter.ToString(extraCheck2) == "00-00-00-00-00-00-00-00-00-00-00-00")
                            {
                                Buffer.BlockCopy(fileBytes1, groupIndex1 * groupSize, extraCheck1, 0, 4);
                                Buffer.BlockCopy(fileBytes1, (groupIndex1 + 1) * groupSize, extraCheck2, 0, 12);
                                string extraCheckBytes2 = BitConverter.ToString(extraCheck1);
                                if (extraCheckBytes2 != "00-00-00-00" && BitConverter.ToString(extraCheck2) == "00-00-00-00-00-00-00-00-00-00-00-00")
                                {
                                    ModifyFile(filePath2, (groupIndex2 - num2) * groupSize, true, 4 * i);
                                    groupIndex2 += i;
                                    num2 += i;
                                    break;
                                }
                            }
                        }
                    }
                    if (i < 4)
                    {
                        continue;
                    }
                }
            }
        }
    }

    static bool AreGroupsEqual(byte[] group1, byte[] group2)
    {
        return StructuralComparisons.StructuralEqualityComparer.Equals(group1, group2);
    }

    static bool IsGroupAllZeroes(byte[] group)
    {
        foreach (byte b in group)
        {
            if (b != 0x00)
                return false;
        }

        return true;
    }

    static void MoveRFLdata(string filePath1, string filePath2, long chunkFromStart, long chunkFromEnd)
    {
        using (FileStream sourceStream = File.OpenRead(filePath2))
        using (FileStream destinationStream = File.OpenWrite(filePath1))
        {
            sourceStream.Position = chunkFromStart;
            destinationStream.Position = chunkFromStart;

            int bufferSize = 4096;  // Adjust the buffer size as needed
            byte[] buffer = new byte[bufferSize];
            int bytesRead;
            long bytesRemaining = chunkFromEnd - chunkFromStart;

            while (bytesRemaining > 0)
            {
                int bytesToRead = (int)Math.Min(bufferSize, bytesRemaining);
                bytesRead = sourceStream.Read(buffer, 0, bytesToRead);
                destinationStream.Write(buffer, 0, bytesRead);
                bytesRemaining -= bytesRead;
            }
        }
    }
    static void ModifyFile(string filePath, int position, bool delete, int numBytes)
    {
        byte[] fileBytes = File.ReadAllBytes(filePath);
        int fileSize = fileBytes.Length;

        if (delete)
        {
            // Create a new array with size reduced by numBytes
            byte[] newFileBytes = new byte[fileSize - numBytes];

            // Copy the bytes before the specified position
            Buffer.BlockCopy(fileBytes, 0, newFileBytes, 0, position);

            // Copy the bytes after the deleted position
            Buffer.BlockCopy(fileBytes, position + numBytes, newFileBytes, position, fileSize - position - numBytes);

            // Write the modified bytes back to the file
            File.WriteAllBytes(filePath, newFileBytes);
        }
        else
        {
            // Create a new array with size increased by numBytes
            byte[] newFileBytes = new byte[fileSize + numBytes];

            // Copy the bytes before the specified position
            Buffer.BlockCopy(fileBytes, 0, newFileBytes, 0, position);

            // Insert the specified number of 00 bytes
            for (int i = 0; i < numBytes; i++)
            {
                newFileBytes[position + i] = 0x00;
            }

            // Copy the bytes after the inserted position
            Buffer.BlockCopy(fileBytes, position, newFileBytes, position + numBytes, fileSize - position);

            // Write the modified bytes back to the file
            File.WriteAllBytes(filePath, newFileBytes);
        }
    }

    static bool DoesUrlExist(string url)
    {
        try
        {
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.Method = "HEAD";

            using (var response = (HttpWebResponse)request.GetResponse())
            {
                return response.StatusCode == HttpStatusCode.OK;
            }
        }
        catch (WebException)
        {
            return false;
        }
    }

    static void JoinFiles(string file1, string file2)
    {
        using (FileStream stream1 = new FileStream(file1, FileMode.Append))
        using (FileStream stream2 = new FileStream(file2, FileMode.Open))
        {
            byte[] buffer = new byte[4096];
            int bytesRead;

            while ((bytesRead = stream2.Read(buffer, 0, buffer.Length)) > 0)
            {
                stream1.Write(buffer, 0, bytesRead);
            }
        }

        File.Delete(file2);
    }

    static void DownloadFile(string fileName)
    {
        string exeDirectory = Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
        using (WebClient webClient = new WebClient())
        {
            webClient.DownloadFile("https://github.com/Blurro/hmmm/raw/main/SwitchFiles/" + fileName, Path.Combine(exeDirectory, "unmodified_switchfiles", fileName));
        }
    }

    static void RunHedgeArcPack(string inputFile)
    {
        string exeDirectory = Path.GetDirectoryName(Assembly.GetEntryAssembly().Location);
        ProcessStartInfo startInfo1 = new ProcessStartInfo(Path.Combine(exeDirectory, "HedgeArcPack.exe"), "\"" + inputFile + "\"")
        {
            WindowStyle = ProcessWindowStyle.Hidden,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false
        };
        Process procc = Process.Start(startInfo1);
        procc.StandardInput.Close();
        procc.StandardOutput.ReadToEnd();
        procc.StandardError.ReadToEnd();
        procc.WaitForExit();

        // so like c:\folder\playercommon being moved to c:\temp\playercommon with 'playercommon' being subject to change via prior stuff
        MoveAfterHAP(inputFile, Path.Combine(exeDirectory, "temp\\", Path.ChangeExtension(Path.GetFileName(inputFile), null)), true);
    }

    static void MoveAfterHAP(string inputFile, string dest, bool directory)
    {
        int i = 0;
        while (i < 2)
        {
            try
            {
                if (directory)
                {
                    Directory.Move(Path.ChangeExtension(inputFile, null), dest);
                } else
                {
                    File.Move(inputFile, dest);
                }
                break;
            }
            catch
            {
                i++;
                if (i == 2)
                {
                    Console.WriteLine("\u001b[31mStill can't find " + Path.ChangeExtension(Path.GetFileName(inputFile), null) + " folder!\u001b[37m");
                    Console.ReadKey(true);
                    Environment.Exit(0);
                }
                else
                {
                    Console.WriteLine("\u001b[31mUh oh, error! (HedgeArcPack failed)\u001b[37m");
                    Console.WriteLine("");
                    Console.WriteLine("Please manually drag (don't move anything!!):");
                    if (directory)
                    {
                        Console.WriteLine(Path.Combine(Path.GetFileName(Path.GetDirectoryName(inputFile)), Path.GetFileName(inputFile)));
                        Console.WriteLine("onto HedgeArcPack, then press any key to continue");
                        Console.WriteLine("OR close this and right click -> run as admin");
                    } else
                    {
                        Console.WriteLine(Path.Combine(Path.GetFileName(Path.GetDirectoryName(inputFile)), Path.ChangeExtension(Path.GetFileName(inputFile), null)));
                        Console.WriteLine("onto HedgeArcPack and enter in 'frontiers'");
                        Console.WriteLine("then press any key to continue");
                    }
                    Console.ReadKey(true);
                }
            }
        }
    }

    static void ScanFilesMore(string filePath1, string filePath2, string inputFile, ref List<string> moreData1, ref List<int> positions, long endPos)
    {
        //Console.WriteLine(endPos);
        byte[] buffer = File.ReadAllBytes(filePath1);
        byte[] buffer2 = File.ReadAllBytes(filePath2);
        byte[] buffer3 = File.ReadAllBytes(inputFile);
        for (int i = 48; i < endPos; i += 4)
        {
            if (!(BitConverter.ToString(buffer, i, 4) == BitConverter.ToString(buffer2, i, 4)) && BitConverter.ToString(buffer, i + 4, 12) == "00-00-00-00-00-00-00-00-00-00-00-00")
            {
                positions.Add(i);
                if (moreData1.IndexOf(BitConverter.ToString(buffer, i, 4)) == -1)
                {
                    moreData1.Add(BitConverter.ToString(buffer, i, 4));
                    //Console.WriteLine(BitConverter.ToString(buffer2, i, 4).Replace("-", " ") + "  " + BitConverter.ToString(buffer, i, 4).Replace("-", " "));
                } else
                {
                    moreData1.Add("search");
                }
            } else if (!(BitConverter.ToString(buffer, i, 4) == BitConverter.ToString(buffer2, i, 4)) && BitConverter.ToString(buffer, i, 4) == BitConverter.ToString(buffer3, i, 4))
            { // this checks any remaining differing bytes, if the mod file bytes match the version its moving from it'll change to the version its moving to. If the value's been changed in the mod, retain that change.
                moreData1.Add("pick");
                positions.Add(i);
            }
        }
        Console.WriteLine("Found " + positions.Count + " pointers to update");
    }
}