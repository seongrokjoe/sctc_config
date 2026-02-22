namespace SCTC_CONFIG.Models;

public class DriverRowModel
{
    public string OriginalLine { get; set; } = string.Empty;
    public string[] OriginalColumns { get; set; } = Array.Empty<string>();
    public int LineIndex { get; set; }

    public string Name => OriginalColumns.Length > 0 ? OriginalColumns[0] : string.Empty;
    public string FileName => OriginalColumns.Length > 1 ? OriginalColumns[1] : string.Empty;
    public bool NeedLoad => OriginalColumns.Length > 2 && OriginalColumns[2].Trim().Equals("TRUE", StringComparison.OrdinalIgnoreCase);
    public string Arg1 => OriginalColumns.Length > 7 ? OriginalColumns[7] : string.Empty;
    public string Arg2 => OriginalColumns.Length > 8 ? OriginalColumns[8] : string.Empty;
    public string Arg3 => OriginalColumns.Length > 9 ? OriginalColumns[9] : string.Empty;
    public string Arg4 => OriginalColumns.Length > 10 ? OriginalColumns[10] : string.Empty;
    public string Arg5 => OriginalColumns.Length > 11 ? OriginalColumns[11] : string.Empty;
}
