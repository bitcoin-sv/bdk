# bscrypt_ide

## Flutter web setup
https://flutter.dev/docs/get-started/web

# Install regular flutter [https://flutter.dev/docs/get-started/install]
    git clone https://github.com/flutter/flutter.git -b stable
    cd flutter
    bin/flutter doctor
    #Add $flutter/bin to system path environment variable

# Update to use flutter web [https://flutter.dev/docs/get-started/web]
    flutter channel beta
    flutter upgrade
    flutter config --enable-web
    cd flutter
    git remote get-url origin
    flutter pub global activate protoc_plugin
    flutter pub global activate junitreport
    #Add Pub/Cache/bin to system path environment variable
    flutter doctor


## IntelliJ setup
Install IntelliJ
Install plugin flutter in IntelliJ
Open project (flutter) in IntelliJ

## Material buttons
https://material.io/resources/icons/?style=baseline

## Extend stateful widget
https://stackoverflow.com/questions/50696945/flutter-statefulwidget-state-class-inheritance

## All material design widgets
https://flutter.github.io/samples/#/
https://flutter.dev/docs/development/ui/widgets/material