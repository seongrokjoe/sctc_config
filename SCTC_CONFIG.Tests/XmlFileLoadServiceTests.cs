using SCTC_CONFIG.Services;
using System.Text;

namespace SCTC_CONFIG.Tests;

public sealed class XmlFileLoadServiceTests : IDisposable
{
    private readonly string _tempRoot = Path.Combine(Path.GetTempPath(), $"SCTC_CONFIG_XML_{Guid.NewGuid():N}");

    public XmlFileLoadServiceTests()
    {
        Directory.CreateDirectory(_tempRoot);
    }

    [Fact]
    public void Load_RemovesInvalidXmlControlCharacters_FromMainXml()
    {
        string xml = string.Join('\n',
        [
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>",
            "<SctcConfig>",
            "  <UseStandaloneMode>true</UseStandaloneMode>",
            $"  <ImagePath>ImageAXIS.ico{(char)0x1A}</ImagePath>",
            "</SctcConfig>"
        ]) + "\n";
        const string scdXml = """
<?xml version="1.0" encoding="utf-8"?>
<SctcScd>
  <SimulationSCD>true</SimulationSCD>
  <SimulationSCDforEC>false</SimulationSCDforEC>
  <SimulationFunction>true</SimulationFunction>
  <SimulationDriver>true</SimulationDriver>
</SctcScd>
""";

        File.WriteAllText(Path.Combine(_tempRoot, "SCTC.xml"), xml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.WriteAllText(Path.Combine(_tempRoot, "SCD.xml"), scdXml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var service = new XmlFileLoadService();
        XmlLoadResult result = service.Load(_tempRoot);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.NotNull(result.MainDocument);
        Assert.Equal("ImageAXIS.ico", result.MainDocument!.Descendants("ImagePath").Single().Value);
        Assert.Contains(result.MainEntries, entry => entry.Key == "UseStandaloneMode" && entry.Value == "true");
    }

    [Fact]
    public void Load_FailsWithFileAndLocation_WhenXmlRemainsMalformed()
    {
        const string mainXml = """
<?xml version="1.0" encoding="utf-8"?>
<SctcConfig>
  <UseStandaloneMode>true</UseStandaloneMode>
</SctcConfig>
""";
        const string scdXml = """
<?xml version="1.0" encoding="utf-8"?>
<SctcScd>
  <SimulationSCD>true</SimulationSCD>
</SctcScd>
""";
        const string invalidDriverXml = """
<?xml version="1.0" encoding="utf-8"?>
<Root>
  <DriverList>
    <Driver>
      <Name>Axis</Name>
  </DriverList>
</Root>
""";

        string driverDir = Path.Combine(_tempRoot, "Driver");
        Directory.CreateDirectory(driverDir);

        File.WriteAllText(Path.Combine(_tempRoot, "SCTC.xml"), mainXml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.WriteAllText(Path.Combine(_tempRoot, "SCD.xml"), scdXml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.WriteAllText(Path.Combine(driverDir, "Driver.xml"), invalidDriverXml, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        var service = new XmlFileLoadService();
        XmlLoadResult result = service.Load(_tempRoot);

        Assert.False(result.Success);
        Assert.NotNull(result.ErrorMessage);
        Assert.Contains("Driver.xml", result.ErrorMessage);
        Assert.Contains("\uB77C\uC778", result.ErrorMessage);
        Assert.Contains("\uC704\uCE58", result.ErrorMessage);
    }

    public void Dispose()
    {
        if (Directory.Exists(_tempRoot))
            Directory.Delete(_tempRoot, recursive: true);
    }
}
