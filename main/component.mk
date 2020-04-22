CXXFLAGS += -fpermissive 
COMPONENT_SRCDIRS = . src HW Cmds
COMPONENT_ADD_INCLUDEDIRS = . inc HW src HW/Include

COMPONENT_EMBED_FILES= ok.png nak.png meter.png
COMPONENT_EMBED_TXTFILES= challenge.html connSetup.html login.html ok.html
COMPONENT_EMBED_TXTFILES+=certs/prvtkey.pem