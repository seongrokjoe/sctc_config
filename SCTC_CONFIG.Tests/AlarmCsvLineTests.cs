using SCTC_CONFIG.Helpers;
using SCTC_CONFIG.Models;
using SCTC_CONFIG.Services;
using SCTC_CONFIG.ViewModels;
using System.Text;

namespace SCTC_CONFIG.Tests;

public sealed class AlarmCsvLineTests : IDisposable
{
    private readonly string _tempRoot = Path.Combine(Path.GetTempPath(), $"SCTC_CONFIG_{Guid.NewGuid():N}");

    public AlarmCsvLineTests()
    {
        Directory.CreateDirectory(_tempRoot);
    }

    [Fact]
    public void TryParse_PreservesDescriptionWithQuotesAndComma()
    {
        const string line = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,EMGStop:1,EMGStop:1,EMGStop:1";

        bool success = AlarmCsvLine.TryParse(line, headerColumnCount: 10, out var parsedLine, out var errorMessage);

        Assert.True(success, errorMessage);
        Assert.Equal("abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc", parsedLine.GetField(AlarmCsvLine.DescriptionColumnIndex));
        Assert.Equal("EMGStop:1", parsedLine.GetField(AlarmCsvLine.DisplayOnlyActionColumnIndex));
        Assert.Equal("EMGStop:1", parsedLine.GetField(AlarmCsvLine.DisplayOnlyRecoveryColumnIndex));
        Assert.Equal("EMGStop:1", parsedLine.GetField(AlarmCsvLine.DisplayOnlyNoteColumnIndex));
    }

    [Fact]
    public void Save_PreservesUntouchedAlarmColumns_WhenDisplayOnlyIsApplied()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note,Enabled";
        const string before = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,EMGStop:1,EMGStop:1,EMGStop:1,TRUE";
        const string expected = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,DisplayOnly:1,DisplayOnly:1,DisplayOnly:1,TRUE";

        string alarmPath = Path.Combine(alarmDir, "AlarmCase.csv");
        File.WriteAllText(alarmPath, string.Join("\r\n", header, before), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var csvTab = new CsvTabViewModel();
        var model = new AlarmFileModel
        {
            FilePath = alarmPath,
            FileName = "AlarmCase.csv",
            Lines = new List<string> { header, before },
            FileEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            LineEnding = "\r\n",
            HeaderColumnCount = 11,
            DataLineIndices = new List<int> { 1 },
            InitialIsDisplayOnly = false
        };

        csvTab.AlarmPanel.Items.Add(new AlarmFileItemViewModel(model)
        {
            IsDisplayOnly = true
        });

        var service = new FileSaveService();
        service.Save(_tempRoot, null, csvTab);

        string saved = File.ReadAllText(alarmPath);
        Assert.Equal(string.Join("\r\n", header, expected), saved);
    }

    [Fact]
    public void Load_ComputesDisplayOnlyState_ForVariableColumnCountAlarmRows()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note,Enabled";
        const string data = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,DisplayOnly:1,DisplayOnly:1,DisplayOnly:1,TRUE";
        File.WriteAllText(Path.Combine(alarmDir, "AlarmCase.csv"), string.Join("\r\n", header, data), Encoding.UTF8);

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Single(result.AlarmFiles);
        Assert.True(result.AlarmFiles[0].InitialIsDisplayOnly);
        Assert.Equal(11, result.AlarmFiles[0].HeaderColumnCount);
    }

    [Fact]
    public void Load_FailsWhenAlarmRowDoesNotReachDisplayOnlyColumns()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note";
        const string invalidData = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,EMGStop:1";
        File.WriteAllText(Path.Combine(alarmDir, "AlarmCase.csv"), string.Join("\r\n", header, invalidData), Encoding.UTF8);

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.False(result.Success);
        Assert.Contains("Alarm CSV 로드 실패", result.ErrorMessage);
        Assert.Contains("AlarmCase.csv", result.ErrorMessage);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
