

/// Define test module name with debug postfix
#ifdef NDEBUG 
#  define BOOST_TEST_MODULE test_http_msg
#else
#  define BOOST_TEST_MODULE test_http_msgd
#endif

#include <boost/test/unit_test.hpp>

#include <http_msg.pb.h>

BOOST_AUTO_TEST_SUITE(test_bscrypt_ide_http_msg)

BOOST_AUTO_TEST_CASE(test_bscrypt_ide_http_msg_MetaMsg)
{
    http_msg::MetaMsg msg;
    msg.set_msg_type("type");
    msg.set_serialized_data("data");
    BOOST_TEST(msg.msg_type() == "type");
    BOOST_TEST(msg.serialized_data() == "data");
}

BOOST_AUTO_TEST_CASE(test_bscrypt_ide_http_msg_StackOp)
{
    http_msg::StackOp msg;
    msg.set_op_type(http_msg::StackOp_StackOpType_NOTHING);
    msg.set_data("data");
    BOOST_TEST(msg.op_type() == http_msg::StackOp_StackOpType_NOTHING);
    BOOST_TEST(msg.data() == "data");
}

BOOST_AUTO_TEST_CASE(test_bscrypt_ide_http_msg_ScriptEvalRequest)
{
    http_msg::ScriptEvalRequest msg;
    msg.set_hash("0x01aabb");
    msg.add_init_stack("0x01");
    msg.add_init_stack("0x02");
    msg.add_tokens("OP_PUSH");
    msg.add_tokens("0x01");
    BOOST_TEST(msg.init_stack_size() == 2);
    BOOST_TEST(msg.tokens_size() == 2);
}

BOOST_AUTO_TEST_CASE(test_bscrypt_ide_http_msg_ScriptEvalReply)
{
    http_msg::ScriptEvalReply msg;
    msg.set_hash("0x01aabb");
    auto op1= *msg.add_ops();
    op1.set_op_type(http_msg::StackOp_StackOpType_PUSH);
    op1.set_data("0x01");
    auto op2 = *msg.add_ops();
    op2.set_op_type(http_msg::StackOp_StackOpType_NOTHING);

    BOOST_TEST(msg.ops_size() == 2);
}

BOOST_AUTO_TEST_SUITE_END()