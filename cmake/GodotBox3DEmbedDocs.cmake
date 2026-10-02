# Embed class reference XML as compressed doc data.
# The XML is generated manually via `python tools/generator/generate.py docs`
# into doc_classes/; this compiles it into the extension so Godot can show the
# editor help without shipping loose XML files.
find_package(Python3 COMPONENTS Interpreter)

set(GODOT_BOX3D_DOC_DATA "${CMAKE_CURRENT_BINARY_DIR}/doc_data.gen.cpp")
set(GODOT_BOX3D_DOC_CLASSES "${CMAKE_CURRENT_SOURCE_DIR}/doc_classes")
if(Python3_EXECUTABLE AND EXISTS "${GODOT_BOX3D_DOC_CLASSES}")
	file(GLOB GODOT_BOX3D_DOC_XML "${GODOT_BOX3D_DOC_CLASSES}/*.xml")
	if(GODOT_BOX3D_DOC_XML)
		add_custom_command(
			OUTPUT "${GODOT_BOX3D_DOC_DATA}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
			COMMAND ${Python3_EXECUTABLE}
				"${CMAKE_CURRENT_SOURCE_DIR}/tools/generator/embed_doc_data.py"
				"${GODOT_BOX3D_DOC_CLASSES}"
				"${GODOT_BOX3D_DOC_DATA}"
			DEPENDS ${GODOT_BOX3D_DOC_XML}
				"${CMAKE_CURRENT_SOURCE_DIR}/tools/generator/embed_doc_data.py"
			COMMENT "Embedding class reference documentation"
		)
		list(APPEND GODOT_BOX3D_SOURCES "${GODOT_BOX3D_DOC_DATA}")
	endif()
endif()
