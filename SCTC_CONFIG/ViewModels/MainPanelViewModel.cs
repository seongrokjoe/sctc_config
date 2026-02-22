using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;
using System.Text;

namespace SCTC_CONFIG.ViewModels;

public partial class MainEntryItemViewModel : ObservableObject
{
    public string Key { get; }
    public int LineIndex { get; }

    [ObservableProperty]
    private bool _isEnabled;

    public MainEntryItemViewModel(MainEntryModel model)
    {
        Key = model.Key;
        LineIndex = model.LineIndex;
        _isEnabled = model.Value.Equals("TRUE", StringComparison.OrdinalIgnoreCase);
    }
}

public class MainPanelViewModel : ObservableObject
{
    public ObservableCollection<MainEntryItemViewModel> Items { get; } = new();

    public List<string> Lines { get; set; } = new();
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
    public string LineEnding { get; set; } = "\r\n";
}
