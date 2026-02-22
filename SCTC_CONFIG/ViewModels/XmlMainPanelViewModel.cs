using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.ViewModels;

public partial class XmlMainEntryItemViewModel : ObservableObject
{
    public string Key { get; }

    [ObservableProperty]
    private bool _isEnabled;

    public XmlMainEntryItemViewModel(XmlMainEntryModel model)
    {
        Key = model.Key;
        _isEnabled = model.Value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }
}

public class XmlMainPanelViewModel : ObservableObject
{
    public ObservableCollection<XmlMainEntryItemViewModel> Items { get; } = new();
    public XDocument? Document { get; set; }
    public string FilePath { get; set; } = string.Empty;
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
}
