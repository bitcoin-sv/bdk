package com.nchain.sesdk;

public class ScriptFlag
{
    public static final int VERIFY_NONE = 0;

    // Evaluate P2SH subscripts (softfork safe, BIP16).
    public static final int VERIFY_P2SH = (1 << 0);

    // Passing a non-strict-DER signature or one with undefined hashtype to a
    // checksig operation causes script failure. Evaluating a pubkey that is not
    // (0x04 + 64 bytes) or (0x02 or 0x03 + 32 bytes) by checksig causes script
    // failure.
    public static final int VERIFY_STRICTENC = (1 << 1);

    // Passing a non-strict-DER signature to a checksig operation causes script
    // failure (softfork safe, BIP62 rule 1)
    public static final int VERIFY_DERSIG = (1 << 2);

    // Passing a non-strict-DER signature or one with S > order/2 to a checksig
    // operation causes script failure
    // (softfork safe, BIP62 rule 5).
    public static final int VERIFY_LOW_S = (1 << 3);

    // verify dummy stack item consumed by CHECKMULTISIG is of zero-length
    // (softfork safe, BIP62 rule 7).
    public static final int VERIFY_NULLDUMMY = (1 << 4);

    // Using a non-push operator in the scriptSig causes script failure
    // (softfork safe, BIP62 rule 2).
    public static final int VERIFY_SIGPUSHONLY = (1 << 5);

    // Require minimal encodings for all push operations (OP_0... OP_16,
    // OP_1NEGATE where possible, direct pushes up to 75 bytes, OP_PUSHDATA up
    // to 255 bytes, OP_PUSHDATA2 for anything larger). Evaluating any other
    // push causes the script to fail (BIP62 rule 3). In addition, whenever a
    // stack element is interpreted as a number, it must be of minimal length
    // (BIP62 rule 4).
    // (softfork safe)
    public static final int VERIFY_MINIMALDATA = (1 << 6);

    // Discourage use of NOPs reserved for upgrades (NOP1-10)
    //
    // Provided so that nodes can avoid accepting or mining transactions
    // containing executed NOP's whose meaning may change after a soft-fork,
    // thus rendering the script invalid; with this flag set executing
    // discouraged NOPs fails the script. This verification flag will never be a
    // mandatory flag applied to scripts in a block. NOPs that are not executed,
    // e.g.  within an unexecuted IF ENDIF block, are *not* rejected.
    public static final int VERIFY_DISCOURAGE_UPGRADABLE_NOPS = (1 << 7);

    // Require that only a single stack element remains after evaluation. This
    // changes the success criterion from "At least one stack element must
    // remain, and when interpreted as a boolean, it must be true" to "Exactly
    // one stack element must remain, and when interpreted as a boolean, it must
    // be true".
    // (softfork safe, BIP62 rule 6)
    // Note: CLEANSTACK should never be used without P2SH or WITNESS.
    public static final int VERIFY_CLEANSTACK = (1 << 8);

    // Verify CHECKLOCKTIMEVERIFY
    //
    // See BIP65 for details.
    public static final int VERIFY_CHECKLOCKTIMEVERIFY = (1 << 9);

    // support CHECKSEQUENCEVERIFY opcode
    //
    // See BIP112 for details
    public static final int VERIFY_CHECKSEQUENCEVERIFY = (1 << 10);

    // Require the argument of OP_IF/NOTIF to be exactly 0x01 or empty vector
    //
    public static final int VERIFY_MINIMALIF = (1 << 13);

    // Signature(s) must be empty vector if an CHECK(MULTI)SIG operation failed
    //
    public static final int VERIFY_NULLFAIL = (1 << 14);

    // Public keys in scripts must be compressed
    //
    public static final int VERIFY_COMPRESSED_PUBKEYTYPE = (1 << 15);

    // Do we accept signature using SIGHASH_FORKID
    //
    public static final int ENABLE_SIGHASH_FORKID = (1 << 16);


    // Is Genesis enabled - transcations that is being executed is part of block that uses Geneisis rules.
    //
    public static final int GENESIS = (1 << 18);

    // UTXO being used in this script was created *after* Genesis upgrade
    // has been activated. This activates new rules (such as original meaning of OP_RETURN)
    // This is per (input!) UTXO flag
    public static final int UTXO_AFTER_GENESIS = (1 << 19);

    // Not actual flag. Used for marking largest flag value.
    public static final int FLAG_LAST = (1 << 20);
}