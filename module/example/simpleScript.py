 #!/usr/bin/python
from io import BytesIO
from random import randint
import binascii

import PyScriptEngine

if __name__ == "__main__":
    print ("starting.....")
    
    script = "'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"
    
    print(script)
    #if PyScriptEngine.ExecuteScript(script,True,0):
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from python")

    scriptVal = '87 81 84 81 81 83 81 87 81 84 81 81 0 107 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 99 83 147 86 135 105 108 139 107 104 108 82 162 105 108 139 107 104 99 0 107 99 85 147 89 135 105 108 139 107 104 99 85 148 82 135 105 108 139 107 104 108 82 162 105 108 139 107 104 108 82 162'
    
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from python..Accumulator multisig")


    
    
    script = "OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from locking script")
        
        
        
     #result = PyScriptEngine.ExecuteScript("HASH160 0x14 0x8febbed40483661de6958d957412f82deed8e2f7 EQUAL") #0
    #result = PyScriptEngine.ExecuteScript(
     #   "HASH256 0x20 0x5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456 EQUAL") #0
     
    script = "'b464e85df2a238416f8bdae11d120add610380ea07f4ef19c5f9dfd472f96c3d' OP_HASH160 OP_PUSHDATA1 0x14 0xbef80ecf3a44500fda1bc92176e442891662aed2 EQUAL"
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from locking script OP_HASH160 test")
        
    script = "'' RIPEMD160 OP_PUSHDATA1 0x14 0x9c1185a5c5e9fc54612808977ee8f548b2258d31 EQUAL"
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from locking script OP_HASH160 test empty string")     
        
        
    script = "15 9 OP_SUB 5 OP_ADD 16 OP_EQUAL"
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Sucesss from final test script with 15 instead of OP_15")
        
        
    script = "0x47 0x304402206e05a6fe23c59196ffe176c9ddc31e73a9885638f9d1328d47c0c703863b8876022076feb53811aa5b04e0e79f938eb19906cc5e67548bc555a8e8b8b0fc603d840c01 0x21 0x038282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23fd5f51508 DUP HASH160 0x14 0x1018853670f9f3b0582c5b9ee8ce93764ac32b93 EQUALVERIFY CHECKSIG VERIFY"
    
    if PyScriptEngine.ExecuteScript(script,True,0,"",0,0):
        print("Success of pay to script hash ... no transaction")
        
        
    fullscript = "0x47 0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f3886140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc360637b01 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY"
    
    txhex = "0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000"

    amt = 10; 
    if PyScriptEngine.ExecuteScript(fullscript,True,0,txhex,0,amt) ==0:
        print("Sucesss from python...evaluate full pay to script hash script with a transaction")
    
    
    scriptsig = "0x47 0x30440220543a3f3651a0409675e35f9e77f5e364214f0c2a22ae5512ec0609bd7a825b4c02206204c137395065e1a53fc0e2d91e121c9210e72a603f53221b531c0816c7f60701 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69"
    scriptpubkey = "DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIG";
    
    if PyScriptEngine.VerifyScript(scriptsig,scriptpubkey,True,0,txhex,0,amt) == 0:
        print("Sucesss from python... verify full pay to script hash script with a transaction")
        
    print ("...Finsihed ...")
