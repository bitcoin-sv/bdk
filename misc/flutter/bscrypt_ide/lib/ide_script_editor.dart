import 'package:flutter/material.dart';

import 'dart:math'; // Generate random index for prefetch list
import 'package:bscrypt_ide/mock_prefetch_data.dart';

class LineNumberEditor extends StatefulWidget {
  LineNumberEditor({
    Key key,
    this.lineID,
  }) : super(key: key);

  final lineID;

  @override
  _LineNumberEditorState createState() => _LineNumberEditorState();
}

class _LineNumberEditorState extends State<LineNumberEditor> {
  bool _toogled = false;

  @override
  Widget build(BuildContext context) {
    Color toogleColor = Colors.black54;
    if (_toogled) toogleColor = Colors.red;
    return RaisedButton(
      color:toogleColor,
      child: Text(widget.lineID.toString()),
      onPressed: handleToogleAction,
    );
  }

  void handleToogleAction() {
    setState(() {
      this._toogled = !(this._toogled);
    });
  }
}

class IDEScriptEditor extends StatefulWidget {
  IDEScriptEditor({
    Key key,
  }) : super(key: key);

  @override
  _IDEScriptEditorState createState() => _IDEScriptEditorState();
}

class _IDEScriptEditorState extends State<IDEScriptEditor> {
  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black12,
      child: _buildListView(),
      constraints: BoxConstraints.expand(),
    );
  }

  Widget _buildListView() {
    int maxNbLines = 200;
    int nbNonEmptyLine = 50;
    return ListView.builder(
      itemCount: maxNbLines,
      itemBuilder: (BuildContext ctxt, int index) {
        String opcode_str='';
        if(index < nbNonEmptyLine) {
          var rng = new Random(index);
          int opcode_index = rng.nextInt(mockListOpCodeStr.length);
          opcode_str = mockListOpCodeStr[opcode_index];
        }

        return ListTile(
          leading: LineNumberEditor(lineID: index),
          title: Text(opcode_str),
        );
      },
    );
  }
}
