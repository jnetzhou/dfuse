all:
	make -f ../Makefile -j8
	CC=arm-linux-gnueabi-gcc make -f ../Makefile.device
	#make -f ../Makefile.device
	../misc/adb push df_device /data/local/tmp
	../misc/adb shell chmod 777 /data/local/tmp/df_device

mrproper:
	make -f ../Makefile mrproper
	make -f ../Makefile.device mrproper
