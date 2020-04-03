// This is a basic Flutter widget test.
//
// To perform an interaction with a widget in your test, use the WidgetTester
// utility that Flutter provides. For example, you can send tap and scroll
// gestures. You can also use WidgetTester to find child widgets in the widget
// tree, read text, and verify that the values of widget properties are correct.

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:bscrypt_ide/http_msg.pb.dart';
import 'package:bscrypt_ide/http_msg.pbenum.dart';
import 'package:bscrypt_ide/http_msg.pbjson.dart';
import 'package:bscrypt_ide/http_msg.pbserver.dart';

void main() {
  group('http_msg dart', () {
    test('MetaMsg', () {
      MetaMsg msg = MetaMsg();
      expect(msg.hasMsgType(), false);
      expect(msg.hasSerializedData(), false);
      msg.msgType = 'test msg type';
      msg.serializedData = [0x01, 0x02, 0x03];
      expect(msg.msgType, 'test msg type');
      expect(msg.serializedData, [0x01, 0x02, 0x03]);
      expect(msg.hasMsgType(), true);
      expect(msg.hasSerializedData(), true);
    });

    test('StackOp', () {
      StackOp msg = StackOp();
      expect(msg.hasOpType(), false);
      expect(msg.hasData(), false);
      msg.opType = StackOp_StackOpType.NOTHING;
      msg.data = [0x01, 0x02, 0x03];
      expect(msg.opType, StackOp_StackOpType.NOTHING);
      expect(msg.data, [0x01, 0x02, 0x03]);
      expect(msg.hasOpType(), true);
      expect(msg.hasData(), true);
    });

    test('ScriptEvalRequest', () {
      ScriptEvalRequest msg = ScriptEvalRequest();
      msg.hash = '0x010abf';
      msg.initStack.addAll(['0x01', '0x02', '0x03']);
      msg.tokens.addAll(['OP_PUSH', '0x01']);
      expect(msg.initStack, ['0x01', '0x02', '0x03']);
      expect(msg.tokens, ['OP_PUSH', '0x01']);
    });

    test('ScriptEvalReply', () {
      ScriptEvalReply msg = ScriptEvalReply();
      msg.hash = '0x010abf';
      StackOp op1 = StackOp();
      op1.opType = StackOp_StackOpType.PUSH;
      op1.data = [0x01, 0x02, 0x03];
      StackOp op2 = StackOp();
      op2.opType = StackOp_StackOpType.NOTHING;

      msg.ops.addAll([op1,op2]);
      expect(msg.ops.length, 2);
    });
  });
}
