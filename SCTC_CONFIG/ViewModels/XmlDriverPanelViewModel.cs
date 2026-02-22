using CommunityToolkit.Mvvm.ComponentModel;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.ViewModels;

public partial class XmlDriverRowItemViewModel : ObservableObject
{
    private readonly XmlDriverRowModel _model;

    public string Name => _model.Name;
    public string FileName => _model.FileName;
    public int Index => _model.Index;

    public bool HasNeedLoad => _model.HasNeedLoad;
    public bool HasArg1 => _model.HasArg1;
    public bool HasArg2 => _model.HasArg2;
    public bool HasArg3 => _model.HasArg3;
    public bool HasArg4 => _model.HasArg4;
    public bool HasArg5 => _model.HasArg5;

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
        (HasNeedLoad && NeedLoad != _model.OriginalNeedLoad) ||
        (HasArg1 && Arg1 != _model.OriginalArg1) ||
        (HasArg2 && Arg2 != _model.OriginalArg2) ||
        (HasArg3 && Arg3 != _model.OriginalArg3) ||
        (HasArg4 && Arg4 != _model.OriginalArg4) ||
        (HasArg5 && Arg5 != _model.OriginalArg5);

    public XmlDriverRowItemViewModel(XmlDriverRowModel model)
    {
        _model = model;
        _needLoad = model.OriginalNeedLoad;
        _arg1 = model.OriginalArg1;
        _arg2 = model.OriginalArg2;
        _arg3 = model.OriginalArg3;
        _arg4 = model.OriginalArg4;
        _arg5 = model.OriginalArg5;
    }
}

public class XmlDriverPanelViewModel : ObservableObject
{
    public ObservableCollection<XmlDriverRowItemViewModel> Items { get; } = new();
    public XDocument? Document { get; set; }
    public string FilePath { get; set; } = string.Empty;
    public Encoding FileEncoding { get; set; } = Encoding.UTF8;
}
