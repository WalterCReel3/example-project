ExternalProject_Add(
    rapidjson
    PREFIX third-party
    GIT_REPOSITORY https://github.com/miloyip/rapidjson.git
    CMAKE_ARGS
	-DRAPIDJSON_BUILD_TESTS=OFF
	-DRAPIDJSON_BUILD_EXAMPLES=OFF
	-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}
    BUILD_IN_SOURCE 1
)
