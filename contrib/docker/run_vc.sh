docker run -it \
	--mount type=bind,src=/home/ads.nchain.com/c.gibson/bb/bdk,dst=/root/bdk \
	--name vc_bdk ubuntu:22.04 /bin/bash
