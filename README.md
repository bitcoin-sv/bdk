To install flutter web

Install regular flutter [https://flutter.dev/docs/get-started/install]
  git clone https://github.com/flutter/flutter.git -b stable
  cd flutter
  bin/flutter doctor
  #Add $flutter/bin to system path environment variable

Update to use flutter web [https://flutter.dev/docs/get-started/web]
  flutter channel beta
  flutter upgrade
  flutter config --enable-web
  flutter pub global activate protoc_plugin
  #Add Pub/Cache/bin to system path environment variable
