#!/bin/bash

docker run --name bscrypt \
           --volumes-from vc_bscrypt \
           --privileged \
            --cap-add=SYS_ADMIN \
           -it \
           --rm \
           bscrypt:20.04

