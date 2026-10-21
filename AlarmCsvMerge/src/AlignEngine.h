#pragma once

#include "WinIo.h"

namespace alarm
{
    struct AlignmentResult
    {
        std::wstring directory;
        std::wstring summary;
        size_t csvFiles = 0;
        size_t sortedFiles = 0;
        size_t unchangedFiles = 0;
        size_t excludedFiles = 0;
        size_t excludedRows = 0;
    };

    AlignmentResult AlignFolder(const std::wstring& inputPath, const std::wstring& outputParent);
}
