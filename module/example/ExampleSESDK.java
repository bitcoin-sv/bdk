package com.nchain.bsv.scriptengine.*;

public class ExampleSESDK {
  public static void main(String[] args){
    int[] intArray = new int[] {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    final byte[] var = new byte[intArray.length];
    for (int i = 0; i < intArray.length; i++){
        var[i] = (byte) intArray[i];
    }

    ScriptEngine scriptEngine = new ScriptEngine();
    if(scriptEngine.evaluate(var,true,0,"",0,0).getCode() == 0){
        System.out.println("...Successful script evaluation");
    }else{
        System.out.println("...Failure of script evaluation");
    }

    
    String ScriptArray = new String ("0x00 0x6b 0x54 0x55 0x93 0x59 0x87");
    if(scriptEngine.evaluate(ScriptArray,true, 0,"",0,0).getCode() == 0){
        System.out.println("...Successful script evaluation from String Type");
    }else{
        System.out.println("...Failure of script evaluation from String Type");
    }
    
    
    String PubKeyStyle = new String("'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
    if(scriptEngine.evaluate(PubKeyStyle,true, 0,"",0,0).getCode() == 0){
        System.out.println("...Successful script evaluation from String Type PubKey Style");
    }else{
        System.out.println("...Failure of script evaluation from String Type PubKey Style");
    }
    
    PubKeyStyle = new String ("'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
    if(scriptEngine.evaluate(PubKeyStyle,true, 0,"",0,0).getCode() == 0){
        System.out.println("...Successful script evaluation from String Type PubKey Style with op codes");
    }else{
        System.out.println("...Failure of script evaluation from String Type PubKey Style with op codes");
    }
    
    
     String pkh = new String ("0x47 0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f3886140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc360637b01 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY");
	     
    String HexID = new String ("0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000");
	     
    int amt = 10; 
    if(scriptEngine.evaluate(pkh,true,0,HexID,0,amt).getCode() == 0){
        System.out.println("...Successful script evaluation Pubkey,signature & tx info");
    }else{
        System.out.println("...Failure script evaluation Pubkey,signature & tx info");
    }

  }//End of main
}//End of FirstJavaProgram Class

