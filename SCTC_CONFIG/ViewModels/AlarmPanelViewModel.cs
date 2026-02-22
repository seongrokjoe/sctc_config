using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;

namespace SCTC_CONFIG.ViewModels;

public partial class AlarmFileItemViewModel : ObservableObject
{
    public AlarmFileModel Model { get; }

    public string FileName => Model.FileName;

    [ObservableProperty]
    private bool _isDisplayOnly;

    public AlarmFileItemViewModel(AlarmFileModel model)
    {
        Model = model;
        _isDisplayOnly = false; // always start unchecked
    }
}

public class AlarmPanelViewModel : ObservableObject
{
    public ObservableCollection<AlarmFileItemViewModel> Items { get; } = new();
}
