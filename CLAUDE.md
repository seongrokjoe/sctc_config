# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Debug build
dotnet build SCTC_CONFIG/SCTC_CONFIG.csproj

# Release single-file exe (outputs to dist/)
dotnet publish SCTC_CONFIG/SCTC_CONFIG.csproj -c Release -r win-x64 --no-self-contained -p:PublishSingleFile=true -o dist

# Convenience wrapper (Windows)
publish.bat
```

There are no automated tests in this project.

## Architecture Overview

This is a C# WPF (.NET 8.0) desktop tool for editing SCTC configuration files (CSV and XML formats). It uses **CommunityToolkit.Mvvm** (MVVM source generation via `[ObservableProperty]`, `[RelayCommand]`).

### Two Config Format Modes (Tabs)

`MainViewModel` is the root VM. It owns two independent tab VMs:
- **CsvTabViewModel** — edits CSV-format configs
- **XmlTabViewModel** — edits XML-format configs

The active tab is selected via `SelectedTabIndex`. Load/Save operate only on the active tab.

### Panel Structure (same shape for both CSV and XML)

Each tab VM composes four panel VMs:

| Panel | Target File(s) | Editable Fields |
|---|---|---|
| **MainPanel** | `General.csv` / `SCTC.xml` | 4 boolean keys (UseStandaloneMode, UseVirtualDriver, UseVirtualInterlock, UseFunctionSimulationMode) |
| **DriverPanel** | `Driver/Driver.csv` / `Driver/Driver.xml` | NeedLoad (bool), Arg1–Arg5 (string) per row |
| **FunctionPanel** | `Function/*.csv` / `Function/*.xml` | IsSimulation (bool) per file; files named `*Def*` or `*IOInfo*` are excluded (CSV only) |
| **AlarmPanel** | `Alarm/*.csv` / `Alarm/*.xml` | DisplayOnly flag — sets cols[7,8,9] to `"DisplayOnly:1"` (CSV) or `StopMode_Run/Setup/Default` elements (XML) |

### Data Flow

```
FolderBrowserService → user picks a root folder
FileLoadService / XmlFileLoadService → LoadResult / XmlLoadResult
  → MainViewModel.LoadCsv() / LoadXml() → populates panel VMs
  → user edits in Views
FileSaveService / XmlFileSaveService → reads panel VM state → writes files
```

### File Integrity Rules

- Read with `StreamReader(path, detectEncodingFromByteOrderMarks: true)` to auto-detect encoding.
- Detect line ending from raw text: `rawText.Contains("\r\n") ? "\r\n" : "\n"`.
- Reconstruct by splitting on detected line ending, modifying only target lines, and rejoining with the same ending.
- Write with `File.WriteAllText(path, joined, detectedEncoding)` to preserve the original encoding (including BOM).

### Key WPF/MVVM Patterns

- All ViewModels extend `ObservableObject` or use `[ObservableProperty]`/`[RelayCommand]` source generation.
- `_wpftmp.csproj` (XAML compiler temp project) does **not** inherit implicit usings — always add explicit `using System.IO;` (and other needed namespaces) to any service or viewmodel file.
- `TextBlock` has no `BorderBrush`/`BorderThickness` — wrap it in `<Border>` instead.
- Toggle buttons in views (e.g., `FunctionPanelView`) use `Mode=OneWay` and set `e.Handled = true` in the Click handler to delegate control to a ViewModel command rather than letting the button auto-toggle.
- `SaveDialogWindow` is a modal dialog; `SaveDialogViewModel.DialogResult` carries the user's choice back to `MainViewModel.Save()`.
- Before saving, a backup of the entire root folder is made (unless the user opts out) by copying recursively to a sibling folder named `{folderName}_{yyyyMMddHHmm}`.
