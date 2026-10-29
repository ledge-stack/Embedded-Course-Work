# Storage API boundary. Trevor currently provides the serialized queue and
# weak persistence hook until Edward supplies the SD-card implementation.
idf_component_register(INCLUDE_DIRS "../../include" REQUIRES common trevor)
