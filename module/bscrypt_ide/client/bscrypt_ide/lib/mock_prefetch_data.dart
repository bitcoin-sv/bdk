/// Just to prototype
/// In production, those data should be fetch from http server

const List<String> mockListFlagStr = [
  "SCRIPT_VERIFY_NONE",
  "SCRIPT_VERIFY_P2SH",
  "SCRIPT_VERIFY_STRICTENC",
  "SCRIPT_VERIFY_DERSIG",
  "SCRIPT_VERIFY_LOW_S",
  "SCRIPT_VERIFY_NULLDUMMY",
  "SCRIPT_VERIFY_SIGPUSHONLY",
  "SCRIPT_VERIFY_MINIMALDATA",
  "SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS",
  "SCRIPT_VERIFY_CLEANSTACK",
  "SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY",
  "SCRIPT_VERIFY_CHECKSEQUENCEVERIFY",
  "SCRIPT_VERIFY_MINIMALIF",
  "SCRIPT_VERIFY_NULLFAIL",
  "SCRIPT_VERIFY_COMPRESSED_PUBKEYTYPE",
  "SCRIPT_ENABLE_SIGHASH_FORKID",
  "SCRIPT_GENESIS",
  "SCRIPT_UTXO_AFTER_GENESIS",
  "SCRIPT_FLAG_LAST"
];

const List<String> mockListOpCodeStr = [
  "OP_0",
  "OP_FALSE",
  "OP_PUSHDATA1",
  "OP_PUSHDATA2",
  "OP_PUSHDATA4",
  "OP_1NEGATE",
  "OP_RESERVED",
  "OP_1",
  "OP_TRUE",
  "OP_2",
  "OP_3",
  "OP_4",
  "OP_5",
  "OP_6",
  "OP_7",
  "OP_8",
  "OP_9",
  "OP_10",
  "OP_11",
  "OP_12",
  "OP_13",
  "OP_14",
  "OP_15",
  "OP_16",
  "OP_NOP",
  "OP_VER",
  "OP_IF",
  "OP_NOTIF",
  "OP_VERIF",
  "OP_VERNOTIF",
  "OP_ELSE",
  "OP_ENDIF",
  "OP_VERIFY",
  "OP_RETURN",
  "OP_TOALTSTACK",
  "OP_FROMALTSTACK",
  "OP_2DROP",
  "OP_2DUP",
  "OP_3DUP",
  "OP_2OVER",
  "OP_2ROT",
  "OP_2SWAP",
  "OP_IFDUP",
  "OP_DEPTH",
  "OP_DROP",
  "OP_DUP",
  "OP_NIP",
  "OP_OVER",
  "OP_PICK",
  "OP_ROLL",
  "OP_ROT",
  "OP_SWAP",
  "OP_TUCK",
  "OP_CAT",
  "OP_SPLIT",
  "OP_NUM2BIN",
  "OP_BIN2NUM",
  "OP_SIZE",
  "OP_INVERT",
  "OP_AND",
  "OP_OR",
  "OP_XOR",
  "OP_EQUAL",
  "OP_EQUALVERIFY",
  "OP_RESERVED1",
  "OP_RESERVED2",
  "OP_1ADD",
  "OP_1SUB",
  "OP_2MUL",
  "OP_2DIV",
  "OP_NEGATE",
  "OP_ABS",
  "OP_NOT",
  "OP_0NOTEQUAL",
  "OP_ADD",
  "OP_SUB",
  "OP_MUL",
  "OP_DIV",
  "OP_MOD",
  "OP_LSHIFT",
  "OP_RSHIFT",
  "OP_BOOLAND",
  "OP_BOOLOR",
  "OP_NUMEQUAL",
  "OP_NUMEQUALVERIFY",
  "OP_NUMNOTEQUAL",
  "OP_LESSTHAN",
  "OP_GREATERTHAN",
  "OP_LESSTHANOREQUAL",
  "OP_GREATERTHANOREQUAL",
  "OP_MIN",
  "OP_MAX",
  "OP_WITHIN",
  "OP_RIPEMD160",
  "OP_SHA1",
  "OP_SHA256",
  "OP_HASH160",
  "OP_HASH256",
  "OP_CODESEPARATOR",
  "OP_CHECKSIG",
  "OP_CHECKSIGVERIFY",
  "OP_CHECKMULTISIG",
  "OP_CHECKMULTISIGVERIFY",
  "OP_NOP1",
  "OP_CHECKLOCKTIMEVERIFY",
  "OP_NOP2",
  "OP_CHECKSEQUENCEVERIFY",
  "OP_NOP3",
  "OP_NOP4",
  "OP_NOP5",
  "OP_NOP6",
  "OP_NOP7",
  "OP_NOP8",
  "OP_NOP9",
  "OP_NOP10",
  "FIRST_UNDEFINED_OP_VALUE",
  "OP_SMALLINTEGER",
  "OP_PUBKEYS",
  "OP_PUBKEYHASH",
  "OP_PUBKEY",
  "OP_INVALIDOPCODE",
];