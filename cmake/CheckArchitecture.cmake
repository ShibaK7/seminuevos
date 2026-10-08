# Prueba de arquitectura: revisa que cada capa solo dependa de lo que tiene
# permitido. Se corre con ctest (prueba `architecture`) o a mano:
#   cmake -DROOT=<raíz del repo> [-DQT_HEADERS=<qt>/include] -P cmake/CheckArchitecture.cmake
#
# Es la tercera cerca. Las otras dos son el compilador (cada capa es una
# biblioteca que solo ve los headers de sus módulos de Qt) y el enlazador (cada
# prueba enlaza solo su capa). Esta atrapa lo que se les escapa:
#   - includes entre capas del proyecto ("adapters/..." desde el dominio, etc.);
#   - headers de Qt de un módulo prohibido, con o sin prefijo (<QtSql/QSqlQuery>
#     se resuelve desde el include base de Qt aunque la biblioteca no enlace
#     Sql; y mientras una capa se compile dentro del ejecutable, que enlaza
#     todo, ni siquiera <QSqlQuery> falla al compilar);
#   - QObject / Q_OBJECT / Q_GADGET en domain y application;
#   - que lo común de una capa (domain/common, application/common...) no
#     dependa de un módulo (inventory, auth...);
#   - acceso a disco desde las vistas;
#   - slots autoconectados (on_<objeto>_<señal>) y conexiones o estilos
#     escritos dentro de los .ui.
#
# QT_HEADERS es la carpeta include/ de Qt. Con ella se reconoce a qué módulo
# pertenece un header sin prefijo (<QSqlQuery>, <qsqlquery.h>); sin ella solo
# se revisan los includes con prefijo de módulo.
#
# Las violaciones que todavía existen a propósito (porque las quita un commit
# posterior del plan) van en cmake/architecture-allowlist.txt. Una violación
# nueva hace fallar la prueba; una entrada del allowlist que ya no hace falta
# solo avisa, para que se borre.
#
# Las carpetas que aún no tienen capa (db/, vehiclewizard/, components/, ...)
# no se revisan: la mudanza a carpetas hexagonales las ubica.

# En modo script (-P) CMake arranca con las políticas viejas, en las que
# while(TRUE) no evalúa TRUE como booleano. Fijar la versión las actualiza.
cmake_minimum_required(VERSION 3.22)

if(NOT ROOT)
    message(FATAL_ERROR "Uso: cmake -DROOT=<raíz del repo> -P CheckArchitecture.cmake")
endif()
get_filename_component(ROOT "${ROOT}" ABSOLUTE)
if(NOT EXISTS "${ROOT}/include" AND NOT EXISTS "${ROOT}/src")
    message(FATAL_ERROR "ROOT='${ROOT}' no parece la raíz del repo (no tiene include/ ni src/).")
endif()
if(QT_HEADERS)
    get_filename_component(QT_HEADERS "${QT_HEADERS}" ABSOLUTE)
    if(NOT EXISTS "${QT_HEADERS}/QtCore")
        message(FATAL_ERROR "QT_HEADERS='${QT_HEADERS}' no contiene QtCore/.")
    endif()
endif()

# --- Utilidades -----------------------------------------------------------

# Quita los comentarios de un fuente C++. Con BLANK_STRINGS también vacía las
# cadenas literales (para buscar palabras sin que cuente un texto entre
# comillas); sin él las conserva (para leer los #include "...").
# Se hace avanzando con string(FIND) y no con un regex: un regex recursivo
# sobre un comentario largo agota la pila de CMake, y quitar primero los //
# rompe un /* ... http://... */.
function(strip_cpp_comments content outvar)
    cmake_parse_arguments(SC "BLANK_STRINGS" "" "" ${ARGN})
    set(result "")
    set(rest "${content}")
    while(TRUE)
        string(FIND "${rest}" "//" p_line)
        string(FIND "${rest}" "/*" p_block)
        string(FIND "${rest}" "\"" p_str)
        set(pos -1)
        set(kind "")
        foreach(pair "line;${p_line}" "block;${p_block}" "str;${p_str}")
            list(GET pair 0 k)
            list(GET pair 1 p)
            if(p GREATER -1 AND (pos EQUAL -1 OR p LESS pos))
                set(pos ${p})
                set(kind ${k})
            endif()
        endforeach()
        if(pos EQUAL -1)
            string(APPEND result "${rest}")
            break()
        endif()

        string(SUBSTRING "${rest}" 0 ${pos} before)
        string(APPEND result "${before}")
        string(SUBSTRING "${rest}" ${pos} -1 rest)

        if(kind STREQUAL "line")
            string(FIND "${rest}" "\n" eol)
            if(eol EQUAL -1)
                set(rest "")
            else()
                string(SUBSTRING "${rest}" ${eol} -1 rest)
            endif()
        elseif(kind STREQUAL "block")
            string(SUBSTRING "${rest}" 2 -1 rest)
            string(FIND "${rest}" "*/" close)
            if(close EQUAL -1)
                set(rest "")
            else()
                math(EXPR close "${close} + 2")
                string(SUBSTRING "${rest}" ${close} -1 rest)
                string(APPEND result " ")
            endif()
        else()
            # Cadena literal. Un '"' entre apóstrofos es un carácter, no el
            # inicio de una cadena.
            string(LENGTH "${result}" result_len)
            set(is_char FALSE)
            if(result_len GREATER 0)
                math(EXPR last "${result_len} - 1")
                string(SUBSTRING "${result}" ${last} 1 prev)
                if(prev STREQUAL "'")
                    set(is_char TRUE)
                endif()
            endif()
            string(SUBSTRING "${rest}" 1 -1 rest)
            if(is_char)
                string(APPEND result "\"")
                continue()
            endif()
            # Buscar la comilla de cierre que no esté escapada.
            set(literal "")
            while(TRUE)
                string(FIND "${rest}" "\"" q)
                if(q EQUAL -1)
                    set(literal "${literal}${rest}")
                    set(rest "")
                    break()
                endif()
                string(SUBSTRING "${rest}" 0 ${q} piece)
                math(EXPR after "${q} + 1")
                string(SUBSTRING "${rest}" ${after} -1 rest)
                set(literal "${literal}${piece}")
                # Cuenta las barras invertidas al final del trozo.
                string(REGEX MATCH "\\\\+$" slashes "${piece}")
                string(LENGTH "${slashes}" nslashes)
                math(EXPR odd "${nslashes} % 2")
                if(odd EQUAL 0)
                    break()
                endif()
                set(literal "${literal}\"")
            endwhile()
            if(SC_BLANK_STRINGS)
                string(APPEND result "\"\"")
            else()
                string(APPEND result "\"${literal}\"")
            endif()
        endif()
    endwhile()
    set(${outvar} "${result}" PARENT_SCOPE)
endfunction()

# Capa de un archivo según su ruta relativa.
function(layer_of relpath outvar)
    set(layer "")
    if(relpath MATCHES "^(include|src)/domain/")
        set(layer "domain")
    elseif(relpath MATCHES "^(include|src)/application/")
        set(layer "application")
    elseif(relpath MATCHES "^(include|src)/adapters/([^/]+)/")
        set(layer "adapters/${CMAKE_MATCH_2}")
    elseif(relpath MATCHES "^(include|src)/presentation/views/")
        set(layer "views")
    elseif(relpath MATCHES "^(include|src)/presentation/")
        set(layer "presentation")
    endif()
    set(${outvar} "${layer}" PARENT_SCOPE)
endfunction()

# Qué includes del proyecto ("a/b.h") puede usar cada capa.
function(allowed_project_includes layer outvar)
    if(layer STREQUAL "domain")
        set(rx "^domain/")
    elseif(layer STREQUAL "application")
        set(rx "^(domain|application)/")
    elseif(layer MATCHES "^adapters/(.+)$")
        set(rx "^(domain/|application/(.+/)?(ports|dto)/|adapters/${CMAKE_MATCH_1}/)")
    elseif(layer STREQUAL "presentation")
        set(rx "^(application/(.+/)?(services|dto)/|domain/(.+/)?value_objects/|presentation/(presenters|navigation|tasks)/)")
    elseif(layer STREQUAL "views")
        set(rx "^(presentation/|application/(.+/)?dto/|domain/(.+/)?value_objects/)")
    endif()
    set(${outvar} "${rx}" PARENT_SCOPE)
endfunction()

# Módulos de Qt prohibidos por capa. "ALL_BUT_CORE" = solo QtCore.
function(forbidden_qt_modules layer outvar)
    if(layer STREQUAL "domain" OR layer STREQUAL "application" OR layer STREQUAL "presentation")
        set(mods "ALL_BUT_CORE")
    elseif(layer STREQUAL "views")
        set(mods "Sql;Network;PrintSupport")
    elseif(layer MATCHES "^adapters/")
        set(mods "Widgets")
    else()
        set(mods "")
    endif()
    set(${outvar} "${mods}" PARENT_SCOPE)
endfunction()

# Módulo de Qt de un header de <...>, o "" si no es de Qt.
function(qt_module_of inc outvar)
    set(module "")
    if(inc MATCHES "^Qt([A-Za-z]+)/")
        set(module "${CMAKE_MATCH_1}")
    elseif(QT_HEADERS AND NOT inc MATCHES "/")
        if(EXISTS "${QT_HEADERS}/QtCore/${inc}")
            set(module "Core")
        else()
            file(GLOB hits "${QT_HEADERS}/Qt*/${inc}")
            if(hits)
                list(GET hits 0 hit)
                get_filename_component(dir "${hit}" DIRECTORY)
                get_filename_component(dir "${dir}" NAME)
                string(REGEX REPLACE "^Qt" "" module "${dir}")
            endif()
        endif()
    endif()
    set(${outvar} "${module}" PARENT_SCOPE)
endfunction()

# Headers de acceso a disco que las vistas no pueden usar (las vistas piden
# rutas al usuario y pintan bytes; leer y escribir archivos es del adaptador).
set(DISK_HEADERS_RX "^(qtcore/|qtgui/|qtwidgets/)?(qfile|qdir|qfileinfo|qtemporaryfile|qtemporarydir|qsavefile|qmimedatabase|qimagereader|qimagewriter|qstandardpaths|qdiriterator|qfilesystemwatcher|qfilesystemmodel)(\\.h)?$")

set(violations "")

# --- Código C++ -------------------------------------------------------------
file(GLOB_RECURSE code_files
    "${ROOT}/include/*.h"
    "${ROOT}/src/*.h"
    "${ROOT}/src/*.cpp"
)
foreach(path IN LISTS code_files)
    file(RELATIVE_PATH rel "${ROOT}" "${path}")
    layer_of("${rel}" layer)
    if(layer STREQUAL "")
        continue()
    endif()

    allowed_project_includes("${layer}" allowed_rx)
    forbidden_qt_modules("${layer}" forbidden_mods)

    file(READ "${path}" content)
    strip_cpp_comments("${content}" code)
    strip_cpp_comments("${content}" bare BLANK_STRINGS)

    string(REGEX MATCHALL "#[ \t]*include[ \t]*(\"[^\"\n]+\"|<[^>\n]+>)" includes "${code}")
    foreach(directive IN LISTS includes)
        if(directive MATCHES "\"([^\"]+)\"")
            set(inc "${CMAKE_MATCH_1}")
            if(inc MATCHES "(^|/)\\.\\.(/|$)")
                list(APPEND violations "${rel}: include relativo \"${inc}\"")
            elseif(inc MATCHES "/")
                if(NOT inc MATCHES "${allowed_rx}")
                    list(APPEND violations "${rel}: incluye \"${inc}\"")
                elseif(rel MATCHES "^(include|src)/[^/]+/common/" AND inc MATCHES "^(domain|application|adapters|presentation)/[^/]+/"
                       AND NOT inc MATCHES "^[^/]+/common/")
                    # Lo común no depende de un módulo: si lo hiciera, el
                    # módulo dejaría de poder usarlo sin un ciclo.
                    list(APPEND violations "${rel}: lo común incluye un módulo \"${inc}\"")
                endif()
            elseif(inc MATCHES "\\.moc$")
                # Código que genera moc para el propio archivo.
            elseif(inc MATCHES "^ui_.+\\.h$" AND layer STREQUAL "views")
                # Formulario de Designer: solo las vistas los usan.
            elseif(rel MATCHES "^src/" AND EXISTS "${ROOT}/${rel}/../${inc}")
                # Header privado de la capa, junto a su .cpp en src/ (no se
                # publica en include/).
            else()
                list(APPEND violations "${rel}: include sin capa \"${inc}\"")
            endif()
        elseif(directive MATCHES "<([^>]+)>")
            set(inc "${CMAKE_MATCH_1}")
            qt_module_of("${inc}" module)
            if(module AND forbidden_mods)
                if(forbidden_mods STREQUAL "ALL_BUT_CORE")
                    if(NOT module STREQUAL "Core")
                        list(APPEND violations "${rel}: incluye <${inc}> (Qt${module})")
                    endif()
                else()
                    list(FIND forbidden_mods "${module}" idx)
                    if(idx GREATER -1)
                        list(APPEND violations "${rel}: incluye <${inc}> (Qt${module})")
                    endif()
                endif()
            endif()
            string(TOLOWER "${inc}" inc_lower)
            if(layer STREQUAL "views" AND inc_lower MATCHES "${DISK_HEADERS_RX}")
                list(APPEND violations "${rel}: acceso a disco <${inc}>")
            endif()
        endif()
    endforeach()

    if(layer STREQUAL "domain" OR layer STREQUAL "application")
        string(REGEX MATCHALL "(Q_OBJECT|Q_GADGET|QObject)" qobject_hits "${bare}")
        list(REMOVE_DUPLICATES qobject_hits)
        foreach(hit IN LISTS qobject_hits)
            list(APPEND violations "${rel}: usa ${hit}")
        endforeach()
    endif()
    string(REGEX MATCHALL "void[ \t]+on_[A-Za-z0-9_]+_[A-Za-z0-9]+[ \t]*\\(" slot_hits "${bare}")
    foreach(hit IN LISTS slot_hits)
        string(REGEX MATCH "on_[A-Za-z0-9_]+" slot "${hit}")
        list(APPEND violations "${rel}: slot autoconectado ${slot}")
    endforeach()
endforeach()

# --- Archivos .ui -------------------------------------------------------------
file(GLOB ui_files "${ROOT}/ui/*.ui" "${ROOT}/src/ui/*.ui")
foreach(path IN LISTS ui_files)
    file(RELATIVE_PATH rel "${ROOT}" "${path}")
    file(READ "${path}" content)

    # Cada conexión declarada en el .ui, por emisor y señal.
    string(REGEX MATCHALL "<connection>[^<]*<sender>[^<]*</sender>[^<]*<signal>[^<]*</signal>" connections "${content}")
    foreach(c IN LISTS connections)
        string(REGEX MATCH "<sender>([^<]*)</sender>" _ "${c}")
        set(sender "${CMAKE_MATCH_1}")
        string(REGEX MATCH "<signal>([^<]*)</signal>" _ "${c}")
        list(APPEND violations "${rel}: conexión ${sender}.${CMAKE_MATCH_1} en el .ui")
    endforeach()
    if(content MATCHES "<connection>" AND NOT connections)
        list(APPEND violations "${rel}: conexión declarada en el .ui")
    endif()

    # Cada styleSheet con contenido, nombrado por el widget que lo lleva. Un
    # styleSheet vacío (<string/> o <string notr="true"/>) es inofensivo.
    set(rest "${content}")
    set(consumed "")
    while(TRUE)
        string(FIND "${rest}" "<property name=\"styleSheet\">" p)
        if(p EQUAL -1)
            break()
        endif()
        string(SUBSTRING "${rest}" 0 ${p} head)
        string(APPEND consumed "${head}")
        string(SUBSTRING "${rest}" ${p} -1 rest)
        string(FIND "${rest}" "<string" s)
        string(SUBSTRING "${rest}" ${s} -1 tail)
        string(FIND "${tail}" ">" gt)
        math(EXPR before_gt "${gt} - 1")
        string(SUBSTRING "${tail}" ${before_gt} 1 closing)
        math(EXPR after_gt "${gt} + 1")
        string(SUBSTRING "${tail}" ${after_gt} 1 first_char)
        if(NOT closing STREQUAL "/" AND NOT first_char STREQUAL "<")
            # Widget dueño: el último <widget ... name="X"> antes de la propiedad.
            string(FIND "${consumed}" "<widget " w REVERSE)
            set(owner "?")
            if(w GREATER -1)
                string(SUBSTRING "${consumed}" ${w} 300 widget_tag)
                if(widget_tag MATCHES "name=\"([^\"]+)\"")
                    set(owner "${CMAKE_MATCH_1}")
                endif()
            endif()
            list(APPEND violations "${rel}: styleSheet en ${owner}")
        endif()
        string(SUBSTRING "${rest}" 1 -1 rest)
        string(APPEND consumed "<")
    endwhile()
endforeach()

# --- Comparar contra el allowlist -------------------------------------------
set(allowed "")
set(allowlist_file "${CMAKE_CURRENT_LIST_DIR}/architecture-allowlist.txt")
if(EXISTS "${allowlist_file}")
    file(STRINGS "${allowlist_file}" allow_lines ENCODING UTF-8)
    foreach(line IN LISTS allow_lines)
        string(REGEX REPLACE "#.*$" "" line "${line}")
        string(STRIP "${line}" line)
        if(NOT line STREQUAL "")
            list(APPEND allowed "${line}")
        endif()
    endforeach()
endif()

list(REMOVE_DUPLICATES violations)
set(new_violations "")
foreach(v IN LISTS violations)
    list(FIND allowed "${v}" idx)
    if(idx EQUAL -1)
        list(APPEND new_violations "${v}")
    endif()
endforeach()

foreach(a IN LISTS allowed)
    list(FIND violations "${a}" idx)
    if(idx EQUAL -1)
        message(WARNING "Entrada del allowlist que ya no hace falta (bórrala): ${a}")
    endif()
endforeach()

list(LENGTH violations total)
list(LENGTH new_violations total_new)
if(total_new GREATER 0)
    foreach(v IN LISTS new_violations)
        message("VIOLACIÓN: ${v}")
    endforeach()
    message(FATAL_ERROR "${total_new} violación(es) nueva(s) de la arquitectura. "
        "Corrígelas o, si las quita un commit posterior del plan, agrégalas a "
        "cmake/architecture-allowlist.txt con ese commit en el comentario.")
endif()
if(NOT QT_HEADERS)
    message(STATUS "Sin QT_HEADERS: solo se revisaron los headers de Qt con prefijo de módulo.")
endif()
message(STATUS "Arquitectura OK (${total} violación(es) conocidas en el allowlist).")
