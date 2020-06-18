import jni.bscriptIF.*;

//java -Djava.library.path=<PATH TO SHARED LIBRARIES> -cp "<PATH TO JAR FILE>/BScriptJNI.jar" TestSEIF.java

public class TestSEIF {
  public static void main(String[] args){
    int[] intArray = new int[] {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    final byte[] var = new byte[intArray.length];
    for (int i = 0; i < intArray.length; i++){
        var[i] = (byte) intArray[i];
    }

    BScriptJNI jniIF = new BScriptJNI();
    if(jniIF.EvalScript(var).getStatusCode() == 0){
        System.out.println("...Successful script evaluation");
    }else{
        System.out.println("...Failure of script evaluation");
    }


    String[] ScriptArray = new String[] {"0x00 0x6b 0x54 0x55 0x93 0x59 0x87","0x00 0x6b 0x54 0x55 0x93 0x59 0x87"};
    if(jniIF.EvalScriptString(ScriptArray)[0].getStatusCode() == 0){
        System.out.println("...Successful script evaluation from String Type");
    }else{
        System.out.println("...Failure of script evaluation from String Type");
    }


    String[] PubKeyStyle = new String[]{"'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88"};
    if(jniIF.EvalScriptString(PubKeyStyle)[0].getStatusCode() == 0){
        System.out.println("...Successful script evaluation from String Type PubKey Style");
    }else{
        System.out.println("...Failure of script evaluation from String Type PubKey Style");
    }

    PubKeyStyle = new String[]{"'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY"};
    if(jniIF.EvalScriptString(PubKeyStyle)[0].getStatusCode() == 0){
        System.out.println("...Successful script evaluation from String Type PubKey Style with op codes");
    }else{
        System.out.println("...Failure of script evaluation from String Type PubKey Style with op codes");
    }
  }//End of main
}//End of FirstJavaProgram Class

