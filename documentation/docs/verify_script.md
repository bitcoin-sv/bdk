# Verify Script

[![](https://mermaid.ink/img/pako:eNrNVV1v2jAU_SuRn0AC2jBKm2hU6ujHeECqBOrDlgmZxBCrwY5sp6Mr_e9zYkjsJE3Z1ochIcX2veee--HjF-DTAAEXrCL60w8hE9b82iOW_D0ghlfPM5_hWHz3gL78vGQnly2fEo4IT3jHWkVwzdse-OER5TxWhuMQ-Y_SWV-6Lo0Rg4KyVrsJKIWZ4TW5IoGKLXGMdcVZonasRGzpDYMFxjQRUKD5NvWVEJz5J0ssfIpJV2x7fhxnOLpVy8gk5TwhcSJUDnyP8QQjHECBKckxKqYNmRW2RwHyxmrPt-Qh934bzjBrFVy-RNR_3J_IEpAGRhXT1tt53TK6maJNTGkkezaGfoiOSrXGrxzFT63f7YduZfSUyyYz-FMwSDj0taKxWP6Ngxys6qNV8DYhgeSaDs8UE8QmZEXnWw10g4lAbIHlforo1jtoiE_ZiC_44fqlQGpxmN58IHKKho8q2YQUkIe2XdNkGaFZjEiwBw6ynQVPt04CDoWQ5ReLEJIgQizHrwHQGH94USc8LY2ZO-aykigroTpuZThGc5PlmsE4tMaUIUu137oha2msDMr6ZFnd7uWuuF9Z4XaG_hWOhgJljh4oX01rZM2ErB1kgXJX1rfpWUuKVNvaWZpMlE51CSuz0BIoa02JiuSQf09IQWw_FX_AQC9VXXxeqUIWI3Mtkyz8DS0qAYxWMOJIwYzuUD3Jthngb6CnaY_kO_T87wFyrplAKjyFJB9VTOY4LsPVqFwRoSKzxoSOBEvQYUxrGTbHaahJJecCU1fTbu-drJfpxleE16Ho2lnqlZQUstSQQxD9CS7PlEr5iOt1JVXkCd00XrLcpumq1TIy-exMNWh6CurzuWdUUJ9GkoHr3lMu7hBBHPM3D1KaNTGrMlob7w1vJaTHetQ8T7W5qdnIh6pa3Crp_6LnNY-c9QH6lAvgoYKgA9YMB8DNsgQbxDYwXYKX9NwDIkQbWX9XfgZoBZNIeMAjr9IthuQbpZuDJ6PJOgSu4gSSOCOPoXwDCxOZBGJjmhABXCdDAO4L2AL302mvfzqwnf6w3-8PB2dnHfAMXNvu9_r2-cW5Lbec4Zljv3bAryzmac8ZDqTlxdAZXjjOwB68_gbKe1fU?type=png)](https://mermaid.live/edit#pako:eNrNVV1v2jAU_SuRn0AC2jBKm2hU6ujHeECqBOrDlgmZxBCrwY5sp6Mr_e9zYkjsJE3Z1ochIcX2veee--HjF-DTAAEXrCL60w8hE9b82iOW_D0ghlfPM5_hWHz3gL78vGQnly2fEo4IT3jHWkVwzdse-OER5TxWhuMQ-Y_SWV-6Lo0Rg4KyVrsJKIWZ4TW5IoGKLXGMdcVZonasRGzpDYMFxjQRUKD5NvWVEJz5J0ssfIpJV2x7fhxnOLpVy8gk5TwhcSJUDnyP8QQjHECBKckxKqYNmRW2RwHyxmrPt-Qh934bzjBrFVy-RNR_3J_IEpAGRhXT1tt53TK6maJNTGkkezaGfoiOSrXGrxzFT63f7YduZfSUyyYz-FMwSDj0taKxWP6Ngxys6qNV8DYhgeSaDs8UE8QmZEXnWw10g4lAbIHlforo1jtoiE_ZiC_44fqlQGpxmN58IHKKho8q2YQUkIe2XdNkGaFZjEiwBw6ynQVPt04CDoWQ5ReLEJIgQizHrwHQGH94USc8LY2ZO-aykigroTpuZThGc5PlmsE4tMaUIUu137oha2msDMr6ZFnd7uWuuF9Z4XaG_hWOhgJljh4oX01rZM2ErB1kgXJX1rfpWUuKVNvaWZpMlE51CSuz0BIoa02JiuSQf09IQWw_FX_AQC9VXXxeqUIWI3Mtkyz8DS0qAYxWMOJIwYzuUD3Jthngb6CnaY_kO_T87wFyrplAKjyFJB9VTOY4LsPVqFwRoSKzxoSOBEvQYUxrGTbHaahJJecCU1fTbu-drJfpxleE16Ho2lnqlZQUstSQQxD9CS7PlEr5iOt1JVXkCd00XrLcpumq1TIy-exMNWh6CurzuWdUUJ9GkoHr3lMu7hBBHPM3D1KaNTGrMlob7w1vJaTHetQ8T7W5qdnIh6pa3Crp_6LnNY-c9QH6lAvgoYKgA9YMB8DNsgQbxDYwXYKX9NwDIkQbWX9XfgZoBZNIeMAjr9IthuQbpZuDJ6PJOgSu4gSSOCOPoXwDCxOZBGJjmhABXCdDAO4L2AL302mvfzqwnf6w3-8PB2dnHfAMXNvu9_r2-cW5Lbec4Zljv3bAryzmac8ZDqTlxdAZXjjOwB68_gbKe1fU)

In Bitcoin SV the `VerifyScript` function is called in many places with different context and argument. It can be confusing if we don't have a clear picture of how it works. This document section is for `bdk` developers to understand how it works in `bitcoin-sv`.

Generally speaking, `VerifyScript` is called in two main contexts:

- Then a transaction comes from a peer
- Then a transaction comes from a block

When a transaction comes from a block, we verify the script with `consensus=true`, skipping all the policy settings check, while a transaction comes from a peer, we might verify if it complies with all the policies settings.

As there are two different contexts, there are two different ways of calculating flags to used as input for `VerifyScript` function.

- [GetScriptVerifyFlags](https://github.com/teranode-group/bitcoin-sv-staging/blob/develop/src/verify_script_flags.h#L9) is to be used when verify a transaction coming from a peer
- [GetBlockScriptFlags](https://github.com/teranode-group/bitcoin-sv-staging/blob/develop/src/verify_script_flags.h#L19) is to be used when verify a transaction coming from a block

## Flags calculations

Flags are canonical combination of individual flags

```
    SCRIPT_VERIFY_NONE
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_SIGPUSHONLY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY
    SCRIPT_VERIFY_MINIMALIF
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE
    SCRIPT_ENABLE_SIGHASH_FORKID
    SCRIPT_GENESIS
    SCRIPT_UTXO_AFTER_GENESIS      // Per-input: UTXO was created after Genesis activation
    SCRIPT_CHRONICLE
    SCRIPT_UTXO_AFTER_CHRONICLE    // Per-input: UTXO was created after Chronicle activation
    SCRIPT_FLAG_LAST
```

When we flatten out the flags calculation of in the two different contexts as mentioned above, we have

`GetScriptVerifyFlags` can have these flags, depending on the era (derived from chain tip + 1)

```
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_ENABLE_SIGHASH_FORKID
    SCRIPT_GENESIS   // If genesis is activated (regardless of Chronicle)
    SCRIPT_CHRONICLE // If Chronicle is activated (both SCRIPT_GENESIS and SCRIPT_CHRONICLE are set when both eras are active)
    // On non-mainnet only (require_standard = false):
    //   If promiscuous mempool flags are set, the entire flag set is replaced by
    //   prom_mempool_flags, then SCRIPT_ENABLE_SIGHASH_FORKID is unconditionally re-added.
```

Note: `SCRIPT_VERIFY_SIGPUSHONLY` is **not** part of the base flags here — it is added per-input (see Per-input flags below).

`GetBlockScriptFlags` can have these flags, depending on the actual block height (height-based hard-fork activation)

```
GetBlockScriptFlags(blockHeight)
    SCRIPT_VERIFY_NONE                 // starting value (= 0), not a real flag
    SCRIPT_VERIFY_P2SH                 // height >= p2shHeight
    SCRIPT_VERIFY_STRICTENC            // height >= uahfHeight (UAHF)
    SCRIPT_VERIFY_DERSIG               // (height + 1) >= BIP66Height
    SCRIPT_VERIFY_LOW_S                // height >= daaHeight
    SCRIPT_VERIFY_SIGPUSHONLY          // Genesis active — added at base level (not per-input)
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY  // (height + 1) >= BIP65Height
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY  // (height + 1) >= CSVHeight
    SCRIPT_VERIFY_NULLFAIL             // height >= daaHeight
    SCRIPT_ENABLE_SIGHASH_FORKID       // height >= uahfHeight (UAHF)
    SCRIPT_GENESIS                     // Genesis protocol active for this block height
    SCRIPT_CHRONICLE                   // Chronicle protocol active for this block height
```

Notable absences vs `GetScriptVerifyFlags`: no `SCRIPT_VERIFY_NULLDUMMY`, `SCRIPT_VERIFY_CLEANSTACK`, `SCRIPT_VERIFY_MINIMALDATA`, `SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS` — these are policy-only flags, not consensus rules.

There are some _predefined_ combined flags to be used for convenient (being flattened out):

```
PRE_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_ENABLE_SIGHASH_FORKID

POST_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_ENABLE_SIGHASH_FORKID
    SCRIPT_CHRONICLE

STANDARD_SCRIPT_VERIFY_FLAGS
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY

PRE_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_ENABLE_SIGHASH_FORKID

POST_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS
    SCRIPT_VERIFY_P2SH
    SCRIPT_VERIFY_STRICTENC
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_LOW_S
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY
    SCRIPT_VERIFY_NULLFAIL
    SCRIPT_ENABLE_SIGHASH_FORKID
    SCRIPT_CHRONICLE

PRE_CHRONICLE_STANDARD_NOT_MANDATORY_VERIFY_FLAGS
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY

POST_CHRONICLE_STANDARD_NOT_MANDATORY_VERIFY_FLAGS
    SCRIPT_VERIFY_DERSIG
    SCRIPT_VERIFY_NULLDUMMY
    SCRIPT_VERIFY_MINIMALDATA
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS
    SCRIPT_VERIFY_CLEANSTACK
    SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY
```

These _predefined_ flags are original defined in a nested approach:

```
PRE_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS =
    SCRIPT_VERIFY_P2SH |
    SCRIPT_VERIFY_STRICTENC |
    SCRIPT_ENABLE_SIGHASH_FORKID |
    SCRIPT_VERIFY_NULLFAIL |
    SCRIPT_VERIFY_LOW_S

POST_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS =
    SCRIPT_VERIFY_P2SH |
    SCRIPT_VERIFY_STRICTENC |
    SCRIPT_ENABLE_SIGHASH_FORKID |
    SCRIPT_VERIFY_NULLFAIL |
    SCRIPT_VERIFY_LOW_S |
    SCRIPT_CHRONICLE

STANDARD_SCRIPT_VERIFY_FLAGS =
        SCRIPT_VERIFY_DERSIG |
        SCRIPT_VERIFY_NULLDUMMY |
        SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
        SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
        SCRIPT_VERIFY_CHECKSEQUENCEVERIFY |
        SCRIPT_VERIFY_CLEANSTACK |
        SCRIPT_VERIFY_MINIMALDATA

PRE_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS =
        PRE_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS |
        STANDARD_SCRIPT_VERIFY_FLAGS

POST_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS =
        POST_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS |
        STANDARD_SCRIPT_VERIFY_FLAGS

PRE_CHRONICLE_STANDARD_NOT_MANDATORY_VERIFY_FLAGS =
        PRE_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS &~PRE_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS

POST_CHRONICLE_STANDARD_NOT_MANDATORY_VERIFY_FLAGS =
        POST_CHRONICLE_STANDARD_SCRIPT_VERIFY_FLAGS &~POST_CHRONICLE_MANDATORY_SCRIPT_VERIFY_FLAGS
```

### Per-input flags

After the base flags are computed, `CheckInputScripts` adds per-UTXO flags via `InputScriptVerifyFlags(spendingEra, utxoEra)`. These are ORed with the base flags for each individual input:

```
SCRIPT_VERIFY_SIGPUSHONLY    // spending era >= Genesis (peer path only — already in base for block path)
SCRIPT_UTXO_AFTER_GENESIS    // UTXO creation era >= Genesis
SCRIPT_UTXO_AFTER_CHRONICLE  // UTXO creation era >= Chronicle
```

This means a transaction spending old (pre-Genesis) UTXOs and new (post-Chronicle) UTXOs in the same transaction will have different flags per input.

---

## Two Transaction Validation Paths

A transaction is validated differently depending on whether it arrives **from a peer** (mempool admission) or **from a block** (block connection). Understanding this distinction is key to interpreting any `VerifyScript` call.

### Path 1 — Transaction from a block (`BlockValidateTxns`)

- **Flags function:** `GetBlockScriptFlags(parentBlockHeight)` — height-based, hard-fork activation only
- **consensus parameter:** `true` — skips policy checks inside `VerifyScript`
- **Calls:** exactly one `CheckInputs` per transaction
- **Failure:** block rejected; peer banned (DoS=100), except for locally-configured frozen TXOs

This path is strict and simple: the transaction either passes the consensus rules or the whole block is rejected.

### Path 2 — Transaction from a peer (`TxnValidation`)

- **Flags function:** `GetScriptVerifyFlags(era)` — era-based, includes policy flags
- **consensus parameter:** `false` — policy rules (standardness) are enforced
- **Calls:** up to **three** `CheckInputs` calls

#### Why up to three calls?

| Call | Flags used | Purpose |
|------|-----------|---------|
| **1** | `GetScriptVerifyFlags` (full policy set) | Primary validation — enforce all policy rules |
| **2** | `GetBlockScriptFlags(chainTip)` | Sanity check — would this tx be valid in the next block? |
| **3** | `MandatoryScriptVerifyFlags(era)` | Last resort — does it pass at least the consensus-mandatory rules? |

Calls 2 and 3 only run when `require_standard = false` (non-mainnet) **and** the promiscuous mempool config dropped flags that the block tip enforces. On mainnet (`require_standard = true`) Call 1's flag set is always a strict superset of the block-tip set, so Calls 2 and 3 always pass trivially.

#### Chronicle grace period (Call 1 only)

During the Chronicle activation grace period, if Call 1 fails, the validation is **retried once** with the inverse-era flags (pre-Chronicle flags). If the retry passes, the transaction is rejected with **no peer ban** — the peer may simply not have activated Chronicle yet.

#### Failure outcomes (peer path)

| Situation | Consequence |
|-----------|-------------|
| Fails on a policy-only flag | Rejected, **no ban** |
| Fails on a consensus flag | Rejected, peer **banned** |
| Timeout (script ran too long) | Rejected, **no ban** |
| Fails Call 1 during Chronicle grace period, passes retry | Rejected, **no ban** |
| Fails Call 3 (mandatory fallback) | Rejected, peer **banned** |
| Passes Call 3 with promiscuous config | Accepted, warning logged |

### Key differences at a glance

| Aspect | From a block | From a peer |
|--------|-------------|-------------|
| Flags function | `GetBlockScriptFlags` | `GetScriptVerifyFlags` |
| Flag basis | Hard-fork height thresholds | Era-based standard/mandatory sets |
| `consensus` to `VerifyScript` | `true` | `false` |
| Policy flags (`NULLDUMMY`, `CLEANSTACK`, etc.) | **No** | **Yes** |
| `SCRIPT_VERIFY_SIGPUSHONLY` | In base flags (Genesis) | Per-input only |
| Number of `CheckInputs` calls | 1 | Up to 3 |
| Grace period retry | No | Yes (Chronicle only) |
| Failure consequence | Block rejected + peer banned | Depends on which flag failed |

---

## Detail VerifyScript Call stack

Below is a summary of analysis `VerifyScript` caller

```
BlockConnector::Connect --> BlockConnector::checkScripts --> BlockConnector::BlockValidateTxns
src/Validation.cpp:TxnValidation --> CheckInputsFromMempoolAndCache

CheckInputsFromMempoolAndCache       -->  CheckInputs (consensus false)
TxnValidation                        -->  CheckInputs (consensus false)   [the most sophisticated]
BlockConnector::BlockValidateTxns    -->  CheckInputs (consensus  true)

CheckInputs                            --> CheckInputScripts --> CScriptCheck::operator() --> VerifyScript
DSAttemptHandler::ValidateDoubleSpend -- > CheckInputScripts --> CScriptCheck::operator() --> VerifyScript  (consensus false)

src/script/sign.cpp:SignAndVerify  --> VerifyScript
```

SignAndVerify is mostly called with consensus true, except in src/script/sign.cpp:SignSignature which call it with consensus false

### Other path to VerifyScript
```
rawtransaction.cpp:signrawtransaction --> VerifyScript
bitcoinconcensus.cpp:verify_script --> VerifyScript
bitcoin-tx.cpp:MutateTx --> MutateTx --> MutateTxSign --> SignAndVerify --> VerifyScript
bitcoin-tx.cpp:MutateTx --> MutateTx --> MutateTxSign --> VerifyScript
src/rpc/misc.cpp:verifyscript --> CScriptCheck::operator() --> VerifyScript (consensus false)
```
All have config object (global) so don't need to mention here

#### Analyse of VerifyScript
```
CScriptCheck::operator(consensusIn, flagsIn)
    VerifyScript(){consensus=consensusIn, flags = flagsIn}

src/script/sign.cpp:SignAndVerify(consensusIn, eraIn, utxoEraIn)
    VerifyScript(){consensus=consensusIn, flags = StandardScriptVerifyFlags(eraIn) | InputScriptVerifyFlags(eraIn, utxoEraIn)}

src/bitcoin-tx.cpp:MutateTxSign() // end of chain call
    SignAndVerify(ActiveEra,utxoEra){consensus=true}
    VerifyScript(){consensus=true, flags = StandardScriptVerifyFlags(ActiveEra) | InputScriptVerifyFlags(ActiveEra, utxoEra)}

src/rpc/rawtransaction.cpp:signrawtransaction() // end of chain call
    VerifyScript(){consensus=true, flags = StandardScriptVerifyFlags(ActiveEra) | InputScriptVerifyFlags(ActiveEra, utxoEra)}

src/script/bitcoinconsensus.cpp:verify_script(flagsIn) // Export to library, don't need to care
    VerifyScript(){consensus=true, flags = flagsIn}
```
#### Analyse of CScriptCheck::CScriptCheck
```
src/validation.cpp:CheckInputScripts(consensusIn, flagsIn)
    CScriptCheck(){consensus = consensusIn, flags = flagsIn | InputScriptVerifyFlags(era, utxoEra) }
    There are check2, check3 to handle grace periode, we don't care
```
#### Analyse of CheckInputScripts
```
src/validation.cpp:CheckInputs(consensusIn, flagsIn)
    CheckInputScripts(){consensus = consensusIn, flags = flagsIn}

src/double_spend/dsattempt_handler.cpp:ValidateDoubleSpend() // end of chain call
    CheckInputScripts(){consensus = false, flags = GetScriptVerifyFlags(era)}
```
#### Analyse of CheckInputs
```
src/validation.cpp:TxnValidation() // end of chain call
    CheckInputs(){consensus = false, flags = GetScriptVerifyFlags(era)}
    CheckInputs(){consensus = false, flags = MandatoryScriptVerifyFlags(era)}

src/validation.cpp:BlockValidateTxns(flagsIn)
    CheckInputs(){consensus = true, flags = flagsIn}

src/validation.cpp:CheckInputsFromMempoolAndCache(flagsIn)
    CheckInputs(){consensus = false, flags = flagsIn}
```
#### Analyse of BlockValidateTxns
```
src/validation.cpp:checkScripts() // End of chain call
    BlockValidateTxns(){flags = GetBlockScriptFlags(blockHeight-1)}
```
#### Analyse of CheckInputsFromMempoolAndCache
```
src/validation.cpp:TxnValidation() // end of chain call
    CheckInputsFromMempoolAndCache(){flags =  GetBlockScriptFlags(chainTip)}
```
#### Analyse of SignAndVerify
```
src/bitcoin-tx.cpp:MutateTxSign()
    SignAndVerify(ActiveEra,utxoEra){consensus=true}

src/rpc/minter_info.cpp:FundAndSignMinerInfoTx()
    SignAndVerify(){consensus=true, ProtocolEra::PostGenesis, ProtocolEra::PostGenesis}

src/rpc/rawtransaction.cpp:signrawtransaction()
    SignAndVerify(){consensus=true, era, utxoEra}

src/script/ismine.cpp:IsMine()
    SignAndVerify(){consensus=true, ProtocolEra::PostGenesis, utxoEra}
```