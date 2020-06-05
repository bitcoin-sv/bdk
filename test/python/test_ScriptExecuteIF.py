import PyScriptEngine

# Test to validate the getInfo def
def test_ScriptEngine():

    script = "'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"

    assert PyScriptEngine.ExecuteScript(script) == True 
    

    script = '87 81 84 81 81 83 81 87 81 84 81 81 0 107 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 99 83 147 86 135 105 108 139 107 104 108 82 162 105 108 139 107 104 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 108 82 162 105 108 139 107 104 108 82 162'
    
    assert PyScriptEngine.ExecuteScript(script) == True 
    
     
    script = "'b464e85df2a238416f8bdae11d120add610380ea07f4ef19c5f9dfd472f96c3d' OP_HASH160 OP_PUSHDATA1 0x14 0xbef80ecf3a44500fda1bc92176e442891662aed2 EQUAL"
    assert PyScriptEngine.ExecuteScript(script) == True 
   
    script = "'' RIPEMD160 OP_PUSHDATA1 0x14 0x9c1185a5c5e9fc54612808977ee8f548b2258d31 EQUAL"
    assert PyScriptEngine.ExecuteScript(script) == True

