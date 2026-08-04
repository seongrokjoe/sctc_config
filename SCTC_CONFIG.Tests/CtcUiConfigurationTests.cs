using SCTC_CONFIG.Models;
using SCTC_CONFIG.Services;
using SCTC_CONFIG.ViewModels;
using System.Text;

namespace SCTC_CONFIG.Tests;

public sealed class CtcUiConfigurationTests : IDisposable
{
    private static readonly Encoding Utf8NoBom = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false);
    private readonly string _tempRoot = Path.Combine(Path.GetTempPath(), $"SCTC_CONFIG_CTC_UI_{Guid.NewGuid():N}");

    public CtcUiConfigurationTests()
    {
        Directory.CreateDirectory(_tempRoot);
    }

    [Fact]
    public void CsvLoad_LoadsOptionalCtcUiToggle()
    {
        WriteRequiredCsvRoot();
        WriteCtcUiXml("""
<?xml version="1.0" encoding="utf-8"?>
<UI>
  <General>
    <OtherSetting>keep</OtherSetting>
    <EnableUIDebugMode>false</EnableUIDebugMode>
  </General>
</UI>
""");

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        var entry = Assert.Single(result.CtcUi.Entries);
        Assert.Equal(CtcUiXmlService.TargetElementName, entry.Key);
        Assert.Equal("false", entry.Value);
        Assert.Equal(Path.Combine(_tempRoot, CtcUiXmlService.FileName), result.CtcUi.FilePath);
        Assert.Contains("<OtherSetting>keep</OtherSetting>", result.CtcUi.RawText);
    }

    [Fact]
    public void XmlLoad_LoadsOptionalCtcUiToggle()
    {
        WriteRequiredXmlRoot();
        WriteCtcUiXml("""
<?xml version="1.0" encoding="utf-8"?>
<UI>
  <General>
    <EnableUIDebugMode>true</EnableUIDebugMode>
  </General>
</UI>
""");

        var service = new XmlFileLoadService();
        XmlLoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        var entry = Assert.Single(result.CtcUi.Entries);
        Assert.Equal(CtcUiXmlService.TargetElementName, entry.Key);
        Assert.Equal("true", entry.Value);
        Assert.Equal(Path.Combine(_tempRoot, CtcUiXmlService.FileName), result.CtcUi.FilePath);
    }

    [Fact]
    public void CsvLoad_SucceedsWhenCtcUiXmlIsMissing()
    {
        WriteRequiredCsvRoot();

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Empty(result.CtcUi.Entries);
        Assert.Equal(string.Empty, result.CtcUi.FilePath);
    }

    [Fact]
    public void XmlLoad_SucceedsWhenCtcUiXmlDoesNotContainTarget()
    {
        WriteRequiredXmlRoot();
        WriteCtcUiXml("""
<UI>
  <General>
    <OtherSetting>keep</OtherSetting>
  </General>
</UI>
""");

        var service = new XmlFileLoadService();
        XmlLoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Empty(result.CtcUi.Entries);
        Assert.Equal(Path.Combine(_tempRoot, CtcUiXmlService.FileName), result.CtcUi.FilePath);
        Assert.Contains("<OtherSetting>keep</OtherSetting>", result.CtcUi.RawText);
    }

    [Fact]
    public void CsvSave_UpdatesOnlyCtcUiToggleText()
    {
        string before = string.Join("\r\n",
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>",
            "<UI>",
            "  <General>",
            "    <OtherSetting>keep</OtherSetting>",
            "    <EnableUIDebugMode>false</EnableUIDebugMode>",
            "  </General>",
            "</UI>");
        string expectedTrue = before.Replace(
            "<EnableUIDebugMode>false</EnableUIDebugMode>",
            "<EnableUIDebugMode>true</EnableUIDebugMode>");
        string ctcUiPath = WriteCtcUiXml(before);

        var csvTab = new CsvTabViewModel();
        csvTab.MainPanel.CtcUiFilePath = ctcUiPath;
        csvTab.MainPanel.CtcUiRawText = before;
        csvTab.MainPanel.CtcUiFileEncoding = Utf8NoBom;
        csvTab.MainPanel.UiItems.Add(new CtcUiEntryItemViewModel(new XmlMainEntryModel
        {
            Key = CtcUiXmlService.TargetElementName,
            Value = "false"
        })
        {
            IsEnabled = true
        });

        var service = new FileSaveService();
        service.Save(_tempRoot, null, csvTab);

        Assert.Equal(expectedTrue, File.ReadAllText(ctcUiPath, Utf8NoBom));
        Assert.Equal(expectedTrue, csvTab.MainPanel.CtcUiRawText);

        csvTab.MainPanel.UiItems[0].IsEnabled = false;
        service.Save(_tempRoot, null, csvTab);

        Assert.Equal(before, File.ReadAllText(ctcUiPath, Utf8NoBom));
        Assert.Equal(before, csvTab.MainPanel.CtcUiRawText);
    }

    [Fact]
    public void XmlSave_UpdatesOnlyCtcUiToggleText()
    {
        string before = string.Join("\r\n",
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>",
            "<UI>",
            "  <General>",
            "    <OtherSetting>keep</OtherSetting>",
            "    <EnableUIDebugMode>true</EnableUIDebugMode>",
            "  </General>",
            "</UI>");
        string expectedFalse = before.Replace(
            "<EnableUIDebugMode>true</EnableUIDebugMode>",
            "<EnableUIDebugMode>false</EnableUIDebugMode>");
        string ctcUiPath = WriteCtcUiXml(before);

        var xmlTab = new XmlTabViewModel();
        xmlTab.MainPanel.CtcUiFilePath = ctcUiPath;
        xmlTab.MainPanel.CtcUiRawText = before;
        xmlTab.MainPanel.CtcUiFileEncoding = Utf8NoBom;
        xmlTab.MainPanel.UiItems.Add(new CtcUiEntryItemViewModel(new XmlMainEntryModel
        {
            Key = CtcUiXmlService.TargetElementName,
            Value = "true"
        })
        {
            IsEnabled = false
        });

        var service = new XmlFileSaveService();
        service.Save(_tempRoot, xmlTab);

        Assert.Equal(expectedFalse, File.ReadAllText(ctcUiPath, Utf8NoBom));
        Assert.Equal(expectedFalse, xmlTab.MainPanel.CtcUiRawText);
        Assert.Contains("<OtherSetting>keep</OtherSetting>", File.ReadAllText(ctcUiPath, Utf8NoBom));
    }

    private void WriteRequiredCsvRoot()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Utf8NoBom);

        string scdDir = Path.Combine(_tempRoot, "SCD");
        Directory.CreateDirectory(scdDir);
        File.WriteAllText(Path.Combine(scdDir, "SCDGeneral.csv"), "SimulationSCD,true", Utf8NoBom);
    }

    private void WriteRequiredXmlRoot()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "SCTC.xml"), """
<?xml version="1.0" encoding="utf-8"?>
<SctcConfig>
  <UseStandaloneMode>true</UseStandaloneMode>
</SctcConfig>
""", Utf8NoBom);

        File.WriteAllText(Path.Combine(_tempRoot, "SCD.xml"), """
<?xml version="1.0" encoding="utf-8"?>
<SctcScd>
  <SimulationSCD>true</SimulationSCD>
</SctcScd>
""", Utf8NoBom);
    }

    private string WriteCtcUiXml(string text)
    {
        string path = Path.Combine(_tempRoot, CtcUiXmlService.FileName);
        File.WriteAllText(path, text, Utf8NoBom);
        return path;
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
