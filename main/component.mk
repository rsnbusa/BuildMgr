CXXFLAGS += -fpermissive 
COMPONENT_SRCDIRS = . src HW Cmds
COMPONENT_ADD_INCLUDEDIRS = . inc HW src HW/Include
COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/server_certs/ca_cert.pem