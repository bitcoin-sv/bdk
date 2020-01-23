#!/usr/bin/env python3

"""
    This script help to get all headers files that has been used to compile a target with MVSC.
    First, to get list of all header files used, open project Properties/Configuration Properties/C/C++/Advanded, change "Show Includes" to yes
    Then recompile the target, the build log will show list of header files used as "1>Note: including file: c:\\path\\to\\header\\file.h"
    Copy list of all lines to a string value preserving new line (triple quote)
    This script will analyse this string to get all used header files inside the bsv source code
"""
import json, re, pathlib

## Replace the root bsv source code here
bsv_root_dir_str = "c:\\path\\to\\bsv\\root\\dir" #

## Replace the log of MVSC here with
list_header_file_log_str="""1>example1.c
1>Note: including file: c:\\path\\to\\bsv\\root\\dir\\include/secp256k1.h
1>Note: including file:  C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\stddef.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt.h
1>Note: including file:    C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\vcruntime.h
1>Note: including file:     C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\sal.h
1>Note: including file:      C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\concurrencysal.h
1>Note: including file:     C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\vadefs.h
1>example2.cpp
1>Note: including file: c:\\path\\to\\bsv\\root\\dir\\src\\secp256k1\\src\\util.h
1>Note: including file:  C:\\Users\\c.nguyen\\development\\buildscrypt\\generated\\hpp\\libsecp256k1-config.h
1>Note: including file:  C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\stdlib.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt_malloc.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt_search.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt_wstdlib.h
1>Note: including file:   C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\limits.h
1>Note: including file:  C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\stdint.h
1>Note: including file:  C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\stdio.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt_wstdio.h
1>Note: including file:    C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt_stdio_config.h
1>Note: including file: c:\\path\\to\\bsv\\root\\dir\\include\\src\\secp256k1\\src\\num_impl.h"""


def analyse_msvc_log_showheader(root_str, log_str, with_comment=True):
    """
    :param root_str:
    :param log_str:
    :return: Analyse from MVSC build log to get source/headers dependencies
    """
    line_list = log_str.splitlines()
    map_src_to_hdr = {}
    current_cpp_file = ''
    src_file_list = []
    cpp_file_reg = re.compile('.+[.]cp*p*$')
    hpp_file_reg = re.compile('^Note: including file:.+$')
    root_path = pathlib.Path(root_str)
    line_id=0
    for _line in line_list:
        line_id+=1
        """ There are exclusively only 2 type of log line to consider : 
            1>filename.cpp             --> indicating a cpp file is compiling
            1>Note: including file:*   --> indicating the header file is being used
        """
        if len(_line) < 3:
            continue
        line_no_prefix = _line[2:len(_line)]  ## remove the '1>' part and normalized windows path to unix path

        is_cpp_file = cpp_file_reg.search(line_no_prefix)
        is_hpp_file = hpp_file_reg.search(line_no_prefix)
        if not (is_cpp_file or is_hpp_file):
            continue

        if is_cpp_file:
            print('----------------  source file [{}]  ------'.format(line_no_prefix))
            src_file_list.append(line_no_prefix)
            current_cpp_file = line_no_prefix
            continue

        ## From here, this is the hpp file. Consider only hpp file that is inside the root
        include_file_path_str= line_no_prefix.replace('Note: including file:','').strip()
        include_path = pathlib.Path(include_file_path_str)
        is_inside_root = False
        for _parent in include_path.parents:
            if _parent == root_path:
                is_inside_root=True
                break

        if not is_inside_root:
            continue

        if current_cpp_file not in map_src_to_hdr:
            map_src_to_hdr[current_cpp_file]=[include_file_path_str]
        else:
            map_src_to_hdr[current_cpp_file].append(include_file_path_str)
        print('{}     [{}]'.format(line_no_prefix, include_file_path_str))

    ### convert relative path and print json
    map_hdr_to_src = {}
    for cfile, hfile_list in map_src_to_hdr.items():
        for hfile in hfile_list:
            if hfile not in map_hdr_to_src:
                map_hdr_to_src[hfile] = [cfile]
            else:
                map_hdr_to_src[hfile].append(cfile)

    print('\n\n========  LIST USED HEADER FILES RELATIVE PATH==============')
    header_files = list(sorted(set(map_hdr_to_src.keys())))## There are some dirty duplicates due to upper/lower windows drive name C: c:
    rel_header_files = set()
    for hfile in header_files:
        hfile_filepath = pathlib.Path(hfile)
        hfile_relative_path =  hfile_filepath.relative_to(root_path)
        if hfile_relative_path in rel_header_files:# Do nothing if duplicate
            continue

        rel_header_files.add(hfile_relative_path)
        list_dependend_cpp = list(sorted( set(map_hdr_to_src[hfile]) ))
        comment_str = '##  Used by [{}]'.format('], ['.join(list_dependend_cpp))
        line_str = '      "{}"  {}'.format(str(hfile_relative_path).replace('\\','/'), comment_str) if with_comment else '      {}'.format(hfile_relative_path)
        print(line_str)
    print('      ## Total number of header files : {}'.format(len(rel_header_files)))


if __name__ == "__main__":
    analyse_msvc_log_showheader(bsv_root_dir_str, list_header_file_log_str, True)