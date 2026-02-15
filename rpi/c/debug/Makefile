# Makefile for MPU-6050 debug test

CC = gcc
CFLAGS = -Wall -O2
LIBS = -lm

all: mpu6050

mpu6050: mpu6050.c
	$(CC) $(CFLAGS) -o mpu6050 mpu6050.c $(LIBS)

clean:
	rm -f mpu6050

.PHONY: all clean
