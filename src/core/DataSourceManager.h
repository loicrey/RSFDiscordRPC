#pragma once

#include "../sources/IDataSource.h"

#include <memory>
#include <utility>
#include <vector>

namespace rsf
{
    class DataSourceManager
    {
    public:
        void add(std::unique_ptr<IDataSource> source)
        {
            sources_.push_back(std::move(source));
        }

        template <typename T, typename... TArgs>
        T& addSource(TArgs&&... args)
        {
            auto source = std::make_unique<T>(std::forward<TArgs>(args)...);
            T& reference = *source;
            add(std::move(source));
            return reference;
        }

        bool update(GameData& data)
        {
            bool updated = false;

            for (const auto& source : sources_)
            {
                if (source->isAvailable() && source->update(data))
                {
                    updated = true;
                }
            }

            return updated;
        }

    private:
        std::vector<std::unique_ptr<IDataSource>> sources_;
    };
}
