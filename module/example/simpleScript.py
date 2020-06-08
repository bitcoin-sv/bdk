 #!/usr/bin/python
from io import BytesIO
from random import randint
import binascii

import PyScriptEngine

if __name__ == "__main__":
    print ("starting.....")
    
    script = "'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"
    
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from python")

    scriptVal = '87 81 84 81 81 83 81 87 81 84 81 81 0 107 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 99 83 147 86 135 105 108 139 107 104 108 82 162 105 108 139 107 104 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 108 82 162 105 108 139 107 104 108 82 162'
    
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from python..Accumulator multisig")


    
    
    script = "OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from locking script")
        
        
        
     #result = PyScriptEngine.ExecuteScript("HASH160 0x14 0x8febbed40483661de6958d957412f82deed8e2f7 EQUAL") #0
    #result = PyScriptEngine.ExecuteScript(
     #   "HASH256 0x20 0x5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456 EQUAL") #0
     
    script = "'b464e85df2a238416f8bdae11d120add610380ea07f4ef19c5f9dfd472f96c3d' OP_HASH160 OP_PUSHDATA1 0x14 0xbef80ecf3a44500fda1bc92176e442891662aed2 EQUAL"
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from locking script OP_HASH160 test")
        
    script = "'' RIPEMD160 OP_PUSHDATA1 0x14 0x9c1185a5c5e9fc54612808977ee8f548b2258d31 EQUAL"
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from locking script OP_HASH160 test empty string")     
        
        
    script = "15 9 OP_SUB 5 OP_ADD 16 OP_EQUAL"
    if PyScriptEngine.ExecuteScript(script):
        print("Sucesss from final test script with 15 instead of OP_15")
        
        
    print ("...Finsihed ...")
