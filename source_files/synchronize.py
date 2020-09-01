# This is a little script to synchronize our various build files.
#
# How it works:
# 1) Edit Qt .pri files
# 2) Run this script with python3
# 3) output: reformatted, Qt, Visual Studio ( cli only ), POSIX Makefile

import re
import sys
import os
import logging
import traceback
import logging
import pathlib

def parse_qt(filename):
    out = []
    with open(filename, "r") as f:
        for line in f:
            match = re.search(r"(\s*)(SOURCES|HEADERS|PRECOMPILED_HEADER|RESOURCES)(\s*\+?\=\s*)([\.\w\/-]*)(\.\w*)", line)
            if match:
                file_type = match.group(2)
                fullpath = match.group(4) + match.group(5)
                dirname = match.group(4)
                ending = match.group(5)
                result = (file_type, fullpath, dirname, ending)
                out.append(result)
    return out

def header(system):
    if system == "VS":
        h = "<!--\n"
    else:
        h = "# "
    h += "This file is automatically generated by " + os.path.basename(__file__) + "\n"
    if system != "VS":
        h += "# "
    h += "To change the list of source files run " + os.path.basename(sys.argv[0])
    if system == "VS":
        h += "\n-->"
    h += "\n\n"
    return h

def write_to_file(filename, out):
    with open(filename, "w") as f:
        f.write(out)

def create_make_str(entries):
    modified_input = replace(entries, r"engine/", r"")
    modified_input = replace(modified_input, r"/", r"$(PATHSEP)")
    prepare = header("Makefile")
    prepare += "SRC += \\"
    for file_type, fullpath in modified_input:
        if file_type in ("SOURCES"): #, "HEADERS"):
            prepare += "\n    " + fullpath + " \\"
    return prepare

def VS_header_str(filename, gui):
    if gui:
        moced_name = "moc_" + re.sub(r".*\\(.*?).hpp", r"\1.cpp", filename)
        return "\n\t\t<ClCompile Include=\"$(IntDir)" + moced_name + "\" />"
    else:
        return "\n\t\t<ClInclude Include=\"" + filename + "\" />"


def create_vs_str(entries, gui=False):
    modified_input = replace(entries, r"^", r"..\\")
    modified_input = replace(modified_input, r"/", r"\\")
    prepare = header("VS")
    prepare += """<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
\t<ItemGroup>"""
    for file_type, fullpath in modified_input:
        if re.search(r"sc_io.cpp", fullpath):
            prepare += "\n\t\t<ClCompile Include=\"" + fullpath + "\" />"
        elif file_type == "HEADERS":
            prepare += VS_header_str(fullpath, gui)
        elif file_type == "SOURCES":
            prepare += "\n\t\t<ClCompile Include=\"" + fullpath + "\" />"
    prepare += "\n\t</ItemGroup>"

    if gui:
        # Gui Resources
        prepare += "\n\n\t<!--Resources -->"
        prepare += "\n\t<ItemGroup>"
        prepare += "\n\t\t<ResourceCompile Include=\"..\simcqt.rc\" />"
        prepare += "\n\t</ItemGroup>"
        prepare += "\n\n"

        # Moc Defines
        prepare += "\t<!-- Moc Definitions -->"
        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='Debug-WebKit'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DWIN32 -DWIN64 -DSC_USE_WEBKIT -DQT_VERSION_5 -DQT_DECLARATIVE_DEBUG -DQT_WIDGETS -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -DQT_WEBKIT_LIB -DQT_WEBKITWIDGETS_LIB -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='Debug-WebEngine'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DWIN32 -DWIN64 -DSC_USE_WEBENGINE -DQT_VERSION_5 -DQT_DECLARATIVE_DEBUG -DQT_WIDGETS -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='WebEngine-PGO'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DWIN32 -DWIN64 -DSC_USE_WEBENGINE -DQT_VERSION_5 -DQT_NO_DEBUG -DQT_WIDGETS -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='WebEngine'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DWIN32 -DWIN64 -DSC_USE_WEBENGINE -DQT_VERSION_5 -DQT_NO_DEBUG -DQT_WIDGETS -DQT_OPENGL_LIB -DQT_WIDGETS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='WebKit'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DSC_USE_WEBKIT -DWIN32 -DWIN64 -DQT_VERSION_5 -DQT_NO_DEBUG -DQT_WEBKITWIDGETS_LIB -DQT_WIDGETS -DQT_MULTIMEDIAWIDGETS_LIB -DQT_OPENGL_LIB  -DQT_QML_LIB -DQT_MULTIMEDIA_LIB -DQT_WEBKIT_LIB -DQT_WIDGETS_LIB -DQT_SENSORS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\t<PropertyGroup Label=\"UserMacros\" Condition=\"'$(Configuration)'=='WebKit-PGO'\">"
        prepare += "\n\t\t<MOC_DEFINES>-DUNICODE -DSC_USE_WEBKIT -DWIN32 -DWIN64 -DQT_VERSION_5 -DQT_NO_DEBUG -DQT_WEBKITWIDGETS_LIB -DQT_WIDGETS -DQT_MULTIMEDIAWIDGETS_LIB -DQT_OPENGL_LIB  -DQT_QML_LIB -DQT_MULTIMEDIA_LIB -DQT_WEBKIT_LIB -DQT_WIDGETS_LIB -DQT_SENSORS_LIB -DQT_NETWORK_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_OPENGL_ES_2 -DQT_OPENGL_ES_2_ANGLE -D_MSC_VER=1800 -D_WIN32 -D_WIN64</MOC_DEFINES>"
        prepare += "\n\t</PropertyGroup>"

        prepare += "\n\n"

        # Moc extra build steps
        prepare += "\t<!-- Moc'ing GUI Header files -->"
        prepare += "\n\t<ItemGroup>"

        for entry in modified_input:
            if entry[0] == "HEADERS":
                prepare += """
\t\t<CustomBuild Include=\"""" + entry[1] + """\">
\t\t\t<AdditionalInputs>$(QTDIR)\\bin\moc.exe</AdditionalInputs>
\t\t\t<Message>Moc%27ing %(Identity)... ( with $(QTDIR)\\bin\moc.exe )</Message>
\t\t\t<Command>"$(QTDIR)\\bin\\moc.exe" $(MOC_DEFINES) -I"$(QTDIR)\\include" -I"(SolutionDir)engine" -I"$(QTDIR)\\mkspecs\\default" "%(Identity)" -o "$(IntDir)moc_%(Filename).cpp" </Command>
\t\t\t<AdditionalInputs>Rem;""" + entry[1] + """;%(AdditionalInputs)</AdditionalInputs>
\t\t\t<Outputs>$(IntDir)\\moc_%(Filename).cpp</Outputs>
\t\t</CustomBuild>"""

        prepare += "\n\t</ItemGroup>"

    prepare += "\n</Project>"
    return prepare

def create_cmake_str(entries):
    engine_source = list(entries)
    engine_cpp_files = [fullpath for file_type, fullpath, dirname, ending in engine_source if file_type in ["SOURCES", "HEADERS", "RESOURCES"]]
    engine_cpp_files = [pathlib.Path(f) for f in engine_cpp_files]
    engine_cpp_files = ["/".join(p.parts[1:]) for p in engine_cpp_files]
    output = "set(source_files\n{}\n)".format("\n".join(engine_cpp_files))
    return output

def replace(entries, separator, repl):
    r = []
    for entry in entries:
        r.append((entry[0], re.sub(separator, repl, entry[1])))
    return r


def sort_by_name(entries):
    entries.sort(key=lambda entry: entry[3], reverse=True)
    entries.sort(key=lambda entry: entry[2], reverse=True)
    entries.sort(key=lambda entry: entry[4], reverse=True)

def qmake_type_str(file_type, path, filters, prefix, exclude_match):
    header_files_nested = [pathlib.Path(path).rglob(filter) for filter in filters]
    header_files = [item for sublist in header_files_nested for item in sublist]
    if exclude_match is not None:
      header_files = filter(lambda x: not re.match(exclude_match, str(x)), header_files)
    header_files = [str(p.relative_to("../")).replace('\\', '/') for p in header_files]
    header_files.sort(key=lambda p: str(p).lower())
    lines = ["{} += {}".format(prefix, entry) for entry in header_files]
    return "\n".join(lines)
  
def create_qmake_str(file_type, path, excludes):
    output = header("qmake")
    output += qmake_type_str(file_type, path, ["*.hpp", "*.hh"], "HEADERS", excludes)
    output += "\n\n"
    output += qmake_type_str(file_type, path, ["*.cpp"], "SOURCES", excludes)
    output += "\n\n"
    output += qmake_type_str(file_type, path, ["*.qrc"], "RESOURCES", excludes)
    return output

def glob_files(file_type, path, excludes):
  write_to_file("QT_" + file_type + ".pri", create_qmake_str(file_type, path, excludes))
  
def create_file(file_type, build_systems):
    try:
        result = parse_qt("QT_" + file_type + ".pri")
        if "make" in build_systems:
            write_to_file(file_type + "_make", create_make_str(result))
        if "VS" in build_systems:
            write_to_file("VS_" + file_type + ".props", create_vs_str(result))
        if "VS_GUI" in build_systems:
            write_to_file("VS_" + file_type + ".props", create_vs_str(result, True))
        if "cmake" in build_systems:
            write_to_file("cmake_" + file_type + ".txt", create_cmake_str(result))
    except Exception as e:
        logging.error("Could not synchronize '{}' files: {}".format(file_type, e))
        logging.debug(traceback.format_exc())

# Creates source file list by looking for all .cpp, .hpp and .hh files
# Creates source file list for qmake, cmake, Visual Studio and classic engine/Makefile
def main():
    logging.basicConfig(level=logging.DEBUG)
    glob_files("engine", "../engine", ".*sc_main.cpp")
    glob_files("gui", "../qt", None)
    
    create_file("engine", ["make", "VS", "cmake"])
    create_file("engine_main", ["make", "VS", "cmake"])
    create_file("gui", ["VS_GUI", "cmake"])  # TODO: finish mocing part of VS_GUI
    logging.info("Done")


if __name__ == "__main__":
    main()
