ExternalProject_Add(
    rapidjson
    PREFIX third-party
    GIT_REPOSITORY https://github.com/miloyip/rapidjson.git
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
    BUILD_IN_SOURCE 1
)
