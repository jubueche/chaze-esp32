#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#
CFLAGS += -DSET_TRUSTED_CERT_IN_SAMPLES
COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/main/server_certs/certs.pem
COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/main/server_certs/howsmyssl_com_root_cert.pem
COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/main/server_certs/msft_ssl_cert.pem