using SCTC_CONFIG.Models;
using SCTC_CONFIG.Services;
using SCTC_CONFIG.ViewModels;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.Tests;

public sealed class XmlAlarmStopModeScopeTests : IDisposable
{
    private readonly string _tempRoot = Path.Combine(Path.GetTempPath(), $"SCTC_CONFIG_XML_ALARM_{Guid.NewGuid():N}");

    public XmlAlarmStopModeScopeTests()
    {
        Directory.CreateDirectory(_tempRoot);
    }

    [Fact]
    public void Save_UpdatesOnlySelectedStopModeElements_WhenDisplayOnlyIsApplied()
    {
        string alarmDir = Path.Combine(_tempRoot, "Alarm");
        Directory.CreateDirectory(alarmDir);

        string alarmPath = Path.Combine(alarmDir, "AlarmCase.xml");
        var document = XDocument.Parse("""
<Root>
  <Alarm>
    <StopMode_Run>RunStop:1</StopMode_Run>
    <StopMode_Setup>SetupStop:1</StopMode_Setup>
    <StopMode_Default>DefaultStop:1</StopMode_Default>
  </Alarm>
</Root>
""", LoadOptions.PreserveWhitespace);

        File.WriteAllText(alarmPath, document.ToString(SaveOptions.DisableFormatting), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var xmlTab = new XmlTabViewModel();
        xmlTab.AlarmPanel.ApplyRun = false;
        xmlTab.AlarmPanel.ApplyDefault = false;
        xmlTab.AlarmPanel.Items.Add(new XmlAlarmFileItemViewModel(new XmlAlarmFileModel
        {
            FilePath = alarmPath,
            FileName = "AlarmCase.xml",
            Document = document,
            FileEncoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            InitialIsDisplayOnly = false
        })
        {
            IsDisplayOnly = true
        });

        var service = new XmlFileSaveService();
        service.Save(_tempRoot, xmlTab);

        var saved = XDocument.Load(alarmPath, LoadOptions.PreserveWhitespace);
        Assert.Equal("RunStop:1", saved.Descendants("StopMode_Run").Single().Value);
        Assert.Equal("DisplayOnly:1", saved.Descendants("StopMode_Setup").Single().Value);
        Assert.Equal("DefaultStop:1", saved.Descendants("StopMode_Default").Single().Value);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
