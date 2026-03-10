using SCTC_CONFIG.Services;
using SCTC_CONFIG.ViewModels;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.Tests;

public sealed class ScdConfigurationTests : IDisposable
{
    private readonly string _tempRoot = Path.Combine(Path.GetTempPath(), $"SCTC_CONFIG_SCD_{Guid.NewGuid():N}");

    public ScdConfigurationTests()
    {
        Directory.CreateDirectory(_tempRoot);
    }

    [Fact]
    public void CsvLoad_FailsWhenScdGeneralIsMissing()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);

        var service = new FileLoadService();
        LoadResult result = service.Load(_tempRoot);

        Assert.False(result.Success);
        Assert.Contains("SCDGeneral.csv", result.ErrorMessage);
    }

    [Fact]
    public void CsvSave_SkipsRewriteWhenScdValuesAreUnchanged()
    {
        string scdPath = WriteCsvRootWithScd();

        var loadService = new FileLoadService();
        LoadResult result = loadService.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(3, result.ScdEntries.Count);

        var csvTab = new CsvTabViewModel();
        csvTab.MainPanel.ScdLines = result.ScdLines;
        csvTab.MainPanel.ScdFileEncoding = result.ScdEncoding;
        csvTab.MainPanel.ScdLineEnding = result.ScdLineEnding;
        foreach (var entry in result.ScdEntries)
            csvTab.MainPanel.ScdItems.Add(new MainEntryItemViewModel(entry));

        var baseline = new DateTime(2020, 1, 2, 3, 4, 5, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(scdPath, baseline);

        var saveService = new FileSaveService();
        saveService.Save(_tempRoot, null, csvTab);

        Assert.Equal(baseline, File.GetLastWriteTimeUtc(scdPath));
    }

    [Fact]
    public void CsvSave_UpdatesTrackedRowsAndPreservesOtherContent()
    {
        string scdPath = WriteCsvRootWithScd();

        var loadService = new FileLoadService();
        LoadResult result = loadService.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);

        var csvTab = new CsvTabViewModel();
        csvTab.MainPanel.ScdLines = result.ScdLines;
        csvTab.MainPanel.ScdFileEncoding = result.ScdEncoding;
        csvTab.MainPanel.ScdLineEnding = result.ScdLineEnding;
        foreach (var entry in result.ScdEntries)
            csvTab.MainPanel.ScdItems.Add(new MainEntryItemViewModel(entry));

        csvTab.MainPanel.ScdItems.Single(item => item.Key == "SimulationFunction").IsEnabled = false;

        var saveService = new FileSaveService();
        saveService.Save(_tempRoot, null, csvTab);

        string saved = File.ReadAllText(scdPath);
        Assert.Contains("SimulationSCD,true", saved);
        Assert.Contains("SimulationFunction,false", saved);
        Assert.Contains("SimulationDriver,true", saved);
        Assert.Contains("# comment", saved);
        Assert.Contains("IgnoredKey,true", saved);
    }

    [Fact]
    public void XmlLoad_FailsWhenScdXmlIsMissing()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "SCTC.xml"), """
<?xml version="1.0" encoding="utf-8"?>
<SctcConfig>
  <UseStandaloneMode>true</UseStandaloneMode>
</SctcConfig>
""", new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var service = new XmlFileLoadService();
        XmlLoadResult result = service.Load(_tempRoot);

        Assert.False(result.Success);
        Assert.Contains("SCD.xml", result.ErrorMessage);
    }

    [Fact]
    public void XmlSave_SkipsRewriteWhenScdValuesAreUnchanged()
    {
        string scdPath = WriteXmlRootWithScd();

        var loadService = new XmlFileLoadService();
        XmlLoadResult result = loadService.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(4, result.ScdEntries.Count);

        var xmlTab = new XmlTabViewModel();
        xmlTab.MainPanel.ScdDocument = result.ScdDocument;
        xmlTab.MainPanel.ScdFilePath = result.ScdFilePath;
        xmlTab.MainPanel.ScdFileEncoding = result.ScdEncoding;
        foreach (var entry in result.ScdEntries)
            xmlTab.MainPanel.ScdItems.Add(new XmlMainEntryItemViewModel(entry));

        var baseline = new DateTime(2020, 2, 3, 4, 5, 6, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(scdPath, baseline);

        var saveService = new XmlFileSaveService();
        saveService.Save(_tempRoot, xmlTab);

        Assert.Equal(baseline, File.GetLastWriteTimeUtc(scdPath));
    }

    [Fact]
    public void XmlSave_UpdatesTrackedElementsAndPreservesOtherData()
    {
        string scdPath = WriteXmlRootWithScd();

        var loadService = new XmlFileLoadService();
        XmlLoadResult result = loadService.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);

        var xmlTab = new XmlTabViewModel();
        xmlTab.MainPanel.ScdDocument = result.ScdDocument;
        xmlTab.MainPanel.ScdFilePath = result.ScdFilePath;
        xmlTab.MainPanel.ScdFileEncoding = result.ScdEncoding;
        foreach (var entry in result.ScdEntries)
            xmlTab.MainPanel.ScdItems.Add(new XmlMainEntryItemViewModel(entry));

        xmlTab.MainPanel.ScdItems.Single(item => item.Key == "SimulationSCDforEC").IsEnabled = true;

        var saveService = new XmlFileSaveService();
        saveService.Save(_tempRoot, xmlTab);

        var saved = XDocument.Load(scdPath, LoadOptions.PreserveWhitespace);
        Assert.Equal("true", saved.Descendants("SimulationSCD").Single().Value);
        Assert.Equal("true", saved.Descendants("SimulationSCDforEC").Single().Value);
        Assert.Equal("true", saved.Descendants("SimulationFunction").Single().Value);
        Assert.Equal("true", saved.Descendants("SimulationDriver").Single().Value);
        Assert.Equal("keep", saved.Descendants("ExtraSetting").Single().Value);
    }

    private string WriteCsvRootWithScd()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "General.csv"), "UseStandaloneMode,TRUE", Encoding.UTF8);

        string scdDir = Path.Combine(_tempRoot, "SCD");
        Directory.CreateDirectory(scdDir);

        string scdPath = Path.Combine(scdDir, "SCDGeneral.csv");
        File.WriteAllText(
            scdPath,
            string.Join("\r\n",
                "# comment",
                "SimulationSCD,true",
                "IgnoredKey,true",
                "SimulationFunction,true",
                "SimulationDriver,true"),
            new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        return scdPath;
    }

    private string WriteXmlRootWithScd()
    {
        File.WriteAllText(Path.Combine(_tempRoot, "SCTC.xml"), """
<?xml version="1.0" encoding="utf-8"?>
<SctcConfig>
  <UseStandaloneMode>true</UseStandaloneMode>
</SctcConfig>
""", new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        string scdPath = Path.Combine(_tempRoot, "SCD.xml");
        File.WriteAllText(scdPath, """
<?xml version="1.0" encoding="utf-8"?>
<SctcScd>
  <SimulationSCD>true</SimulationSCD>
  <SimulationSCDforEC>false</SimulationSCDforEC>
  <SimulationFunction>true</SimulationFunction>
  <SimulationDriver>true</SimulationDriver>
  <ExtraSetting>keep</ExtraSetting>
</SctcScd>
""", new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        return scdPath;
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
