idf_component_register(
    SRCS "main.c" "wifi.c" "mdns.c"
      "rfcodes/signal_parser.c"
      "rfcodes/signal_collector.c"
    INCLUDE_DIRS "." "rfcodes"
)

# Force C linking
# set_target_properties(${COMPONENT_LIB} PROPERTIES LINKER_LANGUAGE C)