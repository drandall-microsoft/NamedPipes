project (NamedPipesLib)

add_library (${PROJECT_NAME})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

target_include_directories(${PROJECT_NAME}
PUBLIC
	"inc"
)
target_sources(${PROJECT_NAME}
PRIVATE
	"src/NamedPipes.cpp"
)