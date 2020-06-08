## Bscrypt object model

Object model is an approach to implement scripts functionalities in a object oriented fashion. The main function in script engine is the evaluation of the script. Other classes provide facilities to hold additional/auxiliare data to this function.

 - **config** class : hold all script's configuration prior to its evaluation.
 - **ExecutionStatus** class : hold the result of script's evaluation, i.e its return code and error message if any.
 - **Assembler** class : Implement method that convert a binary script to string script (the underlying data of a script is a binary array)
 - **ScriptEngine** class : hold the script configuration and script's evaluation flag switch. These determine how script should be evaluated. It has the execute method to evaluate a script
 - **Stack** class : hold the stack result of script evaluation.


```plantuml
@startuml
class ExecutionStatus 
{ 
    +unsigned long code;
    +string message;
}
class Config 
note right: Additional parameters to go here.
class Config 
{
    +unsigned long maxMemoryPolicy;
    +unsigned long maxMemoryConsensus
    +load(string filename);
}

class Assembler
{
    +byte[] fromAsm(string asm);
    +string toAsm(byte[] script);
}
class ScriptIterator
{
    +ScriptIterator(byte[])
    +bool next();
    +int opcode;
    +byte[] data;
}
class CancellationToken
{
    +void cancel();
}
class StackElement 
{
    +string asString();
    +string asHex();
}
class Stack
{
    +unsigned long size();
    +StackElement at(unsigned long pos);
    +void clear();
}
class ScriptEngine 
{
    +ScriptEngine(Config config);
    +unsigned long Flags;
    +ExecutionStatus execute(byte[] script, byte[] transaction, bool consensus, CancellationToken token);
}
ScriptEngine --> Stack : stack
ScriptEngine --> Stack : alt-stack
Stack *-- StackElement
@enduml
```