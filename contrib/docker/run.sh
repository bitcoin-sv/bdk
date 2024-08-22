#!/bin/bash

docker run --name bdk \
           --volumes-from vc_bdk \
           --privileged \
            --cap-add=SYS_ADMIN \
           -it \
           --rm \
           bdk:20.04

