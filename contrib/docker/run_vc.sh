docker run -it \
	--mount type=bind,src=/home/ads.nchain.com/c.gibson/bb/bscrypt,dst=/root/bscrypt \
	--name vc_bscrypt ubuntu:20.04 /bin/bash
