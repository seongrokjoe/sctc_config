using CommunityToolkit.Mvvm.ComponentModel;

namespace SCTC_CONFIG.ViewModels;

public class XmlTabViewModel : ObservableObject
{
    public XmlMainPanelViewModel MainPanel { get; } = new();
    public XmlDriverPanelViewModel DriverPanel { get; } = new();
    public XmlFunctionPanelViewModel FunctionPanel { get; } = new();
    public XmlAlarmPanelViewModel AlarmPanel { get; } = new();
}
