ccflags-y += -O3
ccflags-y += -I$(srctree)/lib/lz4/freestanding
ccflags-y += -std=gnu99

obj-$(CONFIG_LZ4_COMPRESS) += lz4_compress.o
obj-$(CONFIG_LZ4HC_COMPRESS) += lz4hc_compress.o
obj-$(CONFIG_LZ4_DECOMPRESS) += lz4_decompress.o
