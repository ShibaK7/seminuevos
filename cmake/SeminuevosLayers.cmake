# Una capa de la arquitectura = una o más carpetas = una biblioteca estática.
#
# Por qué bibliotecas y no un solo ejecutable: cada biblioteca enlaza SOLO los
# módulos de Qt que su capa tiene permitidos (la tabla de dependencias está en
# cmake/CheckArchitecture.cmake). Así la regla la hace cumplir el compilador: un
# #include <QSqlQuery> dentro del dominio no compila, porque seminuevos_domain
# no ve los headers de Qt Sql. Lo que se escapa por esa vía (los includes con
# prefijo de módulo, como <QtSql/QSqlQuery>, o los includes entre capas) lo
# atrapa la prueba `architecture` (cmake/CheckArchitecture.cmake).
#
# Estáticas y no DLL: con MinGW una DLL obliga a exportar símbolos a mano y no
# aporta nada en una app que se distribuye como un solo ejecutable.
#
# Uso:
#   seminuevos_layer(<target>
#       DIRS <carpeta relativa a include/ y src/>...
#       [DEPENDS <otras bibliotecas del proyecto>...]
#       [QT <componentes de Qt además de Core>...]
#       [NO_MOC])
#
# NO_MOC apaga moc/uic/rcc. Se usa en el dominio: un Q_OBJECT ahí no tendría
# su código generado y el enlace fallaría, que es justo lo que se quiere (el
# dominio no hereda de QObject).

function(seminuevos_layer target)
    cmake_parse_arguments(LAYER "NO_MOC" "" "DIRS;DEPENDS;QT" ${ARGN})
    if(NOT LAYER_DIRS)
        message(FATAL_ERROR "seminuevos_layer(${target}): falta DIRS")
    endif()

    set(layer_files)
    foreach(dir IN LISTS LAYER_DIRS)
        # Los headers se listan además de los .cpp: AUTOMOC solo procesa los
        # Q_OBJECT de los headers que forman parte del target.
        file(GLOB_RECURSE dir_files CONFIGURE_DEPENDS
            "${PROJECT_SOURCE_DIR}/include/${dir}/*.h"
            "${PROJECT_SOURCE_DIR}/src/${dir}/*.h"
            "${PROJECT_SOURCE_DIR}/src/${dir}/*.cpp"
        )
        if(NOT dir_files)
            message(FATAL_ERROR
                "seminuevos_layer(${target}): la carpeta '${dir}' no tiene fuentes "
                "en include/ ni en src/. ¿Se movió o se escribió mal?")
        endif()
        list(APPEND layer_files ${dir_files})
    endforeach()

    add_library(${target} STATIC ${layer_files})
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/include")

    set(qt_libs Qt6::Core)
    foreach(component IN LISTS LAYER_QT)
        list(APPEND qt_libs Qt6::${component})
    endforeach()
    target_link_libraries(${target} PUBLIC ${qt_libs} ${LAYER_DEPENDS})

    if(LAYER_NO_MOC)
        set_target_properties(${target} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endif()
endfunction()
