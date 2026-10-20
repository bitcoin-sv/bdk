#include <memory>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#include <cancellation.hpp>

namespace
{

// A relaxed atomic load of the caller's flag. The flag is plain int32_t memory that
// another thread stores to atomically, so it is read with the compiler's atomic load
// rather than through std::atomic_ref, which needs a non-const object and libc++ 19.
int32_t LoadRelaxed(const int32_t* flag)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return __iso_volatile_load32(reinterpret_cast<const volatile __int32*>(flag));
#else
    return __atomic_load_n(flag, __ATOMIC_RELAXED);
#endif
}

// A cancellation source whose state lives in caller memory.
class CFlagCancellationSource final : public task::CCancellationSource
{
public:
    explicit CFlagCancellationSource(const int32_t* flag) : flag_{ flag } {}

    static std::unique_ptr<CFlagCancellationSource> Make(const int32_t* flag)
    {
        return std::make_unique<CFlagCancellationSource>(flag);
    }

    // Nothing outside this file can reach the source to call Cancel() on it, so the
    // flag is the only state to read.
    bool IsCanceled() override
    {
        return flag_ != nullptr && LoadRelaxed(flag_) != 0;
    }

private:
    const int32_t* flag_;
};

} // namespace

task::CCancellationToken bsv::MakeFlagCancellationToken(const int32_t* flag)
{
    // A token holds its sources by shared_ptr and GetToken() calls shared_from_this(),
    // so hand ownership to a shared_ptr first.
    std::shared_ptr<task::CCancellationSource> source = CFlagCancellationSource::Make(flag);
    return source->GetToken();
}
