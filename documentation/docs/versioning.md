# Bitcoin Development Kit Versioning

This document contains a description of the proposed versioning strategy for the Bitcoin Development Kit.

The BDK consists of a number of components
* The BDK "core" which contains core code taken over from SV, and common code used by lnaguage bindings.
* Language bindings. 

The object model can be found in documentation/docs/ObjectModel.md

For the purpose of this document, we assume that bindings are present for Java and Python.

## Proposal
* Each language binding (module) will be assigned a version number, following sematic versioning rules (https://semver.org/)
* The SDK "core" will be assigned a version number, following sematic versioning rules.
* The SDK will be assigned an overall version number.

"Bumping" a version means incrementing the patch, minor or major version, and reseting the lesser versions to 0.

The following rules will apply to the assignment of versions, and is consistent with semantic versioning rules.
* If the version number of a language binding is "bumped", then the version number of the SDK itself is bumped in a similar manner.
* If the version number of core is bumped, then the version number of the language bindings is also "bumped". To avoid detailed dependency analysis, the language binding version may be bumped even if the change to core does not directly effect the language binding.
* Only a single "most significant" bump is ever applied to the SDK. 

The SDK version is always greater than the versions of any of its components.

### Examples
If the patch versions of 2 modules (bindings or core) are incremented, then the SDK patch version is incremented. If the minor version of a module is incremented, and the patch version of another is incremented, then the SDK minor version is incremented and the patch version is reset.

If the Java binding version is bumped from "1.2.3" to "1.3.0", the SDK minor version is incremented and the patch number reset, say, from "1.4.3" to "1.5.0".

If the Java binding version is bumped from "1.2.3" to "1.3.0" and the Python binding version is bumped from "1.1.6" to "1.1.7", the bump to the Java binding is more significant and the SDK minor version is incremented and the patch number reset, from  "1.4.3" to "1.5.0".

## Dependency on SV

The Bitcoin Development Kit is built against an instance of the SV source, however that instance of the SV source does not need to keep pace with SV releases or SV development branches.
The build process pulls in a version of the SV source (release or development branches), and that code is labelled with the SDK version.
I.e. The version of the SV source used does not directly affect the SDK version.

Internal use: SDK instances built against development branches may have the strings "develop" or "RC"+integer appended to their versions. E.g. The SDK version "1.5.6-RC2" would be 2nd release candidate for "1.5.6".

## When should I upgrade the SDK?

Generally you should consult README.md to determine if the changes to the SDK will affect you.

* If the versions of core and the language binding you are using has not changed, then you do not need to upgrade.
