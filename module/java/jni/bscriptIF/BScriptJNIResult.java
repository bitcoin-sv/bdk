package com.nchain.bsv.scriptengine;

public class BScriptJNIResult {

    private int statusCode;
    private String statusMessage;

    public BScriptJNIResult(int statusCode, String statusMessage){
        this.statusCode = statusCode;
        this.statusMessage = statusMessage;
    }

    public BScriptJNIResult(){
    }

    public int getStatusCode(){
        return statusCode;
    }

    public void setStatusCode(int statusCode){
        this.statusCode = statusCode;
    }

    public String getStatusMessage(){
        return statusMessage;
    }

    public void setStatusMessage(String statusMessage){
        this.statusMessage = statusMessage;
    }

}
    
