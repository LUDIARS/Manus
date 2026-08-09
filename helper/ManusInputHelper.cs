// ManusInputHelper — stdin JSON Lines で受けた入力コマンドを Win32 SendInput で注入する常駐ヘルパ。
// .NET Framework 4.x の csc.exe 単体でビルドできること (外部 NuGet 依存禁止) がこのファイルの制約。
// プロトコル: 1 行 = 1 リクエスト {"id":n,"op":"...",...} → 1 行レスポンス {"id":n,"ok":true|false,"error":...}
// 診断は stderr のみ (stdout はレスポンス専用のデータストリーム)。
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Web.Script.Serialization;

namespace Manus
{
    internal static class Native
    {
        public const uint INPUT_MOUSE = 0;
        public const uint INPUT_KEYBOARD = 1;
        public const uint KEYEVENTF_KEYUP = 0x0002;
        public const uint KEYEVENTF_UNICODE = 0x0004;
        public const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
        public const uint MOUSEEVENTF_MOVE = 0x0001;
        public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
        public const uint MOUSEEVENTF_LEFTUP = 0x0004;
        public const uint MOUSEEVENTF_RIGHTDOWN = 0x0008;
        public const uint MOUSEEVENTF_RIGHTUP = 0x0010;
        public const uint MOUSEEVENTF_MIDDLEDOWN = 0x0020;
        public const uint MOUSEEVENTF_MIDDLEUP = 0x0040;
        public const uint MOUSEEVENTF_WHEEL = 0x0800;
        public const uint MOUSEEVENTF_HWHEEL = 0x1000;

        [StructLayout(LayoutKind.Sequential)]
        public struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct KEYBDINPUT
        {
            public ushort wVk;
            public ushort wScan;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        // INPUT は union。フィールドの大きい MOUSEINPUT に合わせた明示レイアウト。
        [StructLayout(LayoutKind.Explicit)]
        public struct INPUT
        {
            [FieldOffset(0)] public uint type;
            [FieldOffset(8)] public MOUSEINPUT mi;
            [FieldOffset(8)] public KEYBDINPUT ki;
        }

        [DllImport("user32.dll", SetLastError = true)]
        public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    }

    public static class Program
    {
        private static readonly JavaScriptSerializer Json = new JavaScriptSerializer();

        public static int Main(string[] args)
        {
            var stdin = new StreamReader(Console.OpenStandardInput(), new UTF8Encoding(false));
            var stdout = new StreamWriter(Console.OpenStandardOutput(), new UTF8Encoding(false)) { AutoFlush = true };
            stdout.WriteLine("{\"event\":\"ready\"}");
            string line;
            while ((line = stdin.ReadLine()) != null)
            {
                if (line.Trim().Length == 0) continue;
                object idObj = null;
                try
                {
                    var req = (Dictionary<string, object>)Json.DeserializeObject(line);
                    req.TryGetValue("id", out idObj);
                    var op = (string)req["op"];
                    Dispatch(op, req);
                    stdout.WriteLine("{\"id\":" + IdLiteral(idObj) + ",\"ok\":true}");
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine("[helper] error: " + ex.Message);
                    stdout.WriteLine("{\"id\":" + IdLiteral(idObj) + ",\"ok\":false,\"error\":" + Quote(ex.Message) + "}");
                }
            }
            return 0; // stdin close = 親 (server) 終了 → 正常終了
        }

        private static string IdLiteral(object id)
        {
            if (id == null) return "null";
            return Convert.ToInt64(id).ToString();
        }

        private static string Quote(string s)
        {
            return Json.Serialize(s);
        }

        private static void Dispatch(string op, Dictionary<string, object> req)
        {
            switch (op)
            {
                case "ping":
                    break;
                case "text":
                    InjectText((string)req["text"]);
                    break;
                case "key":
                    InjectKeys(ToIntList(req["vks"]), (string)req["action"]);
                    break;
                case "mouse_move":
                    MouseMove(Convert.ToInt32(req["dx"]), Convert.ToInt32(req["dy"]));
                    break;
                case "mouse_button":
                    MouseButton((string)req["button"], (string)req["action"]);
                    break;
                case "wheel":
                    Wheel(GetInt(req, "dx"), GetInt(req, "dy"));
                    break;
                default:
                    throw new ArgumentException("unknown op: " + op);
            }
        }

        private static int GetInt(Dictionary<string, object> req, string key)
        {
            object v;
            return req.TryGetValue(key, out v) && v != null ? Convert.ToInt32(v) : 0;
        }

        private static List<ushort> ToIntList(object v)
        {
            var result = new List<ushort>();
            foreach (var item in (object[])v) result.Add(Convert.ToUInt16(item));
            return result;
        }

        private static void Send(List<Native.INPUT> inputs)
        {
            if (inputs.Count == 0) return;
            var arr = inputs.ToArray();
            uint sent = Native.SendInput((uint)arr.Length, arr, Marshal.SizeOf(typeof(Native.INPUT)));
            if (sent != arr.Length)
                throw new InvalidOperationException("SendInput injected " + sent + "/" + arr.Length + " (err=" + Marshal.GetLastWin32Error() + ")");
        }

        private static Native.INPUT KeyEvent(ushort vk, ushort scan, uint flags)
        {
            var input = new Native.INPUT { type = Native.INPUT_KEYBOARD };
            input.ki = new Native.KEYBDINPUT { wVk = vk, wScan = scan, dwFlags = flags, time = 0, dwExtraInfo = IntPtr.Zero };
            return input;
        }

        private const ushort VK_RETURN = 0x0D;
        private const ushort VK_TAB = 0x09;

        private static void InjectText(string text)
        {
            var inputs = new List<Native.INPUT>();
            foreach (char c in text)
            {
                if (c == '\r') continue;
                if (c == '\n') { AddPress(inputs, VK_RETURN); continue; }
                if (c == '\t') { AddPress(inputs, VK_TAB); continue; }
                // KEYEVENTF_UNICODE は UTF-16 code unit 単位。サロゲートペアもユニットごとで正しく合成される。
                inputs.Add(KeyEvent(0, c, Native.KEYEVENTF_UNICODE));
                inputs.Add(KeyEvent(0, c, Native.KEYEVENTF_UNICODE | Native.KEYEVENTF_KEYUP));
            }
            Send(inputs);
        }

        private static void AddPress(List<Native.INPUT> inputs, ushort vk)
        {
            inputs.Add(KeyEvent(vk, 0, 0));
            inputs.Add(KeyEvent(vk, 0, Native.KEYEVENTF_KEYUP));
        }

        // 拡張キー (矢印/Home/End/Ins/Del/PgUp/PgDn/右Ctrl系/分割キー) は EXTENDEDKEY が必要
        private static readonly HashSet<ushort> ExtendedVks = new HashSet<ushort>
        {
            0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // PgUp PgDn End Home 矢印
            0x2D, 0x2E, 0x5B, 0x5C, 0x6F, 0x90              // Ins Del LWin RWin NumDiv NumLock
        };

        private static uint KeyFlags(ushort vk, bool up)
        {
            uint flags = up ? Native.KEYEVENTF_KEYUP : 0;
            if (ExtendedVks.Contains(vk)) flags |= Native.KEYEVENTF_EXTENDEDKEY;
            return flags;
        }

        private static void InjectKeys(List<ushort> vks, string action)
        {
            var inputs = new List<Native.INPUT>();
            if (action == "down" || action == "press")
                foreach (var vk in vks) inputs.Add(KeyEvent(vk, 0, KeyFlags(vk, false)));
            if (action == "up" || action == "press")
                for (int i = vks.Count - 1; i >= 0; i--) inputs.Add(KeyEvent(vks[i], 0, KeyFlags(vks[i], true)));
            Send(inputs);
        }

        private static Native.INPUT MouseEvent(int dx, int dy, uint data, uint flags)
        {
            var input = new Native.INPUT { type = Native.INPUT_MOUSE };
            input.mi = new Native.MOUSEINPUT { dx = dx, dy = dy, mouseData = data, dwFlags = flags, time = 0, dwExtraInfo = IntPtr.Zero };
            return input;
        }

        private static void MouseMove(int dx, int dy)
        {
            Send(new List<Native.INPUT> { MouseEvent(dx, dy, 0, Native.MOUSEEVENTF_MOVE) });
        }

        private static void MouseButton(string button, string action)
        {
            uint down, up;
            switch (button)
            {
                case "left": down = Native.MOUSEEVENTF_LEFTDOWN; up = Native.MOUSEEVENTF_LEFTUP; break;
                case "right": down = Native.MOUSEEVENTF_RIGHTDOWN; up = Native.MOUSEEVENTF_RIGHTUP; break;
                case "middle": down = Native.MOUSEEVENTF_MIDDLEDOWN; up = Native.MOUSEEVENTF_MIDDLEUP; break;
                default: throw new ArgumentException("unknown button: " + button);
            }
            var inputs = new List<Native.INPUT>();
            if (action == "down" || action == "click") inputs.Add(MouseEvent(0, 0, 0, down));
            if (action == "up" || action == "click") inputs.Add(MouseEvent(0, 0, 0, up));
            if (inputs.Count == 0) throw new ArgumentException("unknown action: " + action);
            Send(inputs);
        }

        private static void Wheel(int dx, int dy)
        {
            var inputs = new List<Native.INPUT>();
            if (dy != 0) inputs.Add(MouseEvent(0, 0, unchecked((uint)dy), Native.MOUSEEVENTF_WHEEL));
            if (dx != 0) inputs.Add(MouseEvent(0, 0, unchecked((uint)dx), Native.MOUSEEVENTF_HWHEEL));
            Send(inputs);
        }
    }
}
