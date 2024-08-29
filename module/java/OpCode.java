package com.nchain.bdk;

public class OpCode
{
    public static final byte _0 = 0x00;
    public static final byte FALSE = 0;
    public static final byte PUSHDATA1 = 0x4c;
    public static final byte PUSHDATA2 = 0x4d;
    public static final byte PUSHDATA4 = 0x4e;
    public static final byte _1NEGATE = 0x4f;
    public static final byte RESERVED = 0x50;
    public static final byte _1 = 0x51;
    public static final byte TRUE = _1;
    public static final byte _2 = 0x52;
    public static final byte _3 = 0x53;
    public static final byte _4 = 0x54;
    public static final byte _5 = 0x55;
    public static final byte _6 = 0x56;
    public static final byte _7 = 0x57;
    public static final byte _8 = 0x58;
    public static final byte _9 = 0x59;
    public static final byte _10 = 0x5a;
    public static final byte _11 = 0x5b;
    public static final byte _12 = 0x5c;
    public static final byte _13 = 0x5d;
    public static final byte _14 = 0x5e;
    public static final byte _15 = 0x5f;
    public static final byte _16 = 0x60;

    // control
    public static final byte NOP = 0x61;
    public static final byte VER = 0x62;
    public static final byte IF = 0x63;
    public static final byte NOTIF = 0x64;
    public static final byte VERIF = 0x65;
    public static final byte VERNOTIF = 0x66;
    public static final byte ELSE = 0x67;
    public static final byte ENDIF = 0x68;
    public static final byte VERIFY = 0x69;
    public static final byte RETURN = 0x6a; 

    // stack ops
    public static final byte TOALTSTACK = 0x6b;
    public static final byte FROMALTSTACK = 0x6c;
    public static final byte _2DROP = 0x6d;
    public static final byte _2DUP = 0x6e;
    public static final byte _3DUP = 0x6f;
    public static final byte _2OVER = 0x70;
    public static final byte _2ROT = 0x71;
    public static final byte _2SWAP = 0x72;
    public static final byte IFDUP = 0x73;
    public static final byte DEPTH = 0x74;
    public static final byte DROP = 0x75;
    public static final byte DUP = 0x76;
    public static final byte NIP = 0x77;
    public static final byte OVER = 0x78;
    public static final byte PICK = 0x79;
    public static final byte ROLL = 0x7a;
    public static final byte ROT = 0x7b;
    public static final byte SWAP = 0x7c;
    public static final byte TUCK = 0x7d;

    // splice ops
    public static final byte CAT = 0x7e;
    public static final byte SPLIT = 0x7f;   // after monolith upgrade (May 2018)
    public static final byte NUM2BIN = (byte)0x80; // after monolith upgrade (May 2018)
    public static final byte BIN2NUM = (byte)0x81; // after monolith upgrade (May 2018)
    public static final byte SIZE = (byte)0x82;

    // bit logic
    public static final byte INVERT = (byte)0x83;
    public static final byte AND = (byte)0x84;
    public static final byte OR = (byte)0x85;
    public static final byte XOR = (byte)0x86;
    public static final byte EQUAL = (byte)0x87;
    public static final byte EQUALVERIFY = (byte)0x88;
    public static final byte RESERVED1 = (byte)0x89;
    public static final byte RESERVED2 = (byte)0x8a;

    // numeric
    public static final byte _1ADD = (byte)0x8b;
    public static final byte _1SUB = (byte)0x8c;
    public static final byte _2MUL = (byte)0x8d;
    public static final byte _2DIV = (byte)0x8e;
    public static final byte NEGATE = (byte)0x8f;
    public static final byte ABS = (byte)0x90;
    public static final byte NOT = (byte)0x91;
    public static final byte NOTEQUAL = (byte)0x92;

    public static final byte ADD = (byte)0x93;
    public static final byte SUB = (byte)0x94;
    public static final byte MUL = (byte)0x95;
    public static final byte DIV = (byte)0x96;
    public static final byte MOD = (byte)0x97;
    public static final byte LSHIFT = (byte)0x98;
    public static final byte RSHIFT = (byte)0x99;

    public static final byte BOOLAND = (byte)0x9a;
    public static final byte BOOLOR = (byte)0x9b;
    public static final byte NUMEQUAL = (byte)0x9c;
    public static final byte NUMEQUALVERIFY = (byte)0x9d;
    public static final byte NUMNOTEQUAL = (byte)0x9e;
    public static final byte LESSTHAN = (byte)0x9f;
    public static final byte GREATERTHAN = (byte)0xa0;
    public static final byte LESSTHANOREQUAL = (byte)0xa1;
    public static final byte GREATERTHANOREQUAL = (byte)0xa2;
    public static final byte MIN = (byte)0xa3;
    public static final byte MAX = (byte)0xa4;

    public static final byte WITHIN = (byte)0xa5;

    // crypto
    public static final byte RIPEMD160 = (byte)0xa6;
    public static final byte SHA1 = (byte)0xa7;
    public static final byte SHA256 = (byte)0xa8;
    public static final byte HASH160 = (byte)0xa9;
    public static final byte HASH256 = (byte)0xaa;
    public static final byte CODESEPARATOR = (byte)0xab;
    public static final byte CHECKSIG = (byte)0xac;
    public static final byte CHECKSIGVERIFY = (byte)0xad;
    public static final byte CHECKMULTISIG = (byte)0xae;
    public static final byte CHECKMULTISIGVERIFY = (byte)0xaf;

    // expansion
    public static final byte NOP1 = (byte)0xb0;
    public static final byte CHECKLOCKTIMEVERIFY = (byte)0xb1;
    public static final byte NOP2 = CHECKLOCKTIMEVERIFY;
    public static final byte CHECKSEQUENCEVERIFY = (byte)0xb2;
    public static final byte NOP3 = CHECKSEQUENCEVERIFY;
    public static final byte NOP4 = (byte)0xb3;
    public static final byte NOP5 = (byte)0xb4;
    public static final byte NOP6 = (byte)0xb5;
    public static final byte NOP7 = (byte)0xb6;
    public static final byte NOP8 = (byte)0xb7;
    public static final byte NOP9 = (byte)0xb8;
    public static final byte NOP10 = (byte)0xb9;

    // The first op_code value after all defined opcodes
    public static final byte FIRST_UNDEFINED_VALUE = (byte)0xba;

    // template matching params
    public static final byte SMALLINTEGER = (byte)0xfa;
    public static final byte PUBKEYS = (byte)0xfb;
    public static final byte PUBKEYHASH = (byte)0xfd;
    public static final byte PUBKEY = (byte)0xfe;

    public static final byte INVALIDOPCODE = (byte)0xff;
}
    
