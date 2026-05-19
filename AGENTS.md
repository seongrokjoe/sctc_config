# Repository Guidelines

## Project Structure & Module Organization
This repository contains a .NET 8 WPF desktop app for SCTC configuration editing.

- `SCTC_CONFIG.sln`: solution entry point.
- `SCTC_CONFIG/`: main application project (`net8.0-windows`).
- `SCTC_CONFIG/Models`: file/domain models for CSV and XML config data.
- `SCTC_CONFIG/ViewModels`: MVVM state and commands (tab/panel orchestration).
- `SCTC_CONFIG/Views`: XAML UI and code-behind.
- `SCTC_CONFIG/Services`: file I/O, folder selection, load/save behavior.
- `SCTC_CONFIG/Converters`, `SCTC_CONFIG/Helpers`: UI converters and utility parsing logic.
- `dist/`: publish output (single-file executable).

## Build, Test, and Development Commands
- `dotnet restore SCTC_CONFIG.sln`: restore NuGet packages.
- `dotnet build SCTC_CONFIG.sln`: compile Debug build for local development.
- `dotnet publish SCTC_CONFIG/SCTC_CONFIG.csproj -c Release -r win-x64 --no-self-contained -p:PublishSingleFile=true -o dist`: create release executable.
- `publish.bat`: Windows wrapper for the release publish command above.

No automated test project is currently configured; validate key flows manually after changes (load/edit/save for both CSV and XML tabs).

## Coding Style & Naming Conventions
- Follow `.editorconfig`: UTF-8 BOM, CRLF, spaces, 4-space indentation, final newline, trim trailing whitespace.
- Keep C# nullable reference types enabled and prefer explicit null handling.
- Use MVVM patterns from `CommunityToolkit.Mvvm` (`[ObservableProperty]`, `[RelayCommand]`).
- Naming:
  - `*ViewModel`, `*View`, `*Service`, `*Model`, `*Converter`.
  - Use PascalCase for public members and file names.

## Testing Guidelines
- Manual regression is required for now:
  - CSV mode: `General.csv`, `Driver/Driver.csv`, `Function/*.csv`, `Alarm/*.csv`.
  - XML mode: `SCTC.xml`, `Driver/Driver.xml`, `Function/*.xml`, `Alarm/*.xml`.
- Verify encoding/line-ending preservation and backup-folder creation behavior before save.

## Commit & Pull Request Guidelines
- Use concise, imperative commit subjects (observed pattern: `Fix ...`, `Add ...`).
- Keep one logical change per commit.
- PRs should include:
  - Purpose and scope summary.
  - User-visible behavior changes (screenshots for UI updates).
  - Manual verification steps performed (CSV/XML scenarios).
  - Linked issue/task if available.
