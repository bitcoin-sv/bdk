import 'package:flutter/material.dart';

import 'dart:math'; // Generate random index for prefetch list

class IDEScriptStack extends StatefulWidget {
  IDEScriptStack({
    Key key,
  }) : super(key: key);

  @override
  _IDEScriptStackState createState() => _IDEScriptStackState();
}

class _IDEScriptStackState extends State<IDEScriptStack> {
  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.green[100],
      alignment: Alignment.center,
      child: _buildStackListView(),
      constraints: BoxConstraints.expand(),
    );
  }

  Widget _buildStackListView() {
    int maxNbLines = 8;
    return ListView.builder(
      reverse: true,
      itemCount: maxNbLines,
      itemBuilder: (BuildContext ctxt, int index) {
        var rng = Random.secure();
        var dec_val = (rng.nextInt(1000000000) + 1) * 999983;
        var hex_val = dec_val.toRadixString(16);
        return ListTile(
          contentPadding: EdgeInsets.symmetric(horizontal: 200.0),
          leading: RaisedButton(
            color: Colors.green,
            child: Text(index.toString()),
            onPressed: () {},
          ),
          title: Text('0x' + hex_val),
        );
      },
    );
  }
}
