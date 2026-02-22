using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.Models;

public class XmlAlarmFileModel
{
    public string FilePath { get; set; } = string.Empty;
    public string FileName { get; set; } = string.Empty;
    public XDocument Document { get; set; } = null!;
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
    public bool InitialIsDisplayOnly { get; set; }
}
