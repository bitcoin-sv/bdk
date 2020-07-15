## Script engine object model

Object model is an approach to implement scripts functionalities in a object oriented fashion. The main function in script engine is the evaluation of the script. Other classes provide facilities to hold additional/auxiliare data to this function.

 - **Status** class : hold the result of script's evaluation, i.e its return code and error message if any.
 - **Assembler** class : Implement method that convert a binary script to string script (the underlying data of a script is a binary array)
 - **ScriptEngine** class : hold the script configuration and script's evaluation flag switch. These determine how script should be evaluated. It has the execute method to evaluate a script

```plantuml
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
```
