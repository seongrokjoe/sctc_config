using SCTC_CONFIG.Models;
using System.IO;
using System.Text;
using System.Xml;
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

    public List<XmlMainEntryModel> ScdEntries { get; set; } = new();
    public XDocument? ScdDocument { get; set; }
    public string ScdFilePath { get; set; } = string.Empty;
    public Encoding ScdEncoding { get; set; } = Encoding.UTF8;

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
    private static readonly string[] TargetScdKeys = { "SimulationSCD", "SimulationSCDforEC", "SimulationFunction", "SimulationDriver" };

    public XmlLoadResult Load(string rootPath)
    {
        var result = new XmlLoadResult();

        string sctcPath = Path.Combine(rootPath, "SCTC.xml");
        if (!File.Exists(sctcPath))
        {
            result.Success = false;
            result.ErrorMessage = $"SCTC.xml \uD30C\uC77C\uC744 \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4:\n{sctcPath}";
            return result;
        }

        string scdPath = Path.Combine(rootPath, "SCD.xml");
        if (!File.Exists(scdPath))
        {
            result.Success = false;
            result.ErrorMessage = $"SCD.xml \uD30C\uC77C\uC744 \uCC3E\uC744 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4:\n{scdPath}";
            return result;
        }

        try
        {
            LoadMainXml(sctcPath, result);
            LoadScdXml(scdPath, result);

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
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = $"XML \uB85C\uB4DC \uC911 \uC624\uB958\uAC00 \uBC1C\uC0DD\uD588\uC2B5\uB2C8\uB2E4:\n{ex.Message}";
        }

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

        string sanitizedText = RemoveInvalidXmlCharacters(rawText);

        try
        {
            var doc = XDocument.Parse(sanitizedText, LoadOptions.PreserveWhitespace);
            return (doc, encoding);
        }
        catch (XmlException ex)
        {
            throw new InvalidDataException(BuildXmlParseErrorMessage(filePath, ex), ex);
        }
    }

    private static string RemoveInvalidXmlCharacters(string text)
    {
        StringBuilder? sanitized = null;

        for (int i = 0; i < text.Length; i++)
        {
            char ch = text[i];

            if (char.IsHighSurrogate(ch))
            {
                if (i + 1 < text.Length && char.IsLowSurrogate(text[i + 1]))
                {
                    if (sanitized != null)
                    {
                        sanitized.Append(ch);
                        sanitized.Append(text[i + 1]);
                    }

                    i++;
                    continue;
                }

                sanitized = EnsureSanitizedBuilder(sanitized, text, i);
                continue;
            }

            if (char.IsLowSurrogate(ch))
            {
                sanitized = EnsureSanitizedBuilder(sanitized, text, i);
                continue;
            }

            if (IsValidXmlCodePoint(ch))
            {
                if (sanitized != null)
                    sanitized.Append(ch);

                continue;
            }

            sanitized = EnsureSanitizedBuilder(sanitized, text, i);
        }

        return sanitized?.ToString() ?? text;
    }

    private static StringBuilder EnsureSanitizedBuilder(StringBuilder? builder, string originalText, int validPrefixLength)
    {
        if (builder != null)
            return builder;

        builder = new StringBuilder(originalText.Length);
        builder.Append(originalText, 0, validPrefixLength);
        return builder;
    }

    private static bool IsValidXmlCodePoint(int codePoint)
    {
        return codePoint == 0x09 ||
               codePoint == 0x0A ||
               codePoint == 0x0D ||
               (codePoint >= 0x20 && codePoint <= 0xD7FF) ||
               (codePoint >= 0xE000 && codePoint <= 0xFFFD) ||
               (codePoint >= 0x10000 && codePoint <= 0x10FFFF);
    }

    private static string BuildXmlParseErrorMessage(string filePath, XmlException ex)
    {
        if (ex.LineNumber > 0 || ex.LinePosition > 0)
        {
            return $"XML \uD30C\uC77C\uC744 \uD30C\uC2F1\uD560 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4:\n{filePath}\n\uB77C\uC778 {ex.LineNumber}, \uC704\uCE58 {ex.LinePosition}\n{ex.Message}";
        }

        return $"XML \uD30C\uC77C\uC744 \uD30C\uC2F1\uD560 \uC218 \uC5C6\uC2B5\uB2C8\uB2E4:\n{filePath}\n{ex.Message}";
    }

    private void LoadMainXml(string filePath, XmlLoadResult result)
    {
        var (doc, encoding) = ParseXml(filePath);
        result.MainDocument = doc;
        result.MainFilePath = filePath;
        result.MainEncoding = encoding;
        result.MainEntries = ExtractToggleEntries(doc, TargetMainKeys);
    }

    private void LoadScdXml(string filePath, XmlLoadResult result)
    {
        var (doc, encoding) = ParseXml(filePath);
        result.ScdDocument = doc;
        result.ScdFilePath = filePath;
        result.ScdEncoding = encoding;
        result.ScdEntries = ExtractToggleEntries(doc, TargetScdKeys);
    }

    private static List<XmlMainEntryModel> ExtractToggleEntries(XDocument doc, IEnumerable<string> keys)
    {
        var entries = new List<XmlMainEntryModel>();

        foreach (var key in keys)
        {
            var element = doc.Descendants(key).FirstOrDefault();
            if (element == null)
                continue;

            entries.Add(new XmlMainEntryModel
            {
                Key = key,
                Value = element.Value.Trim().ToLower()
            });
        }

        return entries;
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

            var stopRunValues = doc.Descendants("StopMode_Run").Select(e => e.Value.Trim()).ToList();
            var stopSetupValues = doc.Descendants("StopMode_Setup").Select(e => e.Value.Trim()).ToList();
            var stopDefaultValues = doc.Descendants("StopMode_Default").Select(e => e.Value.Trim()).ToList();

            bool isDisplayOnly = stopRunValues.Count > 0 &&
                stopRunValues.All(v => v == "DisplayOnly:1") &&
                stopSetupValues.All(v => v == "DisplayOnly:1") &&
                stopDefaultValues.All(v => v == "DisplayOnly:1");

            result.AlarmFiles.Add(new XmlAlarmFileModel
            {
                FilePath = xmlFile,
                FileName = Path.GetFileName(xmlFile),
                Document = doc,
                FileEncoding = encoding,
                InitialIsDisplayOnly = isDisplayOnly,
            });
        }
    }
}
