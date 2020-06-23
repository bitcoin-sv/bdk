/// Define test module name with debug postfix
/// Use it as an example how to add a test module
#ifdef NDEBUG 
#  define BOOST_TEST_MODULE test_SEIF
#else
#  define BOOST_TEST_MODULE test_SEIFd
#endif

#include <memory>
#include <string>
#include <boost/test/unit_test.hpp>
#include <ScriptEngineIF.h>

BOOST_AUTO_TEST_SUITE(test_scriptengine_suite)
//BOOST_FIXTURE_TEST_SUITE(test_scriptengine_suite,BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_case_1)
{
    const uint8_t direct[] = {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    
    BOOST_CHECK_EQUAL(ScriptEngineIF::executeScript(direct,true,0,std::string(),0,0), SCRIPT_ERR_OK);
        
    std::string scriptString("0x00 0x6b 0x54 0x55 0x93 0x59 0x87");
    BOOST_CHECK_EQUAL(ScriptEngineIF::executeScript(scriptString, true, 0, std::string(),0,0), SCRIPT_ERR_OK);
        
        
    std::string scriptHashExample ("'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
    BOOST_CHECK_EQUAL(ScriptEngineIF::executeScript(scriptHashExample,true, 0,std::string(),0,0), SCRIPT_ERR_OK);
        
        
    std::string scriptHash ("'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
    BOOST_CHECK_EQUAL(ScriptEngineIF::executeScript(scriptHash,true, 0,std::string(),0,0), SCRIPT_ERR_OK);
}


BOOST_AUTO_TEST_CASE(test_case_2)
{
    // Please refer to the code in TestTxKeys.cpp to see where the generated data came from
    const std::string& fullscript = "0x47 0x304402201fcefdc44242241964b5ca81a7e480364364b11a7f5a90163c42c0db3f3886140220387c073f39d63f60deb93b7935a84b93eb498fc12fbe3d65551b905fc360637b01 0x41 0x040b4c866585dd868a9d62348a9cd008d6a312937048fff31670e7e920cfc7a7447b5f0bba9e01e6fe4735c8383e6e7a3347a0fd72381b8f797a19f694054e5a69 DUP HASH160 0x14 0xff197b14e502ab41f3bc8ccb48c4abac9eab35bc EQUALVERIFY CHECKSIGVERIFY";
    
    const std::string& txhex = "0100000001d92670dd4ad598998595be2f1bec959de9a9f8b1fd97fb832965c96cd55145e20000000000ffffffff010a000000000000000000000000";

    int64_t amt = 10; 
    
    BOOST_CHECK_EQUAL(ScriptEngineIF::executeScript(fullscript,true, 0,txhex,0,amt),SCRIPT_ERR_OK);
}


BOOST_AUTO_TEST_SUITE_END()
