#pragma once

#include "../core/GameData.h"

namespace rsf
{
    class IDataSource
    {
    public:
        virtual ~IDataSource() = default;

        virtual const char* name() const = 0;
        virtual bool isAvailable() = 0;
        virtual bool update(GameData& data) = 0;
    };
}
