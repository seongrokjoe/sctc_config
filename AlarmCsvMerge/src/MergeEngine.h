#pragma once

#include "WinIo.h"

namespace alarm
{
    struct Comparison
    {
        Snapshot oldSnapshot;
        Snapshot newSnapshot;
        std::map<std::wstring, PairResult, PathLess> csvResults;
        std::wstring report;
        std::wstring summary;
        bool canGenerate = false;
        size_t excludedFiles = 0;
    };

    Comparison CompareFolders(const std::wstring& oldPath, const std::wstring& newPath);
    struct GenerationResult
    {
        std::wstring directory;
        std::wstring summary;
        size_t changedFiles = 0;
        size_t changedIds = 0;
        size_t changedCells = 0;
        size_t whitespaceCells = 0;
    };
    GenerationResult Generate(const Comparison& comparison, const std::wstring& outputParent,
        const std::wstring& reportPath);
}
