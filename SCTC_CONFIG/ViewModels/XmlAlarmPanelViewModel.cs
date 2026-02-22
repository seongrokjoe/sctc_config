using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;

namespace SCTC_CONFIG.ViewModels;

public partial class XmlAlarmFileItemViewModel : ObservableObject
{
    public XmlAlarmFileModel Model { get; }

    public string FileName => Model.FileName;

    [ObservableProperty]
    private bool _isDisplayOnly;

    public XmlAlarmFileItemViewModel(XmlAlarmFileModel model)
    {
        Model = model;
        _isDisplayOnly = model.InitialIsDisplayOnly;
    }
}

public class XmlAlarmPanelViewModel : ObservableObject
{
    public ObservableCollection<XmlAlarmFileItemViewModel> Items { get; } = new();
}
