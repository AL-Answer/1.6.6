NAME := tg7100c_utils

## This component's src
COMPONENT_SRCS := src/utils_hex.c \
                  src/utils_fec.c \
                  src/utils_log.c \
                  src/utils_list.c \
                  src/utils_rbtree.c \
                  src/utils_hexdump.c \
                  src/utils_time.c \
                  src/utils_notifier.c \
				  src/utils_tlv_bl.c \
				  src/utils_getopt.c \
				  src/utils_dns.c \
				  src/utils_psk_fast.c \
				  src/utils_hmac_sha1_fast.c \

GLOBAL_INCLUDES += include

$(NAME)_SOURCES := $(COMPONENT_SRCS)

