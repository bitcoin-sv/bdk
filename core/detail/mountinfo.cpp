#include <detail/mountinfo.hpp>

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

using namespace bsv::detail;

std::ostream& bsv::detail::operator<<(std::ostream& os, const MountinfoError error)
{
    switch(error)
    {
    case MountinfoError::MalformedLine:
        return os << "MalformedLine";
    case MountinfoError::InvalidEscape:
        return os << "InvalidEscape";
    }
    return os << "MountinfoError " << static_cast<int>(error);
}

std::ostream& bsv::detail::operator<<(std::ostream& os, const MountFields& fields)
{
    return os
        << '\''
        << fields.root
        << "' '"
        << fields.mountPoint
        << "' '"
        << fields.fsType
        << "' '"
        << fields.rest
        << '\'';
}

tl::expected<MountFields, MountinfoError> bsv::detail::SplitMountLine(
    std::string_view line)
{
    // The field before the next space, which the line then starts after.
    const auto next = [&line]() -> std::optional<std::string_view>
    {
        const size_t space{line.find(' ')};
        if(space == std::string_view::npos)
            return std::nullopt;

        const std::string_view field{line.substr(0, space)};
        line.remove_prefix(space + 1);
        return field;
    };
    if(!next() || !next() || !next()) // ID, parent ID, major:minor
        return tl::unexpected{MountinfoError::MalformedLine};

    const auto root{next()};
    if(!root || !root->starts_with('/'))
        return tl::unexpected{MountinfoError::MalformedLine};

    const auto mountPoint{next()};
    if(!mountPoint)
        return tl::unexpected{MountinfoError::MalformedLine};

    // Fields hold no spaces, so the first " - " is the lone "-" that ends the
    // optional fields.
    constexpr std::string_view SEPARATOR{" - "};
    const size_t separator{line.find(SEPARATOR)};
    if(separator == std::string_view::npos)
        return tl::unexpected{MountinfoError::MalformedLine};

    line.remove_prefix(separator + SEPARATOR.size());
    const auto fsType{next()};
    if(!fsType)
        return tl::unexpected{MountinfoError::MalformedLine};

    return MountFields{.root = *root,
                       .mountPoint = *mountPoint,
                       .fsType = *fsType,
                       .rest = line};
}

tl::expected<std::string, MountinfoError> bsv::detail::UnescapeMountPath(
    const std::string_view path)
{
    constexpr int MAX_ESCAPED_BYTE{0xFF};
    constexpr size_t ESCAPE_DIGITS{3};
    std::string out;
    for(size_t i = 0; i < path.size(); ++i)
    {
        if(path[i] != '\\')
        {
            out.push_back(path[i]);
            continue;
        }
        const std::string_view digits{path.substr(i + 1, ESCAPE_DIGITS)};
        if(digits.size() < ESCAPE_DIGITS)
            return tl::unexpected{MountinfoError::InvalidEscape};

        int value{};
        for(const char c : digits)
        {
            if(c < '0' || c > '7')
                return tl::unexpected{MountinfoError::InvalidEscape};

            value = (value * 8) + (c - '0');
        }
        if(value > MAX_ESCAPED_BYTE)
            return tl::unexpected{MountinfoError::InvalidEscape};

        out.push_back(static_cast<char>(value));
        i += ESCAPE_DIGITS;
    }
    return out;
}
