## Bscrypt object model

Object model is an approach to implement scripts functionalities in a object oriented fashion. The main function in script engine is the evaluation of the script. Other classes provide facilities to hold additional/auxiliare data to this function.

 - **config** class : hold all script's configuration prior to its evaluation.
 - **ExecutionStatus** class : hold the result of script's evaluation, i.e its return code and error message if any.
 - **Assembler** class : Implement method that convert a binary script to string script (the underlying data of a script is a binary array)
 - **ScriptEngine** class : hold the script configuration and script's evaluation flag switch. These determine how script should be evaluated. It has the execute method to evaluate a script
 - **Stack** class : hold the stack result of script evaluation.

```plantuml
@startuml

package "c++ Interface" {
    namespace bsv {
        class Assembler {
            +CScript bsv::from_asm(const std::string& script);
            +std::string bsv::to_asm(const bsv::span<const uint8_t> script)
        }


        class Interpreter {
            +ScriptError evaluate(bsv::span<const uint8_t> script, bool consensus, unsigned int flags, const std::string& transaction, int tx_input_index, int64_t amount);
            +ScriptError evaluate(const std::string& script, bool consensus, unsigned int flags, const std::string& transaction, int tx_input_index, int64_t amount);
            +std::string formatScript(const std::string& script);

            -unique_sig_checker make_unique_sig_checker()
            -unique_sig_checker make_unique_sig_checker(const CTransaction& tx, const int vinIndex, const int64_t a)

            -ScriptError evaluate_impl(const CScript& script, const bool consensus, const unsigned int scriptflag, BaseSignatureChecker* sigCheck)
            -ScriptError evaluate_impl(const CScript& script, const bool consensus, const unsigned int scriptflag, const std::string& txhex, const int vinIndex, const int64_t amount)
        }
    }
}

package "Java Interface" {
    class ScriptEngine {
            +ScriptEngine()
            +Status evaluate(byte[] script,boolean concensus, int scriptflags,String txHex, int index, int amount);
            +Status evaluateString(String script,boolean concensus, int scriptflags, String txHex, int index, int amount);
    }

    class Status {
         -int statusCode;
         -String statusMessage;

         +Status(int statusCode, String statusMessage);

         +int getStatusCode()
         +void setStatusCode(int statusCode)
         +String getStatusMessage()
         +void setStatusMessage(String statusMessage)
    }


    class Assembler {
        +byte[] fromAsm(String script);
        +String toAsm(byte[] script);
    }
}

package "Python Interface" {
    class PySESDK {
        +ScriptError ExecuteScript(string script, int concensus, unsigned int scriptflags, string hextx, int index, int64_t amount;
    }

}
@enduml
```
