# tools/odscap.ps1
#
# Minimal OutputDebugString capture (DebugView-style) for reading the mod's
# [xiii-*] telemetry without installing anything. Captures the global DBWIN
# stream for -Seconds, optionally filtering lines and writing them to a file.
#
#   powershell -File tools\odscap.ps1 -Seconds 40 -Filter "xiii-" -OutFile ods.log
#
# Only one DBWIN listener can run at a time system-wide -- close DebugView
# first if it is open.

param(
    [int]$Seconds = 30,
    [string]$Filter = "",
    [string]$OutFile = ""
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class OdsCap {
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateEvent(IntPtr attrs, bool manualReset, bool initialState, string name);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateFileMapping(IntPtr file, IntPtr attrs, uint protect,
                                           uint sizeHigh, uint sizeLow, string name);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr MapViewOfFile(IntPtr mapping, uint access, uint offHigh, uint offLow, UIntPtr size);
    [DllImport("kernel32.dll")]
    static extern bool SetEvent(IntPtr h);
    [DllImport("kernel32.dll")]
    static extern uint WaitForSingleObject(IntPtr h, uint ms);

    // DBWIN protocol: a 4 KB shared buffer (DWORD pid + message bytes) plus two
    // events; the debugger signals BUFFER_READY, a logging process fills the
    // buffer and signals DATA_READY.
    public static void Run(int seconds, string filter, string outFile) {
        IntPtr bufReady = CreateEvent(IntPtr.Zero, false, false, "DBWIN_BUFFER_READY");
        IntPtr dataReady = CreateEvent(IntPtr.Zero, false, false, "DBWIN_DATA_READY");
        IntPtr mapping = CreateFileMapping((IntPtr)(-1), IntPtr.Zero, 0x04 /*PAGE_READWRITE*/, 0, 4096, "DBWIN_BUFFER");
        if (bufReady == IntPtr.Zero || dataReady == IntPtr.Zero || mapping == IntPtr.Zero) {
            Console.Error.WriteLine("odscap: failed to create DBWIN objects (another listener running?)");
            return;
        }
        IntPtr view = MapViewOfFile(mapping, 4 /*FILE_MAP_READ*/, 0, 0, UIntPtr.Zero);
        if (view == IntPtr.Zero) { Console.Error.WriteLine("odscap: MapViewOfFile failed"); return; }

        var log = outFile.Length > 0 ? new System.IO.StreamWriter(outFile, false, Encoding.UTF8) : null;
        var deadline = DateTime.UtcNow.AddSeconds(seconds);
        var msg = new byte[4092];
        while (DateTime.UtcNow < deadline) {
            SetEvent(bufReady);
            if (WaitForSingleObject(dataReady, 500) != 0) continue;  // timeout: poll deadline
            int pid = Marshal.ReadInt32(view);
            Marshal.Copy((IntPtr)((long)view + 4), msg, 0, msg.Length);
            int len = Array.IndexOf(msg, (byte)0);
            if (len < 0) len = msg.Length;
            string s = Encoding.Default.GetString(msg, 0, len).TrimEnd('\r', '\n');
            if (s.Length == 0) continue;
            if (filter.Length > 0 && s.IndexOf(filter, StringComparison.OrdinalIgnoreCase) < 0) continue;
            string line = string.Format("[{0:HH:mm:ss.fff}] [pid {1}] {2}", DateTime.Now, pid, s);
            Console.WriteLine(line);
            if (log != null) { log.WriteLine(line); log.Flush(); }
        }
        if (log != null) log.Close();
    }
}
"@

[OdsCap]::Run($Seconds, $Filter, $OutFile)
