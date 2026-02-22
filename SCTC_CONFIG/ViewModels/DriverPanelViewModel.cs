using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;
using System.Text;

namespace SCTC_CONFIG.ViewModels;

public partial class DriverRowItemViewModel : ObservableObject
{
    private readonly DriverRowModel _model;

    public string Name => _model.Name;
    public string FileName => _model.FileName;
    public int LineIndex => _model.LineIndex;
    public string[] OriginalColumns => _model.OriginalColumns;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private bool _needLoad;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private string _arg1 = string.Empty;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private string _arg2 = string.Empty;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private string _arg3 = string.Empty;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private string _arg4 = string.Empty;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsModified))]
    private string _arg5 = string.Empty;

    public bool IsModified =>
        NeedLoad != _model.NeedLoad ||
        Arg1 != _model.Arg1 ||
        Arg2 != _model.Arg2 ||
        Arg3 != _model.Arg3 ||
        Arg4 != _model.Arg4 ||
        Arg5 != _model.Arg5;

    public DriverRowItemViewModel(DriverRowModel model)
    {
        _model = model;
        _needLoad = model.NeedLoad;
        _arg1 = model.Arg1;
        _arg2 = model.Arg2;
        _arg3 = model.Arg3;
        _arg4 = model.Arg4;
        _arg5 = model.Arg5;
    }
}

public class DriverPanelViewModel : ObservableObject
{
    public ObservableCollection<DriverRowItemViewModel> Items { get; } = new();

    public List<string> Lines { get; set; } = new();
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
    public string LineEnding { get; set; } = "\r\n";
}
