/************************************************************** *
*  Date             04/08/2020                                  *
*  Author           Chi Thanh NGUYEN                            *
*                                                               *
*  Copyright (c) 2020 nChain Limited. All rights reserved       *
****************************************************************/

#ifndef BDK_GOLANG_VERSION_H
#define BDK_GOLANG_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

///!  Version of Bitcoin SV on which BDK has been built
extern const int CGO_BSV_CLIENT_VERSION_MAJOR ;
extern const int CGO_BSV_CLIENT_VERSION_MINOR ;
extern const int CGO_BSV_CLIENT_VERSION_REVISION ;
extern const char* CGO_BSV_VERSION_STRING ;

extern const char* CGO_BSV_GIT_COMMIT_TAG_OR_BRANCH ;
extern const char* CGO_BSV_GIT_COMMIT_HASH ;
extern const char* CGO_BSV_GIT_COMMIT_DATETIME ;

///!  Global version of Bitcoin Development Kit
extern const int CGO_BDK_VERSION_MAJOR ;
extern const int CGO_BDK_VERSION_MINOR ;
extern const int CGO_BDK_VERSION_PATCH ;
extern const char* CGO_BDK_VERSION_STRING ;

extern const char* CGO_SOURCE_GIT_COMMIT_TAG_OR_BRANCH ;
extern const char* CGO_SOURCE_GIT_COMMIT_HASH ;
extern const char* CGO_SOURCE_GIT_COMMIT_DATETIME ;
extern const char* CGO_BDK_BUILD_DATETIME_UTC ;

///!  Version of Golang Bitcoin Development Kit
extern const int BDK_GOLANG_VERSION_MAJOR ;
extern const int BDK_GOLANG_VERSION_MINOR ;
extern const int BDK_GOLANG_VERSION_PATCH ;
extern const char* BDK_GOLANG_VERSION_STRING ;

#ifdef __cplusplus
}
#endif

#endif /* BDK_GOLANG_VERSION_HPP */
