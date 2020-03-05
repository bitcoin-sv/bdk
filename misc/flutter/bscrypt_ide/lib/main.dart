import 'package:flutter/material.dart';

import 'package:bscrypt_ide/bscrypt_ide.dart';

// Help to show dialog debug
//showDialog( context: context, builder: (_) => new AlertDialog( title: new Text("TODO"), content: new Text("Button is clicked"),));

void main() => runApp(MaterialApp(
    debugShowCheckedModeBanner: false,
    title: 'bscrypt IDE',
    home: BscryptIDE()));