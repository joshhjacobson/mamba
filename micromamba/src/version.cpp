// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "version.hpp"


namespace umamba
{
    std::string version()
    {
        return UMAMBA_VERSION_STRING;
    }

    std::array<int, 3> version_arr()
    {
        return { UMAMBA_VERSION_MAJOR, UMAMBA_VERSION_MINOR, UMAMBA_VERSION_PATCH };
    }
}
