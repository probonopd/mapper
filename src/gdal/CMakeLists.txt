#
#    Copyright 2016-2017 Kai Pastor
#    
#    This file is part of OpenOrienteering.
# 
#    OpenOrienteering is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
# 
#    OpenOrienteering is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
# 
#    You should have received a copy of the GNU General Public License
#    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.

find_package(GDAL REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC ON)
 
# Extra header to be shown in the IDE or to be translated
set(MAPPER_GDAL_HEADERS
  ogr_file_format_p.h
)
	
set(MAPPER_GDAL_SOURCES
  gdal_manager.cpp
  gdal_settings_page.cpp
  ogr_file_format.cpp
  ogr_template.cpp
  mapper-osmconf.ini
)
	
qt5_translations_sources(
  SOURCES ${MAPPER_GDAL_HEADERS} ${MAPPER_GDAL_SOURCES}
)

add_library(mapper-gdal STATIC ${MAPPER_GDAL_HEADERS} ${MAPPER_GDAL_SOURCES})

target_compile_definitions(mapper-gdal PRIVATE
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
  QT_USE_QSTRINGBUILDER
)

target_compile_definitions(mapper-gdal INTERFACE MAPPER_USE_GDAL)

target_include_directories(mapper-gdal PRIVATE "${GDAL_INCLUDE_DIR}" "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(mapper-gdal "${GDAL_LIBRARY}" Qt5::Core Qt5::Gui Qt5::Widgets)

set_target_properties(mapper-gdal PROPERTIES PREFIX "")

install(FILES mapper-osmconf.ini DESTINATION "${MAPPER_DATA_DESTINATION}/gdal")
