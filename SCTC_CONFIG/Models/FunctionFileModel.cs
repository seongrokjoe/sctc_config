using System.Text;

namespace SCTC_CONFIG.Models;

public class FunctionFileModel
{
    public string FilePath { get; set; } = string.Empty;
    public string FileName { get; set; } = string.Empty;
    public List<string> Lines { get; set; } = new();
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
    public string LineEnding { get; set; } = "\r\n";
    public List<int> DataLineIndices { get; set; } = new();
    public bool InitialIsSimulation { get; set; }
    public bool IsInconsistent { get; set; }
}
