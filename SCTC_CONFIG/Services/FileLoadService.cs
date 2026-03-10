using SCTC_CONFIG.Helpers;
using SCTC_CONFIG.Models;
using System.IO;
using System.Text;

namespace SCTC_CONFIG.Services;

public class LoadResult
{
    public bool Success { get; set; }
    public string? ErrorMessage { get; set; }

    public List<MainEntryModel> MainEntries { get; set; } = new();
    public List<string> MainLines { get; set; } = new();
    public Encoding MainEncoding { get; set; } = Encoding.UTF8;
    public string MainLineEnding { get; set; } = "\r\n";

    public List<MainEntryModel> ScdEntries { get; set; } = new();
    public List<string> ScdLines { get; set; } = new();
    public Encoding ScdEncoding { get; set; } = Encoding.UTF8;
    public string ScdLineEnding { get; set; } = "\r\n";

    public List<DriverRowModel> DriverRows { get; set; } = new();
    public List<string> DriverLines { get; set; } = new();
    public Encoding DriverEncoding { get; set; } = Encoding.UTF8;
    public string DriverLineEnding { get; set; } = "\r\n";

    public List<FunctionFileModel> FunctionFiles { get; set; } = new();
    public List<AlarmFileModel> AlarmFiles { get; set; } = new();
}

public class FileLoadService
{
    private static readonly string[] TargetMainKeys =
    {
        "UseStandaloneMode",
        "UseVirtualDriver",
        "UseVirtualInterlock",
        "UseFunctionSimulationMode"
    };

    private static readonly string[] TargetScdKeys =
    {
        "SimulationSCD",
        "SimulationFunction",
        "SimulationDriver"
    };

    public LoadResult Load(string rootPath)
    {
        var result = new LoadResult();

        // General.csv is required
        string generalPath = Path.Combine(rootPath, "General.csv");
        if (!File.Exists(generalPath))
        {
            result.Success = false;
            result.ErrorMessage = $"General.csv 파일을 찾을 수 없습니다:\n{generalPath}";
            return result;
        }

        LoadMainCsv(generalPath, result);

        string scdPath = Path.Combine(rootPath, "SCD", "SCDGeneral.csv");
        if (!File.Exists(scdPath))
        {
            result.Success = false;
            result.ErrorMessage = $"SCDGeneral.csv 파일을 찾을 수 없습니다:\n{scdPath}";
            return result;
        }

        LoadScdCsv(scdPath, result);

        // Driver/Driver.csv is optional
        string driverPath = Path.Combine(rootPath, "Driver", "Driver.csv");
        if (File.Exists(driverPath))
            LoadDriverCsv(driverPath, result);

        // Function/*.csv optional
        string functionDir = Path.Combine(rootPath, "Function");
        if (Directory.Exists(functionDir))
            LoadFunctionFiles(functionDir, result);

        // Alarm/*.csv optional
        string alarmDir = Path.Combine(rootPath, "Alarm");
        if (Directory.Exists(alarmDir) && !LoadAlarmFiles(alarmDir, result))
            return result;

        result.Success = true;
        return result;
    }

    private static void LoadMainCsv(string filePath, LoadResult result)
    {
        var parsed = LoadToggleCsvEntries(filePath, TargetMainKeys);
        result.MainEntries = parsed.entries;
        result.MainLines = parsed.lines;
        result.MainEncoding = parsed.encoding;
        result.MainLineEnding = parsed.lineEnding;
    }

    private static void LoadScdCsv(string filePath, LoadResult result)
    {
        var parsed = LoadToggleCsvEntries(filePath, TargetScdKeys);
        result.ScdEntries = parsed.entries;
        result.ScdLines = parsed.lines;
        result.ScdEncoding = parsed.encoding;
        result.ScdLineEnding = parsed.lineEnding;
    }

    private static (List<MainEntryModel> entries, List<string> lines, Encoding encoding, string lineEnding) LoadToggleCsvEntries(
        string filePath,
        IEnumerable<string> targetKeys)
    {
        using var reader = new StreamReader(filePath, detectEncodingFromByteOrderMarks: true);
        string rawText = reader.ReadToEnd();
        Encoding encoding = reader.CurrentEncoding;
        string lineEnding = rawText.Contains("\r\n") ? "\r\n" : "\n";
        var lines = rawText.Split(new[] { lineEnding }, StringSplitOptions.None).ToList();
        var entries = new List<MainEntryModel>();
        var keySet = new HashSet<string>(targetKeys, StringComparer.Ordinal);

        for (int i = 0; i < lines.Count; i++)
        {
            string line = lines[i];
            if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('#'))
                continue;

            var columns = line.Split(',');
            if (columns.Length < 2)
                continue;

            string key = columns[0].Trim();
            if (!keySet.Contains(key))
                continue;

            entries.Add(new MainEntryModel
            {
                Key = key,
                Value = columns[1].Trim(),
                LineIndex = i
            });
        }

        return (entries, lines, encoding, lineEnding);
    }

    private static void LoadDriverCsv(string filePath, LoadResult result)
    {
        using var reader = new StreamReader(filePath, detectEncodingFromByteOrderMarks: true);
        string rawText = reader.ReadToEnd();
        result.DriverEncoding = reader.CurrentEncoding;
        result.DriverLineEnding = rawText.Contains("\r\n") ? "\r\n" : "\n";
        result.DriverLines = rawText.Split(new[] { result.DriverLineEnding }, StringSplitOptions.None).ToList();

        bool headerSkipped = false;
        for (int i = 0; i < result.DriverLines.Count; i++)
        {
            string line = result.DriverLines[i];
            if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('#'))
                continue;

            if (!headerSkipped)
            {
                headerSkipped = true;
                continue;
            }

            var columns = line.Split(',');
            result.DriverRows.Add(new DriverRowModel
            {
                OriginalLine = line,
                OriginalColumns = columns,
                LineIndex = i
            });
        }
    }

    private static void LoadFunctionFiles(string functionDir, LoadResult result)
    {
        var csvFiles = Directory.GetFiles(functionDir, "*.csv")
            .Where(f =>
            {
                var name = Path.GetFileNameWithoutExtension(f);
                return !name.Contains("Def", StringComparison.OrdinalIgnoreCase) &&
                       !name.Contains("IOInfo", StringComparison.OrdinalIgnoreCase);
            })
            .OrderBy(f => Path.GetFileName(f))
            .ToList();

        foreach (var csvFile in csvFiles)
        {
            using var reader = new StreamReader(csvFile, detectEncodingFromByteOrderMarks: true);
            string rawText = reader.ReadToEnd();
            var encoding = reader.CurrentEncoding;
            string lineEnding = rawText.Contains("\r\n") ? "\r\n" : "\n";
            var lines = rawText.Split(new[] { lineEnding }, StringSplitOptions.None).ToList();

            var model = new FunctionFileModel
            {
                FilePath = csvFile,
                FileName = Path.GetFileName(csvFile),
                Lines = lines,
                FileEncoding = encoding,
                LineEnding = lineEnding
            };

            bool headerSkipped = false;
            var col6Values = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            for (int i = 0; i < lines.Count; i++)
            {
                string line = lines[i];
                if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('#'))
                    continue;

                if (!headerSkipped)
                {
                    headerSkipped = true;
                    continue;
                }

                var cols = line.Split(',');
                if (cols.Length > 6)
                {
                    model.DataLineIndices.Add(i);
                    col6Values.Add(cols[6].Trim());
                }
            }

            if (col6Values.Count == 0)
            {
                model.InitialIsSimulation = false;
                model.IsInconsistent = false;
            }
            else if (col6Values.Count == 1)
            {
                model.InitialIsSimulation = col6Values.First().Equals("TRUE", StringComparison.OrdinalIgnoreCase);
                model.IsInconsistent = false;
            }
            else
            {
                model.InitialIsSimulation = false;
                model.IsInconsistent = true;
            }

            result.FunctionFiles.Add(model);
        }
    }

    private static bool LoadAlarmFiles(string alarmDir, LoadResult result)
    {
        var csvFiles = Directory.GetFiles(alarmDir, "*.csv")
            .OrderBy(f => Path.GetFileName(f))
            .ToList();

        foreach (var csvFile in csvFiles)
        {
            using var reader = new StreamReader(csvFile, detectEncodingFromByteOrderMarks: true);
            string rawText = reader.ReadToEnd();
            var encoding = reader.CurrentEncoding;
            string lineEnding = rawText.Contains("\r\n") ? "\r\n" : "\n";
            var lines = rawText.Split(new[] { lineEnding }, StringSplitOptions.None).ToList();

            var model = new AlarmFileModel
            {
                FilePath = csvFile,
                FileName = Path.GetFileName(csvFile),
                Lines = lines,
                FileEncoding = encoding,
                LineEnding = lineEnding
            };

            bool headerSkipped = false;
            bool allDisplayOnly = true;
            for (int i = 0; i < lines.Count; i++)
            {
                string line = lines[i];
                if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('#'))
                    continue;

                if (!headerSkipped)
                {
                    model.HeaderColumnCount = lines[i].Split(',').Length;
                    headerSkipped = true;
                    continue;
                }

                model.DataLineIndices.Add(i);

                if (!AlarmCsvLine.TryParse(line, model.HeaderColumnCount, out var parsedLine, out string errorMessage))
                {
                    result.Success = false;
                    result.ErrorMessage = $"Alarm CSV 로드 실패: {csvFile} ({i + 1}행)\n{errorMessage}";
                    return false;
                }

                if (parsedLine.GetField(AlarmCsvLine.DisplayOnlyActionColumnIndex).Trim() != "DisplayOnly:1" ||
                    parsedLine.GetField(AlarmCsvLine.DisplayOnlyRecoveryColumnIndex).Trim() != "DisplayOnly:1" ||
                    parsedLine.GetField(AlarmCsvLine.DisplayOnlyNoteColumnIndex).Trim() != "DisplayOnly:1")
                {
                    allDisplayOnly = false;
                }
            }

            model.InitialIsDisplayOnly = model.DataLineIndices.Count > 0 && allDisplayOnly;
            result.AlarmFiles.Add(model);
        }

        return true;
    }
}
