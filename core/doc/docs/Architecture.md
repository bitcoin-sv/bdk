Bscrypt C++ architecture is a 'plugin' architecture. It mainly contain
 - The core part that compile all script's functionalities in Bitcoin source code. Core is considered as low level implementation that will be reused by different modules. It is possible to add some extra functionalities to core.
 - The modules part where each module is considerated as an extened implementation from user's level. Modules should depend to core only, and should be independent each other.

The source code should be organised as follow

```
development
|---build
|---sv
|---bscrypt
|    |---core
|    |---modules
|         |---example_module
|         |---java
|         |---python
|    |---test
|         |---cpp
|         |---java
|         |---python
```

#### Add core's extra functionalities
To add extra functionalities in the core in order to be commonly reused by different modules later, you just need to drop *hpp* and *cpp* files in the core directory. CMake will detect them and add to the compilation of bscrypt-core.

Don't forget to  test them by adding a test in test/cpp

