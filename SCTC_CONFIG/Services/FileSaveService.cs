using SCTC_CONFIG.ViewModels;
using System.IO;

namespace SCTC_CONFIG.Services;

public class FileSaveService
{
    public void Save(string rootPath, string? backupFolderName, CsvTabViewModel csvTab)
    {
        // 1. Backup if requested
        if (!string.IsNullOrWhiteSpace(backupFolderName))
        {
            string parentDir = Path.GetDirectoryName(rootPath)!;
            string backupPath = Path.Combine(parentDir, backupFolderName);
            CopyDirectory(rootPath, backupPath);
        }

        // 2. Save General.csv
        SaveMainCsv(rootPath, csvTab.MainPanel);

        // 3. Save Driver/Driver.csv
        SaveDriverCsv(rootPath, csvTab.DriverPanel);

        // 4. Save Function files (HasBeenToggled only)
        SaveFunctionFiles(csvTab.FunctionPanel);

        // 5. Save Alarm files (IsDisplayOnly only)
        SaveAlarmFiles(csvTab.AlarmPanel);
    }

    private static void SaveMainCsv(string rootPath, MainPanelViewModel panel)
    {
        if (panel.Lines.Count == 0) return;

        var lines = new List<string>(panel.Lines);
        foreach (var item in panel.Items)
        {
            if (item.LineIndex < 0 || item.LineIndex >= lines.Count) continue;

            var cols = lines[item.LineIndex].Split(',');
            if (cols.Length >= 2)
            {
                cols[1] = item.IsEnabled ? "TRUE" : "FALSE";
                lines[item.LineIndex] = string.Join(",", cols);
            }
        }

        string filePath = Path.Combine(rootPath, "General.csv");
        File.WriteAllText(filePath, string.Join(panel.LineEnding, lines), panel.FileEncoding);
    }

    private static void SaveDriverCsv(string rootPath, DriverPanelViewModel panel)
    {
        if (panel.Lines.Count == 0) return;

        var lines = new List<string>(panel.Lines);
        foreach (var item in panel.Items)
        {
            if (!item.IsModified) continue;
            if (item.LineIndex < 0 || item.LineIndex >= lines.Count) continue;

            var cols = item.OriginalColumns.ToList();
            while (cols.Count <= 11) cols.Add(string.Empty);

            cols[2] = item.NeedLoad ? "TRUE" : "FALSE";
            cols[7] = item.Arg1;
            cols[8] = item.Arg2;
            cols[9] = item.Arg3;
            cols[10] = item.Arg4;
            cols[11] = item.Arg5;

            lines[item.LineIndex] = string.Join(",", cols);
        }

        string filePath = Path.Combine(rootPath, "Driver", "Driver.csv");
        File.WriteAllText(filePath, string.Join(panel.LineEnding, lines), panel.FileEncoding);
    }

    private static void SaveFunctionFiles(FunctionPanelViewModel panel)
    {
        foreach (var item in panel.Items)
        {
            if (!item.HasBeenToggled) continue;

            var lines = new List<string>(item.Model.Lines);
            string targetValue = item.IsSimulation ? "TRUE" : "FALSE";

            foreach (int idx in item.Model.DataLineIndices)
            {
                if (idx >= lines.Count) continue;
                var cols = lines[idx].Split(',');
                if (cols.Length > 6)
                {
                    cols[6] = targetValue;
                    lines[idx] = string.Join(",", cols);
                }
            }

            File.WriteAllText(item.Model.FilePath, string.Join(item.Model.LineEnding, lines), item.Model.FileEncoding);
        }
    }

    private static void SaveAlarmFiles(AlarmPanelViewModel panel)
    {
        foreach (var item in panel.Items)
        {
            if (!item.IsDisplayOnly) continue;

            var lines = new List<string>(item.Model.Lines);

            foreach (int idx in item.Model.DataLineIndices)
            {
                if (idx >= lines.Count) continue;
                var cols = lines[idx].Split(',').ToList();
                while (cols.Count <= 9) cols.Add(string.Empty);

                cols[7] = "DisplayOnly:1";
                cols[8] = "DisplayOnly:1";
                cols[9] = "DisplayOnly:1";
                lines[idx] = string.Join(",", cols);
            }

            File.WriteAllText(item.Model.FilePath, string.Join(item.Model.LineEnding, lines), item.Model.FileEncoding);
        }
    }

    private static void CopyDirectory(string sourceDir, string destDir)
    {
        Directory.CreateDirectory(destDir);
        foreach (var file in Directory.GetFiles(sourceDir))
        {
            File.Copy(file, Path.Combine(destDir, Path.GetFileName(file)), overwrite: true);
        }
        foreach (var dir in Directory.GetDirectories(sourceDir))
        {
            CopyDirectory(dir, Path.Combine(destDir, Path.GetFileName(dir)));
        }
    }
}
