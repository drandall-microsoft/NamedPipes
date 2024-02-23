project (NamedPipesClient)

add_executable (${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} NamedPipesLib)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(${PROJECT_NAME}
PUBLIC
	"inc"
)
target_sources(${PROJECT_NAME}
PRIVATE
	"src/ClientMain.cpp"
)