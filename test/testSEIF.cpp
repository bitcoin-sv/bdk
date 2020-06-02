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

BOOST_AUTO_TEST_CASE(test_case_1)
{
   static const uint8_t direct[] = {0x00, 0x6b, 0x54, 0x55, 0x93, 0x59,0x87};
    
    std::unique_ptr<unsigned char []> script(new unsigned char [sizeof(direct)]);
    for(int i=0; i<sizeof(direct); ++i){
        script.get()[i] = direct[i];
    }
    
    const size_t& directLen(sizeof(direct));
    
    BOOST_TEST(ScriptEngineIF::executeScript(script, directLen));

               
        
    std::string scriptString("0x00 0x6b 0x54 0x55 0x93 0x59 0x87");
    BOOST_TEST(ScriptEngineIF::executeScript(scriptString));
        
        
    std::string scriptHashExample ("'abcdefghijklmnopqrstuvwxyz' 0xaa 0x4c 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 0x88");
    BOOST_TEST(ScriptEngineIF::executeScript(scriptHashExample));
        
        
    std::string scriptHash ("'abcdefghijklmnopqrstuvwxyz' OP_HASH256 OP_PUSHDATA1 0x20 0xca139bc10c2f660da42666f72e89a225936fc60f193c161124a672050c434671 OP_EQUALVERIFY");
    BOOST_TEST(ScriptEngineIF::executeScript(scriptHash));
}

BOOST_AUTO_TEST_SUITE_END()
