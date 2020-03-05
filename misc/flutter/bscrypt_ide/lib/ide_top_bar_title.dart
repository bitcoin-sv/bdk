import 'package:flutter/material.dart';

class FlagStatus {
  FlagStatus({
    @required this.flag_str,
    this.status = true,
  });

  final String flag_str;
  bool status;
}

class IDETopBarTitle extends StatefulWidget {
  IDETopBarTitle({
    Key key,
    @required this.onBuildEvaluationsPressed,
    @required this.onStepBackwardPressed,
    @required this.onStepForwardPressed,
    @required this.onRunPressed,
    @required this.flagStringList,
  })  : assert(onBuildEvaluationsPressed != null),
        assert(onStepBackwardPressed != null),
        assert(onStepForwardPressed != null),
        assert(onRunPressed != null),
        super(key: key)
  {

  }

  final VoidCallback onBuildEvaluationsPressed;
  final VoidCallback onStepBackwardPressed;
  final VoidCallback onStepForwardPressed;
  final VoidCallback onRunPressed;
  final List<String> flagStringList;

  @override
  _IDETopBarTitleState createState() => _IDETopBarTitleState();
}

class _IDETopBarTitleState extends State<IDETopBarTitle> {
  bool _synced_status; //TODO : When user click build script button, it fetche the data and set the synced_status to true --> to be used
  List<FlagStatus> flagStringList;

  @override
  Widget build(BuildContext context) {
    /// Initialize flagStringList only if it is not initialized
    if (this.flagStringList == null || this.flagStringList.length == 0) {
      this.flagStringList = initFlagStatusList(widget.flagStringList);
    }
    Widget flagCheckButton = buildFlagSettingButton(context);

    return Row(
      children: <Widget>[
        IconButton(
          // TODO move it to bottom bar
          icon: Image.asset('assets/icons/logo.ico'),
          tooltip: 'Bitcoin SV',
          onPressed: () {
            //TODO
          },
        ),
        flagCheckButton,
        IconButton(
          icon: const Icon(Icons.sync),
          tooltip: 'Build evaluations',
          onPressed: widget.onBuildEvaluationsPressed,
        ),
        IconButton(
          icon: const Icon(Icons.navigate_before),
          tooltip: 'Step backward',
          onPressed: widget.onStepBackwardPressed,
        ),
        IconButton(
          icon: const Icon(Icons.navigate_next),
          tooltip: 'Step forward',
          onPressed: widget.onStepForwardPressed,
        ),
        IconButton(
          icon: const Icon(Icons.last_page),
          tooltip: 'Play to next break point',
          onPressed: widget.onRunPressed,
        ),
      ],
    );
  }

  Widget buildFlagSettingButton(BuildContext context) {
    return PopupMenuButton<int>(
      icon: const Icon(Icons.playlist_play),
      tooltip: 'Customize script flags',
      onSelected: handleScriptFlagChange,
      itemBuilder: (context) {
        var popup_list =
            List<CheckedPopupMenuItem<int>>(this.flagStringList.length);
        for (var i = 0; i < this.flagStringList.length; i++) {
          popup_list[i] = CheckedPopupMenuItem<int>(
            value: i,
            checked: this.flagStringList[i].status,
            child: Text(this.flagStringList[i].flag_str),
          );
        }
        return popup_list;
      },
    );
  }

  void handleScriptFlagChange(int iChanged) {
    setState(() {
      this.flagStringList[iChanged].status =
          !(this.flagStringList[iChanged].status);
    });
  }

  List<FlagStatus> initFlagStatusList(str_list) {
    if (str_list == null) return List<FlagStatus>(0);
    var result = List<FlagStatus>(str_list.length);
    for (var i = 0; i < str_list.length; i++) {
      result[i] = FlagStatus(
        flag_str: str_list[i],
        status: true,
      );
    }
    return result;
  }
}
