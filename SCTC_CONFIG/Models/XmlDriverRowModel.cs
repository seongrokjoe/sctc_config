namespace SCTC_CONFIG.Models;

public class XmlDriverRowModel
{
    public int Index { get; set; }
    public string Name { get; set; } = string.Empty;
    public string FileName { get; set; } = string.Empty;

    public bool HasNeedLoad { get; set; }
    public bool OriginalNeedLoad { get; set; }

    public bool HasArg1 { get; set; }
    public string OriginalArg1 { get; set; } = string.Empty;

    public bool HasArg2 { get; set; }
    public string OriginalArg2 { get; set; } = string.Empty;

    public bool HasArg3 { get; set; }
    public string OriginalArg3 { get; set; } = string.Empty;

    public bool HasArg4 { get; set; }
    public string OriginalArg4 { get; set; } = string.Empty;

    public bool HasArg5 { get; set; }
    public string OriginalArg5 { get; set; } = string.Empty;
}
