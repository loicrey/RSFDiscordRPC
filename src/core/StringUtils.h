#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace rsf::strings
{
    inline std::string Slugify(std::string_view value)
    {
        std::string slug;
        slug.reserve(value.size());

        for (unsigned char ch : value)
        {
            if (std::isalnum(ch))
            {
                slug.push_back(static_cast<char>(std::tolower(ch)));
            }
            else if ((std::isspace(ch) || ch == '-' || ch == '_' || ch == '.') && !slug.empty() && slug.back() != '_')
            {
                slug.push_back('_');
            }
        }

        while (!slug.empty() && slug.back() == '_')
        {
            slug.pop_back();
        }

        return slug;
    }

    inline std::string Limit(std::string value, std::size_t maxLen)
    {
        if (value.size() > maxLen)
        {
            value.resize(maxLen);
        }
        return value;
    }
}
