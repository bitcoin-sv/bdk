#pragma once

#include <cstdint>

#include <taskcancellation.h>

namespace bsv
{

    /**
     * MakeFlagCancellationToken returns a token for the cancellable CTxValidator calls
     * that reports cancelled once *flag is non-zero.
     *
     * The flag belongs to the caller and must be 4-byte aligned. Any thread may set it at
     * any time with an atomic store; the token only reads it, with relaxed atomic loads.
     * It must stay valid for as long as the token or a call using it is alive. A null
     * flag gives a token that is never cancelled.
     *
     * This is how the C bindings take cancellation: a pointer to caller memory rather
     * than a handle to create and free, so a late cancel can never touch freed memory.
     */
    task::CCancellationToken MakeFlagCancellationToken(const int32_t* flag);

}
