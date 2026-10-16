#include <atomic>
#include <memory>

#include <cancellation.hpp>

namespace
{

// A cancellation source whose state lives in caller memory.
class CFlagCancellationSource final : public task::CCancellationSource
{
public:
    static std::shared_ptr<task::CCancellationSource> Make(const int32_t* flag)
    {
        return std::shared_ptr<task::CCancellationSource>{ new CFlagCancellationSource{ flag } };
    }

    // Nothing outside this file can reach the source to call Cancel() on it, so the
    // flag is the only state to read.
    bool IsCanceled() override
    {
        // atomic_ref needs a non-const object; it is only ever loaded here.
        return flag_ != nullptr
            && std::atomic_ref<int32_t>(*const_cast<int32_t*>(flag_)).load(std::memory_order_relaxed) != 0;
    }

private:
    explicit CFlagCancellationSource(const int32_t* flag) : flag_{ flag } {}

    const int32_t* const flag_;
};

} // namespace

task::CCancellationToken bsv::MakeFlagCancellationToken(const int32_t* flag)
{
    return CFlagCancellationSource::Make(flag)->GetToken();
}
