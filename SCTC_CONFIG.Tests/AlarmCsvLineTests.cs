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
    public void Save_UpdatesOnlySelectedStopModeColumns_WhenDisplayOnlyIsApplied()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note,Enabled";
        const string before = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,Description,EMGStop:1,SetupStop:1,DefaultStop:1,TRUE";
        const string expected = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,Description,DisplayOnly:1,SetupStop:1,DefaultStop:1,TRUE";

        string alarmPath = Path.Combine(alarmDir, "AlarmCase.csv");
        File.WriteAllText(alarmPath, string.Join("\r\n", header, before), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var csvTab = new CsvTabViewModel();
        csvTab.AlarmPanel.ApplySetup = false;
        csvTab.AlarmPanel.ApplyDefault = false;

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
        WriteRequiredScdCsv();

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
    public void Load_SplitsAlarmCsvRows_WithMixedLineEndings()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);
        WriteRequiredScdCsv();

        string header = string.Join(",", Enumerable.Range(1, 19).Select(i => $"Column{i}"));
        string displayOnlyData = string.Join(",",
            "A001",
            "OverTemperature",
            "CRITICAL",
            "Hardware",
            "Sensor",
            "TempSensor",
            "Description",
            "DisplayOnly:1",
            "DisplayOnly:1",
            "DisplayOnly:1",
            "TRUE",
            "C12",
            "C13",
            "C14",
            "C15",
            "C16",
            "C17",
            "C18",
            "C19");

        string alarmText = header + "\n" + displayOnlyData + "\r\n" + displayOnlyData;
        File.WriteAllText(Path.Combine(alarmDir, "AlarmCase.csv"), alarmText, Encoding.UTF8);

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Single(result.AlarmFiles);
        Assert.Equal(2, result.AlarmFiles[0].DataLineIndices.Count);
        Assert.Equal(19, result.AlarmFiles[0].HeaderColumnCount);
        Assert.True(result.AlarmFiles[0].InitialIsDisplayOnly);
    }

    [Fact]
    public void Load_FailsWhenAlarmRowDoesNotReachDisplayOnlyColumns()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);
        WriteRequiredScdCsv();

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note";
        const string invalidData = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc,EMGStop:1";
        File.WriteAllText(Path.Combine(alarmDir, "AlarmCase.csv"), string.Join("\r\n", header, invalidData), Encoding.UTF8);

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.False(result.Success);
        Assert.Contains("Alarm CSV 로드 실패", result.ErrorMessage);
        Assert.Contains("AlarmCase.csv", result.ErrorMessage);
    }

    [Fact]
    public void Export_WritesOnlyRunDefaultDifferences_FromAllAlarmFiles()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note";
        const string sameRow = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,Description,DisplayOnly:1,SetupStop:1,DisplayOnly:1";
        const string differentRow = "A002,LowPressure,WARNING,Process,Valve,PressureValve,Description,RunStop:1,SetupStop:1,DefaultStop:1";
        const string uncheckedDifferentRow = "B001,VisionTimeout,WARNING,Vision,VisionDrv,Camera1,Description,RunStop:2,SetupStop:1,DefaultStop:2";

        var panel = new AlarmPanelViewModel();
        panel.ApplyRun = false;
        panel.ApplyDefault = false;
        panel.Items.Add(new AlarmFileItemViewModel(new AlarmFileModel
        {
            FilePath = Path.Combine(alarmDir, "AlarmA.csv"),
            FileName = "AlarmA.csv",
            Lines = new List<string> { header, sameRow, differentRow },
            HeaderColumnCount = 10,
            DataLineIndices = new List<int> { 1, 2 },
            InitialIsDisplayOnly = true
        })
        {
            IsDisplayOnly = true
        });
        panel.Items.Add(new AlarmFileItemViewModel(new AlarmFileModel
        {
            FilePath = Path.Combine(alarmDir, "AlarmB.csv"),
            FileName = "AlarmB.csv",
            Lines = new List<string> { header, uncheckedDifferentRow },
            HeaderColumnCount = 10,
            DataLineIndices = new List<int> { 1 },
            InitialIsDisplayOnly = false
        })
        {
            IsDisplayOnly = false
        });

        var service = new AlarmStopModeDiffExportService();
        var result = service.Export(panel, new DateTime(2026, 8, 4, 12, 34, 56));

        string report = File.ReadAllText(result.FilePath, Encoding.UTF8);
        Assert.Equal(2, result.DifferenceCount);
        Assert.Equal(Path.Combine(alarmDir, "20260804_123456.txt"), result.FilePath);
        Assert.Contains("DifferenceCount: 2", report);
        Assert.Contains("File=AlarmA.csv, AlarmID=A002, Line=3, StopMode_Run=RunStop:1, StopMode_Default=DefaultStop:1", report);
        Assert.Contains("File=AlarmB.csv, AlarmID=B001, Line=2, StopMode_Run=RunStop:2, StopMode_Default=DefaultStop:2", report);
        Assert.DoesNotContain("A001", report);
    }

    [Fact]
    public void Export_WritesNoDifferencesMessage_WhenRunAndDefaultAreSame()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        const string header = "AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note";
        const string sameRow = "A001,OverTemperature,CRITICAL,Hardware,Sensor,TempSensor,Description,DisplayOnly:1,SetupStop:1,DisplayOnly:1";

        var panel = new AlarmPanelViewModel();
        panel.Items.Add(new AlarmFileItemViewModel(new AlarmFileModel
        {
            FilePath = Path.Combine(alarmDir, "AlarmA.csv"),
            FileName = "AlarmA.csv",
            Lines = new List<string> { header, sameRow },
            HeaderColumnCount = 10,
            DataLineIndices = new List<int> { 1 },
            InitialIsDisplayOnly = true
        }));

        var service = new AlarmStopModeDiffExportService();
        var result = service.Export(panel, new DateTime(2026, 8, 4, 12, 34, 56));

        string report = File.ReadAllText(result.FilePath, Encoding.UTF8);
        Assert.Equal(0, result.DifferenceCount);
        Assert.Contains("DifferenceCount: 0", report);
        Assert.Contains("차이 없음", report);
    }

    private void WriteRequiredScdCsv()
    {
        string scdDir = Path.Combine(_tempRoot, "SCD");
        Directory.CreateDirectory(scdDir);
        File.WriteAllText(
            Path.Combine(scdDir, "SCDGeneral.csv"),
            string.Join("\r\n",
                "SimulationSCD,true",
                "SimulationFunction,true",
                "SimulationDriver,true"),
            Encoding.UTF8);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
