# Serial command boundary; command handlers will depend on auth, entrance,
# storage and room-facing APIs when Edward's implementation is integrated.
idf_component_register(INCLUDE_DIRS "../../include" REQUIRES auth common)
