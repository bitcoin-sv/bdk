import 'package:flutter/material.dart';

import 'package:bscrypt_ide/ide_top_bar_title.dart';
import 'package:bscrypt_ide/ide_script_editor.dart';
import 'package:bscrypt_ide/ide_script_stack.dart';
import 'package:bscrypt_ide/ide_bottom_bar_content.dart';

import 'package:bscrypt_ide/mock_prefetch_data.dart';

typedef DebugCallback = void Function(String args); // DebugCallback type to popup a dialog

class BscryptIDE extends StatefulWidget {
  BscryptIDE({
    Key key,
  }) : super(key: key);

  Widget top_bar_tilte;
  Widget script_editor;
  Widget script_stack;
  Widget bottom_bar_content;

  DebugCallback popupDebugDialog; // Only for debug

  @override
  _BscryptIDEState createState() => _BscryptIDEState();
}

class _BscryptIDEState extends State<BscryptIDE> {

  @override
  Widget build(BuildContext context) {
    widget.popupDebugDialog = (String some_str) {
      showDialog(
        context: context,
        builder: (x) =>
            AlertDialog(
              title: Text('TODO'),
              content: Text(some_str),
            ),
      );
    };

    return Scaffold(
      appBar: AppBar(
        title: _initIDETopBarTitle(),
        actions: <Widget>[
          IconButton(
            icon: const Icon(Icons.settings),
            tooltip: 'Settings',
            onPressed: () {
              widget.popupDebugDialog(
                  'Dropdown a list of settings : \n  Execution speed\n  Report Bug\n  Support\n  About');
            },
          ),
        ],
      ),
      body: Container(
        margin: EdgeInsets.symmetric(vertical: 8.0),
        constraints: BoxConstraints.expand(),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: <Widget>[
            Flexible(flex: 1, child: _initIDEScriptEditor()),
            VerticalDivider(),
            Flexible(flex: 1, child: _initIDEScriptStack()),
          ],
        ),
      ),
      floatingActionButton: FloatingActionButton(
        child: const Icon(Icons.play_arrow),
        tooltip: 'Quick execution',
        onPressed: () {
          widget.popupDebugDialog(
              'Clear stack, resend the http request and replay the stack evolution');
        },
        backgroundColor: Colors.green,
      ),
      bottomNavigationBar: BottomAppBar(
        child: _initIDEBottomBarContent(),
      ),
    );
  }

  /// Build component widgets for the main IDE /////////////////////////////////////////////////
  Widget _initIDETopBarTitle() {
    widget.top_bar_tilte = IDETopBarTitle(
      onBuildEvaluationsPressed: pressedBuildEvaluations,
      onStepBackwardPressed: pressedStepBackward,
      onStepForwardPressed: pressedStepForward,
      onRunPressed: pressedRun,
      flagStringList: mockListFlagStr,
    );
    return widget.top_bar_tilte;
  }

  Widget _initIDEBottomBarContent() {
    widget.bottom_bar_content = IDEBottomBarContent();
    return widget.bottom_bar_content;
  }

  Widget _initIDEScriptEditor() {
    widget.script_editor = IDEScriptEditor();
    return widget.script_editor;
  }

  Widget _initIDEScriptStack() {
    widget.script_stack = IDEScriptStack();
    return widget.script_stack;
  }

  /// Those are callback to handlers pressed button event from Top Bar ////////////////////////
  void pressedBuildEvaluations() {
    widget.popupDebugDialog(
        'Http send script to get the stack evolutions. Might need to clear the actual stack (or not? think about it)');
  }

  void pressedStepBackward() {
    widget.popupDebugDialog(
        'step backward 1 step to see how the stack changes');
  }

  void pressedStepForward() {
    widget.popupDebugDialog('step forward 1 step to see how the stack changes');
  }

  void pressedRun() {
    widget.popupDebugDialog('step forward to the next breakpoint position');
  }
}