using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;

namespace SCTC_CONFIG.ViewModels;

public partial class CtcUiEntryItemViewModel : ObservableObject
{
    public string Key { get; }

    [ObservableProperty]
    private bool _isEnabled;

    public CtcUiEntryItemViewModel(XmlMainEntryModel model)
    {
        Key = model.Key;
        _isEnabled = model.Value.Equals("true", StringComparison.OrdinalIgnoreCase);
    }
}
