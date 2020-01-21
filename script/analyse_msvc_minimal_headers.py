#!/usr/bin/env python3

"""
    This script help to get all headers files that has been used to compile a target with MVSC.
    First, to get list of all header files used, open project Properties/Configuration Properties/C/C++/Advanded, change "Show Includes" to yes
    Then recompile the target, the build log will show list of header files used as "1>Note: including file: c:\\path\\to\\header\\file.h"
    Copy list of all lines to a string value preserving new line (triple quote)
    This script will analyse this string to get all used header files inside the bsv source code
"""
import json

## Replace the root bsv source code here
bsv_root_dir_str = "c:\\path\\to\\bsv\\root\\dir" #

## Replace the log of MVSC here with
list_header_file_log_str="""1>example.c
1>Note: including file: c:\\path\\to\\bsv\\root\\dir\\include/secp256k1.h
1>Note: including file:  C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\stddef.h
1>Note: including file:   C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.17763.0\\ucrt\\corecrt.h
1>Note: including file:    C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\vcruntime.h
1>Note: including file:     C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\sal.h
1>Note: including file:      C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\concurrencysal.h
1>Note: including file:     C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Tools\\MSVC\\14.16.27023\\include\\vadefs.h
1>Note: including file: c:\\users\\c.nguyen\\development\\sv\\src\\secp256k1\\src\\util.h
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


def analyse_msvc_log_showheader(root_str, log_str):
    """
    :param root_str:
    :param log_str:
    :return: Analyse from MVSC build log to get source/headers dependencies
    """
    line_list = log_str.splitlines()
    map_src_to_hdr = {}
    current_file_name = ''
    src_file_list = []
    for _line in line_list:
        if len(_line) < 1:
            continue

        line = _line[2:len(_line)]
        if len(line) < 20:
            print('----------------  source file [{}]  ------'.format(line))
            src_file_list.append(line)
            current_file_name = line

        if "Program Files (x86)" in _line or  "Note: including file:" not in _line or root_str not in _line:
            continue

        parts = line.rstrip().split(' ')
        file_path = parts[-1]
        if current_file_name not in map_src_to_hdr:
            map_src_to_hdr[current_file_name]=[file_path]
        else:
            map_src_to_hdr[current_file_name].append(file_path)
        print('{}     [{}]'.format(line, file_path))

    ### convert relative path and print json
    map_hdr_to_src = {}
    for cfile, hfile_list in map_src_to_hdr.items():
        for hfile in hfile_list:
            if hfile not in map_hdr_to_src:
                map_hdr_to_src[hfile] = [cfile]
            else:
                map_hdr_to_src[hfile].append(cfile)

    print('\n\n========  LIST USED HEADER FILES RELATIVE PATH==============')
    header_files = sorted(list(map_hdr_to_src.keys()))
    root_str_with_slash = '{}\\'.format(root_str)
    for hfile in header_files:
        list_dependend_cpp = map_hdr_to_src[hfile]
        comment_str = '##  Used by [{}]'.format('],[ '.join(list_dependend_cpp))
        line_str = '      {}  {}'.format(hfile.replace(root_str_with_slash,'').replace('\\','/'), comment_str)
        print(line_str)



if __name__ == "__main__":
    analyse_msvc_log_showheader(bsv_root_dir_str, list_header_file_log_str)