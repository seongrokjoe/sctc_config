using SCTC_CONFIG.Models;
using System.IO;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.Services;

public class XmlLoadResult
{
    public bool Success { get; set; }
    public string? ErrorMessage { get; set; }

    // Main (SCTC.xml)
    public List<XmlMainEntryModel> MainEntries { get; set; } = new();
    public XDocument? MainDocument { get; set; }
    public string MainFilePath { get; set; } = string.Empty;
    public Encoding MainEncoding { get; set; } = Encoding.UTF8;

    // Driver (Driver/Driver.xml)
    public List<XmlDriverRowModel> DriverRows { get; set; } = new();
    public XDocument? DriverDocument { get; set; }
    public string DriverFilePath { get; set; } = string.Empty;
    public Encoding DriverEncoding { get; set; } = Encoding.UTF8;

    // Function
    public List<XmlFunctionFileModel> FunctionFiles { get; set; } = new();

    // Alarm
    public List<XmlAlarmFileModel> AlarmFiles { get; set; } = new();
}

public class XmlFileLoadService
{
    private static readonly string[] TargetMainKeys = { "UseStandaloneMode", "UseVirtualDriver" };

    public XmlLoadResult Load(string rootPath)
    {
        var result = new XmlLoadResult();

        string sctcPath = Path.Combine(rootPath, "SCTC.xml");
        if (!File.Exists(sctcPath))
        {
            result.Success = false;
            result.ErrorMessage = $"SCTC.xml 파일을 찾을 수 없습니다:\n{sctcPath}";
            return result;
        }

        LoadMainXml(sctcPath, result);

        string driverPath = Path.Combine(rootPath, "Driver", "Driver.xml");
        if (File.Exists(driverPath))
            LoadDriverXml(driverPath, result);

        string functionDir = Path.Combine(rootPath, "Function");
        if (Directory.Exists(functionDir))
            LoadFunctionFiles(functionDir, result);

        string alarmDir = Path.Combine(rootPath, "Alarm");
        if (Directory.Exists(alarmDir))
            LoadAlarmFiles(alarmDir, result);

        result.Success = true;
        return result;
    }

    private static (XDocument doc, Encoding encoding) ParseXml(string filePath)
    {
        Encoding encoding;
        string rawText;
        using (var reader = new StreamReader(filePath, detectEncodingFromByteOrderMarks: true))
        {
            rawText = reader.ReadToEnd();
            encoding = reader.CurrentEncoding;
        }
        var doc = XDocument.Parse(rawText, LoadOptions.PreserveWhitespace);
        return (doc, encoding);
    }

    private void LoadMainXml(string filePath, XmlLoadResult result)
    {
        var (doc, encoding) = ParseXml(filePath);
        result.MainDocument = doc;
        result.MainFilePath = filePath;
        result.MainEncoding = encoding;

        foreach (var key in TargetMainKeys)
        {
            var element = doc.Descendants(key).FirstOrDefault();
            if (element != null)
            {
                result.MainEntries.Add(new XmlMainEntryModel
                {
                    Key = key,
                    Value = element.Value.Trim().ToLower()
                });
            }
        }
    }

    private void LoadDriverXml(string filePath, XmlLoadResult result)
    {
        var (doc, encoding) = ParseXml(filePath);
        result.DriverDocument = doc;
        result.DriverFilePath = filePath;
        result.DriverEncoding = encoding;

        var driverElements = doc.Descendants("Driver")
            .Where(e => e.Parent?.Name.LocalName == "DriverList")
            .ToList();

        for (int i = 0; i < driverElements.Count; i++)
        {
            var el = driverElements[i];
            var model = new XmlDriverRowModel { Index = i };

            model.Name = el.Element("Name")?.Value ?? string.Empty;
            model.FileName = el.Element("FileName")?.Value ?? string.Empty;

            var needLoadEl = el.Element("NeedLoad");
            model.HasNeedLoad = needLoadEl != null;
            model.OriginalNeedLoad = needLoadEl?.Value.Trim().Equals("true", StringComparison.OrdinalIgnoreCase) ?? false;

            var arg1El = el.Element("Arg1");
            model.HasArg1 = arg1El != null;
            model.OriginalArg1 = arg1El?.Value ?? string.Empty;

            var arg2El = el.Element("Arg2");
            model.HasArg2 = arg2El != null;
            model.OriginalArg2 = arg2El?.Value ?? string.Empty;

            var arg3El = el.Element("Arg3");
            model.HasArg3 = arg3El != null;
            model.OriginalArg3 = arg3El?.Value ?? string.Empty;

            var arg4El = el.Element("Arg4");
            model.HasArg4 = arg4El != null;
            model.OriginalArg4 = arg4El?.Value ?? string.Empty;

            var arg5El = el.Element("Arg5");
            model.HasArg5 = arg5El != null;
            model.OriginalArg5 = arg5El?.Value ?? string.Empty;

            result.DriverRows.Add(model);
        }
    }

    private void LoadFunctionFiles(string functionDir, XmlLoadResult result)
    {
        var xmlFiles = Directory.GetFiles(functionDir, "*.xml")
            .OrderBy(Path.GetFileName)
            .ToList();

        foreach (var xmlFile in xmlFiles)
        {
            var (doc, encoding) = ParseXml(xmlFile);

            var isSimElements = doc.Descendants("IsSimulation").ToList();
            var distinctValues = isSimElements
                .Select(e => e.Value.Trim().Equals("true", StringComparison.OrdinalIgnoreCase))
                .Distinct()
                .ToList();

            var model = new XmlFunctionFileModel
            {
                FilePath = xmlFile,
                FileName = Path.GetFileName(xmlFile),
                Document = doc,
                FileEncoding = encoding,
            };

            if (distinctValues.Count == 0)
            {
                model.InitialIsSimulation = false;
                model.IsInconsistent = false;
            }
            else if (distinctValues.Count == 1)
            {
                model.InitialIsSimulation = distinctValues[0];
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

    private void LoadAlarmFiles(string alarmDir, XmlLoadResult result)
    {
        var xmlFiles = Directory.GetFiles(alarmDir, "*.xml")
            .OrderBy(Path.GetFileName)
            .ToList();

        foreach (var xmlFile in xmlFiles)
        {
            var (doc, encoding) = ParseXml(xmlFile);

            result.AlarmFiles.Add(new XmlAlarmFileModel
            {
                FilePath = xmlFile,
                FileName = Path.GetFileName(xmlFile),
                Document = doc,
                FileEncoding = encoding,
            });
        }
    }
}
