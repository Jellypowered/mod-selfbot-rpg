# SelfBot RPG is an extension module for mod-playerbots.
# Sources are recursively collected from src/; re-run CMake after adding files.
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src")
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod-selfbot-rpg.conf.dist")
