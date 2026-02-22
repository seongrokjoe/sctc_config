using SCTC_CONFIG.ViewModels;
using System.IO;
using System.Text;
using System.Xml.Linq;

namespace SCTC_CONFIG.Services;

public class XmlFileSaveService
{
    public void Save(string rootPath, XmlTabViewModel xmlTab)
    {
        SaveMainXml(xmlTab.MainPanel);
        SaveDriverXml(xmlTab.DriverPanel);
        SaveFunctionFiles(xmlTab.FunctionPanel);
        SaveAlarmFiles(xmlTab.AlarmPanel);
    }

    private static void SaveMainXml(XmlMainPanelViewModel panel)
    {
        if (panel.Document == null) return;

        bool anyChanged = false;
        foreach (var item in panel.Items)
        {
            string newValue = item.IsEnabled ? "true" : "false";
            var element = panel.Document.Descendants(item.Key).FirstOrDefault();
            if (element != null && element.Value != newValue)
            {
                element.Value = newValue;
                anyChanged = true;
            }
        }

        if (anyChanged)
            WriteXDocument(panel.Document, panel.FilePath, panel.FileEncoding);
    }

    private static void SaveDriverXml(XmlDriverPanelViewModel panel)
    {
        if (panel.Document == null) return;

        var driverElements = panel.Document.Descendants("Driver")
            .Where(e => e.Parent?.Name.LocalName == "DriverList")
            .ToList();

        bool anyChanged = false;
        foreach (var item in panel.Items)
        {
            if (!item.IsModified) continue;
            if (item.Index < 0 || item.Index >= driverElements.Count) continue;

            var driverEl = driverElements[item.Index];

            if (item.HasNeedLoad)
            {
                var el = driverEl.Element("NeedLoad");
                if (el != null && el.Value != (item.NeedLoad ? "true" : "false"))
                { el.Value = item.NeedLoad ? "true" : "false"; anyChanged = true; }
            }
            if (item.HasArg1) { var el = driverEl.Element("Arg1"); if (el != null && el.Value != item.Arg1) { el.Value = item.Arg1; anyChanged = true; } }
            if (item.HasArg2) { var el = driverEl.Element("Arg2"); if (el != null && el.Value != item.Arg2) { el.Value = item.Arg2; anyChanged = true; } }
            if (item.HasArg3) { var el = driverEl.Element("Arg3"); if (el != null && el.Value != item.Arg3) { el.Value = item.Arg3; anyChanged = true; } }
            if (item.HasArg4) { var el = driverEl.Element("Arg4"); if (el != null && el.Value != item.Arg4) { el.Value = item.Arg4; anyChanged = true; } }
            if (item.HasArg5) { var el = driverEl.Element("Arg5"); if (el != null && el.Value != item.Arg5) { el.Value = item.Arg5; anyChanged = true; } }
        }

        if (anyChanged)
            WriteXDocument(panel.Document, panel.FilePath, panel.FileEncoding);
    }

    private static void SaveFunctionFiles(XmlFunctionPanelViewModel panel)
    {
        foreach (var item in panel.Items)
        {
            if (!item.HasBeenToggled) continue;

            string newValue = item.IsSimulation ? "true" : "false";
            foreach (var el in item.Model.Document.Descendants("IsSimulation").ToList())
                el.Value = newValue;

            WriteXDocument(item.Model.Document, item.Model.FilePath, item.Model.FileEncoding);
        }
    }

    private static void SaveAlarmFiles(XmlAlarmPanelViewModel panel)
    {
        foreach (var item in panel.Items)
        {
            if (!item.IsDisplayOnly) continue;

            var doc = item.Model.Document;
            foreach (var el in doc.Descendants("StopMode_Run").ToList())
                el.Value = "DisplayOnly:1";
            foreach (var el in doc.Descendants("StopMode_Setup").ToList())
                el.Value = "DisplayOnly:1";
            foreach (var el in doc.Descendants("StopMode_Default").ToList())
                el.Value = "DisplayOnly:1";

            WriteXDocument(doc, item.Model.FilePath, item.Model.FileEncoding);
        }
    }

    private static void WriteXDocument(XDocument doc, string filePath, Encoding encoding)
    {
        var sb = new StringBuilder();
        if (doc.Declaration != null)
            sb.Append(doc.Declaration.ToString());
        sb.Append(doc.ToString(SaveOptions.DisableFormatting));
        File.WriteAllText(filePath, sb.ToString(), encoding);
    }
}
