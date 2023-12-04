SOURCE_main := $(shell find ./ -name '*.c')

output: $(SOURCE_main)
		mpicc -pthread -o output $(SOURCE_main)

