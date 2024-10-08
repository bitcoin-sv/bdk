/************************************************************** *
*  Date             20/01/2020                                  *
*  Author           Chi Thanh NGUYEN                            *
*                                                               *
*  Copyright (c) 2020 nChain Limited. All rights reserved       *
****************************************************************/

#ifndef BDK_CORE_VERSION_H
#define BDK_CORE_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

///!  Version of Bitcoin SV on which BDK has been built
extern const int BSV_CLIENT_VERSION_MAJOR ;
extern const int BSV_CLIENT_VERSION_MINOR ;
extern const int BSV_CLIENT_VERSION_REVISION ;
extern const char* BSV_VERSION_STRING ;

extern const char* BSV_GIT_COMMIT_TAG_OR_BRANCH ;
extern const char* BSV_GIT_COMMIT_HASH ;
extern const char* BSV_GIT_COMMIT_DATETIME ;

///!  Global version of Bitcoin Development Kit
extern const int BDK_VERSION_MAJOR ;
extern const int BDK_VERSION_MINOR ;
extern const int BDK_VERSION_PATCH ;
extern const char* BDK_VERSION_STRING ;

extern const char* SOURCE_GIT_COMMIT_TAG_OR_BRANCH ;
extern const char* SOURCE_GIT_COMMIT_HASH ;
extern const char* SOURCE_GIT_COMMIT_DATETIME ;
extern const char* BDK_BUILD_DATETIME_UTC ;

#ifdef __cplusplus
}

#endif

#endif /* BDK_CORE_VERSION_H */
