## Script engine object model

Plant UML description

@startuml
class Status 
{ 
	+unsigned long code;
	+string message;
}
class Config 
{
	+Config(boolean isGenesisEnabled, boolean isConsensus)

    +void load(String filename);

    +uint64 getMaxOpsPerScript();
    +uint64 getMaxScriptNumLength();
    +uint64 getMaxScriptSize();
    +uint64 getMaxPubKeysPerMultiSig();
    +uint64 getMaxStackMemoryUsage();

    +void setMaxOpsPerScriptPolicy(uint64 v);
    +void setMaxScriptnumLengthPolicy(uint64 v);
    +void setMaxScriptSizePolicy(uint64 v);
    +void setMaxPubkeysPerMultisigPolicy(uint64 v);
    +void setMaxStackMemoryUsage(uint64 v1, uint64 v2);

    +bool isGenesisEnabled;
    +bool isConsensus;
}
class Assembler
{
    +byte[] fromAsm(String script);
    +string toAsm(byte[] script);
}
class ScriptIterator
{
	+ScriptIterator(byte[])
	
	+bool next();
	+readonly int opcode;
	+readonly byte[] data;
}
class CancellationToken
{
	+void cancel();
}
class Stack
{
    +int64 size();
    +byte[] at(int pos);
}
class ScriptEngine 
{
	+ScriptEngine(Config config, uint64 Flags);
	
	+Status execute(byte[] script, CancellationToken token, String txHex, int index, int amount);
    +Status execute(String script, CancellationToken token, String txHex, int index, int amount);
	+reset(Config config, uint64 Flags);
	
	+boolean[] getExecState();
	+boolean[] getElseState();
}
ScriptEngine --> Stack : stack
ScriptEngine --> Stack : alt-stack
@enduml

