Test connectivity https client/server

client implement in Dart (flutter web)
server implement in C++ with boost::beast (sync/async?).

A simple hello world to test connectivity:
  Client is a web app, with a clickable button. When the button is clicked, it will send out a simple http request to the server
  The sever will send back a text ACK to the client, and the client app will display it on the screen

Build and Run:
 - build client : follow instruction on the ./client/README
 - build server : follow instructions in the ./server/CMakeLists.txt
 - run the server,  then run the client

Notes :
 - The server has 4 programs, but only http works. The others are just there at the moment. So use http.exe as the local server
 - There are 3 other variants for https, async. But they are not functional yet -> TODO

IMPORTANCES : 
 The http reply from server has to set the header 'access_control_allow_origin' field with the value is the one in the field origin of the request's header.
 Explanation :
   The client is on its own a web/app. It action to request data from other website it is a CORS
      https://en.wikipedia.org/wiki/Cross-origin_resource_sharing
   The http server in C++ thus has to add the field access_control_allow_origin. Its value should be the original web that request to it, which is in the
   header of the request, i.e field 'origin'.
   See detail in ./server/http.cpp, in the end of the method 'handle_request'.

