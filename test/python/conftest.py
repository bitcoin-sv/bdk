#!/usr/bin/env python3

import os, sys
import pathlib
import pytest

#### Run individual test from command line with rootdir will load conftest.py
#python -m pytest -s --module_dir=/Path/to/module/dir my_test_file.py

#### From Pycharm : https://www.jetbrains.com/help/pycharm/pytest.html
#### Open the Settings/Preferences | Tools | Python Integrated Tools settings dialog, chose In the "Default test runner" field select pytest.
#### From an instance, go to "Edit Configuration" on the top-right,
#### Create a Python tests :
####    Specify Scrip path to the test file
####    Specify Additional Arguments : -s --module_dir=/Path/to/module/dir ## Option -s is to show the print to std output

default_module_dir = os.path.dirname(os.path.abspath(__file__))
def pytest_addoption(parser):## hack to make pytest accept arguments
    parser.addoption('--module_dir', metavar='-w', default=default_module_dir,help='default:{}'.format(default_module_dir))

def pytest_configure(config):
    import os, sys
    arg_module_dir_str = config.getoption("--module_dir")
    arg_module_dir_list = arg_module_dir_str.split(',')
    sys.path = arg_module_dir_list + sys.path
    import PyBDK 
    print('conftest.py successfully load all Py Modules')
    #print(sys.path)

@pytest.fixture
def test_data_dir(request):
    return pathlib.Path(request.config.getoption("--data_dir"))
