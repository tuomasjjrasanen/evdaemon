from __future__ import absolute_import
from __future__ import with_statement

import os.path
import subprocess

import evdaemon.capabilities
import evdaemon.utils

def _query_config_dir():
    p = subprocess.Popen(["evdaemon", "--config-dir"], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    dirpath, errmsg = p.communicate()
    if errmsg:
        raise RuntimeError(errmsg.strip())
    return dirpath.strip()

DIRPATH = _query_config_dir()
FILTER_DURATION_FILEPATH = os.path.join(DIRPATH, "filter", "duration")

def access(mode):
    for dirpath, _, filenames in os.walk(DIRPATH):
        for filename in filenames:
            yield os.access(os.path.join(dirpath, filename), mode)

def rwaccess():
    return all(access(os.R_OK)), all(access(os.W_OK))

def write_line(line, path):
    with open(path, "w") as f:
        f.write(unicode(line))
        f.write("\n")

def write_name(name, device_role):
    return write_line(name, os.path.join(DIRPATH, device_role, "name"))

def write_filter_duration(value):
    return write_line(value, FILTER_DURATION_FILEPATH)

def write_clone_id(value, id_name):
    return write_line(int(value), os.path.join(DIRPATH, "clone", "id", id_name))

def write_capability_bits(value, device_role, capability):
    path = os.path.join(DIRPATH, device_role, "capabilities", capability)
    return write_line(evdaemon.utils.int_to_hexline(value), path)

def read_line(path):
    with open(path) as f:
        return f.readline().strip()

def read_int_line(path):
    return int(read_line(path))

def read_float_line(path):
    return float(read_line(path))

def read_filter_duration():
    return read_float_line(FILTER_DURATION_FILEPATH)

def read_clone_id(id_name):
    path = os.path.join(DIRPATH, "clone", "id", id_name)
    return read_int_line(path)

def read_capability_bits(device_role, capability):
    path = os.path.join(DIRPATH, device_role, "capabilities", capability)
    return evdaemon.utils.hexline_to_int(read_line(path), 16)

def read_name(device_role):
    path = os.path.join(DIRPATH, device_role, "name")
    return read_line(path)
