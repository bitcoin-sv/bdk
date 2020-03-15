// https://flutter.dev/docs/cookbook/networking/fetch-data
// https://medium.com/@diegoveloper/flutter-fetching-parsing-json-data-c019ddddaa34
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async';
import 'dart:convert';

void main() => runApp(MyApp());

class MyApp extends StatelessWidget {
  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter web demo',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: MyHomePage(title: 'Test connectivity to http CROSS server'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  MyHomePage({Key key, this.title}) : super(key: key);

  final String title;

  @override
  _MyHomePageState createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  String httpReply = '';

  _fetchReply() async {
    //final url = 'https://www.httpvshttps.com';
    //final url = 'https://jsonplaceholder.typicode.com/photos';
    final url = 'http://127.0.0.1:8080';

    http.get(url).then((response){
      if (response.statusCode == 200) {
        setState(() {
          httpReply = response.body;
        });
      } else {
        print('Response with error');
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            httpReply.length < 1 ? Text("Click to trigger http request server") :Text(httpReply),
          ],
        ),
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _fetchReply,
        tooltip: 'Trigger http request',
        child: Icon(Icons.add),
      ), // This trailing comma makes auto-formatting nicer for build methods.
    );
  }
}