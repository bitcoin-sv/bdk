package com.nchain.bdk;

public class Status {

    private final static boolean init = PackageInfo.loadJNI();

    private final int code;
    private final String message;

    Status(int code, String message){
        this.code = code;
        this.message = message;
    }

    public int getCode(){
        return code;
    }

    public String getMessage(){
        return message;
    }
	
	public static final int OK = 0;
    public static final int UNKNOWN_ERROR = 1;
    public static final int EVAL_FALSE = 2;
    public static final int OP_RETURN = 3;

    /* Max sizes */
    public static final int ERR_SCRIPT_SIZE = 4;
    public static final int ERR_PUSH_SIZE = 5;
    public static final int ERR_OP_COUNT = 6;
    public static final int ERR_STACK_SIZE = 7;
    public static final int ERR_SIG_COUNT = 8;
    public static final int ERR_PUBKEY_COUNT = 9;

    /* Operands checks */
    public static final int ERR_INVALID_OPERAND_SIZE = 10;
    public static final int ERR_INVALID_NUMBER_RANGE = 11;
    public static final int ERR_IMPOSSIBLE_ENCODING = 12;
    public static final int ERR_INVALID_SPLIT_RANGE = 13;
    public static final int ERR_SCRIPTNUM_OVERFLOW = 14;
    public static final int ERR_SCRIPTNUM_MINENCODE = 15;

    /* Failed verify operations */
    public static final int ERR_VERIFY = 16;
    public static final int ERR_EQUALVERIFY = 17;
    public static final int ERR_CHECKMULTISIGVERIFY = 18;
    public static final int ERR_CHECKSIGVERIFY = 19;
    public static final int ERR_NUMEQUALVERIFY = 20;

    /* Logical/Format/Canonical errors */
    public static final int ERR_BAD_OPCODE = 21;
    public static final int ERR_DISABLED_OPCODE = 22;
    public static final int ERR_INVALID_STACK_OPERATION = 23;
    public static final int ERR_INVALID_ALTSTACK_OPERATION = 24;
    public static final int ERR_UNBALANCED_CONDITIONAL = 25;

    /* Divisor errors */
    public static final int ERR_DIV_BY_ZERO = 26;
    public static final int ERR_MOD_BY_ZERO = 27;

    /* CHECKLOCKTIMEVERIFY and CHECKSEQUENCEVERIFY */
    public static final int ERR_NEGATIVE_LOCKTIME = 28;
    public static final int ERR_UNSATISFIED_LOCKTIME = 29;

    /* Malleability */
    public static final int ERR_SIG_HASHTYPE = 30;
    public static final int ERR_SIG_DER = 31;
    public static final int ERR_MINIMALDATA = 32;
    public static final int ERR_SIG_PUSHONLY = 33;
    public static final int ERR_SIG_HIGH_S = 34;
    public static final int ERR_SIG_NULLDUMMY = 35;
    public static final int ERR_PUBKEYTYPE = 36;
    public static final int ERR_CLEANSTACK = 37;
    public static final int ERR_MINIMALIF = 38;
    public static final int ERR_SIG_NULLFAIL = 39;

    /* softfork safeness */
    public static final int ERR_DISCOURAGE_UPGRADABLE_NOPS = 40;

    /* misc */
    public static final int ERR_NONCOMPRESSED_PUBKEY = 41;

    /* anti replay */
    public static final int ERR_ILLEGAL_FORKID = 42;
    public static final int ERR_MUST_USE_FORKID = 43;

    public static final int ERR_BIG_INT = 44;

    public static final int ERR_ERROR_COUNT = 45;
}
    
