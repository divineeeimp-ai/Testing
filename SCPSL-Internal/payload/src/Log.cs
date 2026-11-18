using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace SLInternal
{
    internal static class Log
    {
        private static readonly object Gate = new object();
        private static readonly Dictionary<string, float> LastByKey = new Dictionary<string, float>();
        private static string _file;

        public static string BaseDir { get; private set; }

        public static void Init(string dir)
        {
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
                return;
            BaseDir = dir;
            _file = Path.Combine(dir, "SLInternal.log");
        }

        public static void Info(string message) => Write("INFO", message);

        public static void Error(string message) => Write("ERROR", message);

        // Main thread only: an exception in a per-frame path is reported at most every 5 s per key.
        public static void Throttled(string key, Exception e)
        {
            float now = Time.unscaledTime;
            if (LastByKey.TryGetValue(key, out float last) && now - last < 5f)
                return;
            LastByKey[key] = now;
            Write("ERROR", key + ": " + e);
        }

        private static void Write(string level, string message)
        {
            string line = $"[{DateTime.Now:HH:mm:ss.fff}] [{level}] {message}";
            try { Debug.Log("[SLInternal] " + line); } catch { }
            if (_file == null)
                return;
            lock (Gate)
            {
                try { File.AppendAllText(_file, line + Environment.NewLine); } catch { }
            }
        }
    }
}
