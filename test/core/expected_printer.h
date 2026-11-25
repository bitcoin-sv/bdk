#pragma once

// How tl::expected results print when a check fails.

#include <ostream>
#include <string>
#include <type_traits>

#include <tl/expected.hpp>

// In bsv::detail, where Boost.Test finds them by argument-dependent lookup on
// the error type.
namespace bsv::detail
{
    template<typename T, typename E>
    std::ostream& operator<<(std::ostream& os, const tl::expected<T, E>& result)
    {
        if(!result)
            return os << result.error();

        if constexpr(std::is_same_v<T, std::string>)
        {
            os << '\'' << *result << '\'';
        }
        else
        {
            os << *result;
        }
        return os;
    }

    template<typename E>
    std::ostream& operator<<(std::ostream& os, const tl::unexpected<E>& error)
    {
        // Through expected, as tl::unexpected names its error value() and
        // std::unexpected error().
        return os << tl::expected<void, E>{error}.error();
    }
}
