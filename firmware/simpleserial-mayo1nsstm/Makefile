TARGET = simpleserial-mayo1nsstm

SRC += simpleserial-mayo1nsstm.c


SRC += api.c
SRC += mayo.c
SRC += params.c
SRC += mem.c
SRC += arithmetic.c
SRC += aes_ctr.c
SRC += aes-publicinputs.c
SRC += fips202.c
SRC += keccakf1600.c
SRC += randombytes.c
#SRC += p1p1t_data.c
SRC += data_p1.c

ASRC += aes-encrypt.S  
ASRC += aes-publicinputs-asm.S
ASRC += aes-keyschedule.S

SS_VER = SS_VER_2_1

CFLAGS   += -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
#ASFLAGS  += -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CFLAGS += -O1

FIRMWAREPATH = ..
CRYPTO_TARGET = NONE


include ../simpleserial/Makefile.simpleserial
include ../Makefile.inc