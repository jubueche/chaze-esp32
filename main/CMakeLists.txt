# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.cpp"
					"iothub_client_sample_mqtt.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
component_compile_definitions(SET_TRUSTED_CERT_IN_SAMPLES)
