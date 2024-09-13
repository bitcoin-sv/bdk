#### Directories
```
|---build
|---bdk
|    |---core
|    |---documentation
|    |---bitcoin-sv
|    |---modules
|         |---example
|         |---java
|         |---python
|         |---...
|    |---test
|         |---cpp
|         |---java
|         |---python
|         |---...
```

 - 'sv' should contain a checkout of the SV codebase. 
 - 'core' contains files required to build the BDK as a stand-alone component from the SV codebase.
 - 'modules' contains language wrappers for the BDK component. Modules should only depend on core only and should be independent of each other.

#### Add core's extra functionalities
To add functionality to core, drop *h* and *cpp* files into the core directory. CMake will detect them and add them to the build.
Don't forget to test them by adding a test in test/cpp

