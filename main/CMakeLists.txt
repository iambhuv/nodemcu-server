idf_component_register(
    SRCS "main.c" "wifi.c" "mdns.c"
    INCLUDE_DIRS "."
    # REQUIRES nvs_flash mdns lwip
)

# Force C linking
# set_target_properties(${COMPONENT_LIB} PROPERTIES LINKER_LANGUAGE C)