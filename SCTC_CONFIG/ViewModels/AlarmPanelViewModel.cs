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
        _isDisplayOnly = model.InitialIsDisplayOnly;
    }
}

public partial class AlarmPanelViewModel : ObservableObject
{
    [ObservableProperty]
    private bool _applyRun = true;

    [ObservableProperty]
    private bool _applySetup = true;

    [ObservableProperty]
    private bool _applyDefault = true;

    public ObservableCollection<AlarmFileItemViewModel> Items { get; } = new();
}
