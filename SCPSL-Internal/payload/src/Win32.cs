using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace SLInternal
{
    // Keyboard state via GetAsyncKeyState (works whether the game uses the legacy
    // Input Manager or the Input System) and mouse output via SendInput.
    internal static class Win32
    {
        public const int VK_LBUTTON = 0x01;

        private const uint INPUT_MOUSE = 0;
        private const uint MOUSEEVENTF_MOVE = 0x0001;
        private const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
        private const uint MOUSEEVENTF_LEFTUP = 0x0004;

        [StructLayout(LayoutKind.Sequential)]
        private struct MOUSEINPUT
        {
            public int dx;
            public int dy;
            public uint mouseData;
            public uint dwFlags;
            public uint time;
            public IntPtr dwExtraInfo;
        }

        // x64 layout: DWORD type, 4 bytes padding, then the union (MOUSEINPUT is its largest member).
        [StructLayout(LayoutKind.Explicit)]
        private struct INPUT
        {
            [FieldOffset(0)] public uint type;
            [FieldOffset(8)] public MOUSEINPUT mi;
        }

        [DllImport("user32.dll")] private static extern short GetAsyncKeyState(int vKey);
        [DllImport("user32.dll")] private static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
        [DllImport("kernel32.dll")] private static extern uint GetCurrentProcessId();
        [DllImport("user32.dll", SetLastError = true)] private static extern uint SendInput(uint count, INPUT[] inputs, int size);

        private static readonly INPUT[] One = new INPUT[1];
        private static readonly int InputSize = Marshal.SizeOf(typeof(INPUT));
        private static readonly uint Pid = GetCurrentProcessId();

        public static bool KeyDown(int vk) => vk > 0 && vk < 256 && (GetAsyncKeyState(vk) & 0x8000) != 0;

        public static bool GameFocused()
        {
            IntPtr fg = GetForegroundWindow();
            if (fg == IntPtr.Zero)
                return false;
            GetWindowThreadProcessId(fg, out uint pid);
            return pid == Pid;
        }

        public static bool MoveMouse(int dx, int dy)
        {
            if (dx == 0 && dy == 0)
                return true;
            return Send(MOUSEEVENTF_MOVE, dx, dy);
        }

        public static bool LeftButton(bool down) => Send(down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP, 0, 0);

        private static bool Send(uint flags, int dx, int dy)
        {
            One[0] = new INPUT { type = INPUT_MOUSE, mi = new MOUSEINPUT { dx = dx, dy = dy, dwFlags = flags } };
            return SendInput(1, One, InputSize) == 1;
        }

        // First virtual key currently held, for the key binder. Generic Shift/Ctrl/Alt are
        // skipped so the left/right variant gets bound.
        public static int FirstKeyDown()
        {
            for (int vk = 1; vk < 256; vk++)
            {
                if (vk == 0x10 || vk == 0x11 || vk == 0x12) // generic Shift/Ctrl/Alt; the L/R variants follow
                    continue;
                if (KeyDown(vk))
                    return vk;
            }
            return 0;
        }

        public static bool AnyKeyDown() => FirstKeyDown() != 0;

        private static readonly Dictionary<int, string> Names = new Dictionary<int, string>
        {
            { 0, "none" }, { 0x01, "Mouse1" }, { 0x02, "Mouse2" }, { 0x04, "Mouse3" }, { 0x05, "Mouse4" }, { 0x06, "Mouse5" },
            { 0x08, "Backspace" }, { 0x09, "Tab" }, { 0x0D, "Enter" }, { 0x14, "CapsLock" }, { 0x1B, "Esc" }, { 0x20, "Space" },
            { 0x21, "PageUp" }, { 0x22, "PageDown" }, { 0x23, "End" }, { 0x24, "Home" }, { 0x25, "Left" }, { 0x26, "Up" },
            { 0x27, "Right" }, { 0x28, "Down" }, { 0x2D, "Insert" }, { 0x2E, "Delete" },
            { 0xA0, "LShift" }, { 0xA1, "RShift" }, { 0xA2, "LCtrl" }, { 0xA3, "RCtrl" }, { 0xA4, "LAlt" }, { 0xA5, "RAlt" },
            { 0xC0, "`" },
        };

        public static string KeyName(int vk)
        {
            if (Names.TryGetValue(vk, out string n)) return n;
            if (vk >= 0x30 && vk <= 0x39 || vk >= 0x41 && vk <= 0x5A) return ((char)vk).ToString();
            if (vk >= 0x70 && vk <= 0x87) return "F" + (vk - 0x6F);
            return "0x" + vk.ToString("X2");
        }
    }

    internal sealed class KeyLatch
    {
        private bool _prev;

        public bool Pressed(int vk)
        {
            bool down = Win32.KeyDown(vk);
            bool edge = down && !_prev;
            _prev = down;
            return edge;
        }
    }
}
