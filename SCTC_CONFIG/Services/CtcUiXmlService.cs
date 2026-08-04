using SCTC_CONFIG.Models;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace SCTC_CONFIG.Services;

public sealed class CtcUiFileState
{
    public List<XmlMainEntryModel> Entries { get; set; } = new();
    public string FilePath { get; set; } = string.Empty;
    public string RawText { get; set; } = string.Empty;
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
}

public static class CtcUiXmlService
{
    public const string FileName = "CTC_UI.xml";
    public const string TargetElementName = "EnableUIDebugMode";

    private static readonly Regex TargetElementRegex = new(
        $@"(<\s*{TargetElementName}\s*>\s*)(true|false)(\s*<\s*/\s*{TargetElementName}\s*>)",
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    public static CtcUiFileState Load(string rootPath)
    {
        string filePath = Path.Combine(rootPath, FileName);
        if (!File.Exists(filePath))
            return new CtcUiFileState();

        using var reader = new StreamReader(filePath, detectEncodingFromByteOrderMarks: true);
        string rawText = reader.ReadToEnd();
        Encoding encoding = reader.CurrentEncoding;

        var state = new CtcUiFileState
        {
            FilePath = filePath,
            RawText = rawText,
            FileEncoding = encoding
        };

        var match = TargetElementRegex.Match(rawText);
        if (match.Success)
        {
            state.Entries.Add(new XmlMainEntryModel
            {
                Key = TargetElementName,
                Value = match.Groups[2].Value.ToLowerInvariant()
            });
        }

        return state;
    }

    public static bool TryReplaceValue(string rawText, bool isEnabled, out string updatedText)
    {
        updatedText = rawText;

        var match = TargetElementRegex.Match(rawText);
        if (!match.Success)
            return false;

        string newValue = isEnabled ? "true" : "false";
        if (match.Groups[2].Value == newValue)
            return false;

        updatedText = TargetElementRegex.Replace(
            rawText,
            matchResult => matchResult.Groups[1].Value + newValue + matchResult.Groups[3].Value,
            count: 1);

        return true;
    }
}
