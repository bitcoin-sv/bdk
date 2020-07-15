@startuml
class Status 
{ 
	+readonly unsigned long code;
	+readonly string message;
}

class Assembler
{
	+byte[] fromAsm(string asm);
	+string toAsm(byte[] script);
}

class ScriptEngine 
{
    Status evaluate(byte[] script, boolean concensus, int flags, string txHex, int index, int amount);
    Status evaluate(string script, boolean concensus, int flags, string txHex, int index, int amount);
}
@enduml