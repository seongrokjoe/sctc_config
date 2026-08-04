using SCTC_CONFIG.Helpers;
using SCTC_CONFIG.ViewModels;
using System.IO;
using System.Text;

namespace SCTC_CONFIG.Services;

public sealed record AlarmStopModeDiffExportResult(string FilePath, int DifferenceCount);

public sealed class AlarmStopModeDiffExportService
{
    public AlarmStopModeDiffExportResult Export(AlarmPanelViewModel panel, DateTime exportedAt)
    {
        if (panel.Items.Count == 0)
            throw new InvalidOperationException("로드된 Alarm CSV 파일이 없습니다.");

        string? alarmDir = Path.GetDirectoryName(panel.Items[0].Model.FilePath);
        if (string.IsNullOrWhiteSpace(alarmDir))
            throw new InvalidOperationException("Alarm export 경로를 찾을 수 없습니다.");

        var reportLines = new List<string>
        {
            $"ExportedAt: {exportedAt:yyyy-MM-dd HH:mm:ss}",
            "Compare: StopMode_Run != StopMode_Default",
            string.Empty
        };

        int differenceCount = 0;
        foreach (var item in panel.Items)
        {
            var model = item.Model;
            foreach (int lineIndex in model.DataLineIndices)
            {
                if (lineIndex < 0 || lineIndex >= model.Lines.Count)
                    continue;

                string line = model.Lines[lineIndex];
                if (!AlarmCsvLine.TryParse(line, model.HeaderColumnCount, out var parsedLine, out string errorMessage))
                {
                    throw new InvalidOperationException(
                        $"Alarm CSV export 실패: {model.FilePath} ({lineIndex + 1}행){Environment.NewLine}{errorMessage}");
                }

                string runValue = parsedLine.GetField(AlarmCsvLine.StopModeRunColumnIndex).Trim();
                string defaultValue = parsedLine.GetField(AlarmCsvLine.StopModeDefaultColumnIndex).Trim();
                if (runValue == defaultValue)
                    continue;

                differenceCount++;
                string alarmId = parsedLine.GetField(0).Trim();
                reportLines.Add(
                    $"{differenceCount}. File={model.FileName}, AlarmID={alarmId}, Line={lineIndex + 1}, StopMode_Run={runValue}, StopMode_Default={defaultValue}");
            }
        }

        if (differenceCount == 0)
            reportLines.Add("차이 없음");

        reportLines.Insert(2, $"DifferenceCount: {differenceCount}");

        Directory.CreateDirectory(alarmDir);
        string exportPath = GetUniqueExportPath(alarmDir, exportedAt);
        File.WriteAllText(exportPath, string.Join(Environment.NewLine, reportLines), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        return new AlarmStopModeDiffExportResult(exportPath, differenceCount);
    }

    private static string GetUniqueExportPath(string alarmDir, DateTime exportedAt)
    {
        string baseName = exportedAt.ToString("yyyyMMdd_HHmmss");
        string path = Path.Combine(alarmDir, $"{baseName}.txt");
        if (!File.Exists(path))
            return path;

        for (int suffix = 1; ; suffix++)
        {
            path = Path.Combine(alarmDir, $"{baseName}_{suffix}.txt");
            if (!File.Exists(path))
                return path;
        }
    }
}
