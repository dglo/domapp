C_BIN_NAMES   := runMessageBuffersTest runMessageTest runMsgHandlerTest domapp domEchoClient runMessagePthreadTest
USES_PROJECTS := hal
USES_TOOLS    := 
DEPENDS_UPON  := 
CC_FLAGS      := 

ifeq (cygwin-x86,$(strip $(PLATFORM)))
  XTRA_TOOLS_LIBDIRS += /usr/local/lib
  EXTRA_DEP_INC_DIRS += /usr/local/include
#  DEPENDS_UPON += cygipc
  USES_TOOLS   += cygipc
  C_BIN_NAMES  += domappSocket 
  CC_FLAGS     += -DCYGWIN
endif

ifeq (Linux-i386, $(strip $(PLATFORM)))
   XTRA_TOOLS_LIBDIRS := /usr/lib
   DEPENDS_UPON += pthread
   USES_TOOLS   += pthread
   CC_FLAGS     += -DLINUX
endif

ifeq (Linux-x86, $(strip $(PLATFORM)))
   XTRA_TOOLS_LIBDIRS := /usr/lib
   DEPENDS_UPON += pthread
   USES_TOOLS   += pthread
   CC_FLAGS     += -DLINUX
endif

include ../resources/standard.mk

# DO NOT DELETE
