add_library(alltools STATIC
  tool
  stat_util
  )
add_dependencies(alltools generate_error_codes generate_action_types)
target_link_libraries(alltools LINK_PRIVATE
  serveronly
  coreserver
  coredb
  notmongodormongos
  )

add_library(docgenerator STATIC
  docgenerator
  )
add_dependencies(docgenerator generate_error_codes generate_action_types)