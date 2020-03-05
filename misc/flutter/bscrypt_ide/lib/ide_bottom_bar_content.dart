import 'package:flutter/material.dart';

class IDEBottomBarContent extends StatefulWidget {
  IDEBottomBarContent({
    Key key,
  }) : super(key: key);

  @override
  _IDEBottomBarContentState createState() => _IDEBottomBarContentState();
}

class _IDEBottomBarContentState extends State<IDEBottomBarContent> {
  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.symmetric(horizontal: 8.0),
      child: Row(
        mainAxisSize: MainAxisSize.max,
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: <Widget>[
          Text('Bitcoin association'),
          Text('Chi Thanh NGUYEN'),
        ],
      ),
    );
  }
}
