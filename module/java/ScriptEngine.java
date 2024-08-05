package com.nchain.sesdk;

public class ScriptEngine implements AutoCloseable {

    private final static boolean init = PackageInfo.loadJNI();

    public ScriptEngine(Config c, int f){
        this.config = c;
        this.flags = f;
        this.stack = new Stack(c.getMaxStackMemoryUsage());
        this.altstack = null;
        initAltStack();
        initBranchStates(); // vfExec and vfElse
    }

    @Override
    public void close() {
		this.config = null;
        this.stack.close();
        this.altstack.close();
        deleteBranchStates(); // vfExec and vfElse
    }

	public void reset(Config config, int flags)
	{
		this.config = null;
        this.altstack.close();
        deleteBranchStates();

		this.config = config;
        this.flags = flags;
        this.stack = new Stack(config.getMaxStackMemoryUsage());
        this.altstack = null;
        initAltStack();
        initBranchStates();
	}
	
    public native Status execute(byte[] script, CancellationToken token, String txHex, int index, long amount);
    public native Status execute(String script, CancellationToken token, String txHex, int index, long amount);

    public Stack getStack()    { return stack; }
    public Stack getAltStack() { return altstack; }

    public native boolean[] getExecState();
    public native boolean[] getElseState();

    private Config config;
    private int flags;
    private Stack stack;

    private Stack altstack;
    private native void initAltStack();

    private native void initBranchStates();
    private native void deleteBranchStates();
    private long cppExecState; // pointer holding cpp vector vfExec
    private long cppElseState; // pointer holding cpp vector vfElse

    public native Status verify(byte[] scriptSig, byte[] scriptPub, boolean concensus, int scriptflags, byte[] txHex, int nIndex, long amount);

    public native Status execute(byte[] script, CancellationToken token, byte[] tx, int index, long amount);
}
    
