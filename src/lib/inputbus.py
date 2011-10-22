from __future__ import absolute_import
from __future__ import with_statement

import re

import evdaemon.utils

_DEVLIST_FILEPATH = "/proc/bus/input/devices"

_ID_PATTERN = re.compile(r"^I: Bus=(?P<bus>[a-f\d]{4}) Vendor=(?P<vendor>[a-f\d]{4}) Product=(?P<product>[a-f\d]{4}) Version=(?P<version>[a-f\d]{4})$")
_NAME_PATTERN = re.compile(r'^N: Name="(?P<name>.*)"$')
_PHYS_PATTERN = re.compile(r"^P: Phys=(?P<phys>.*)$")
_SYSFS_PATTERN = re.compile(r"^S: Sysfs=(?P<sysfs>.*)$")
_UNIQ_PATTERN = re.compile(r"^U: Uniq=(?P<uniq>.*)$")
_HANDLERS_PATTERN = re.compile(r"^H: Handlers=(?P<handlers>.*)$")
_EV_PATTERN = re.compile(r"^B: EV=(?P<ev>.*)$")
_CAPABILITY_PATTERN = re.compile(r"^B: (?P<capability>[A-Z]+)=(?P<value>.*)$")
_BLOCK_END_PATTERN = re.compile(r"^$")

def device(attr_name, attr_value, device_list_filepath=_DEVLIST_FILEPATH):
    for device in devices(device_list_filepath).values():
        if device[attr_name] == value:
            yield device

def devices(device_list_filepath=_DEVLIST_FILEPATH):
    with open(device_list_filepath) as devfile:
        result = {}
        for line in devfile:
            device = {
                "bus": None,
                "vendor": None,
                "product": None,
                "version": None,
                "name": None,
                "phys": None,
                "sysfs": None,
                "uniq": None,
                "handlers": None,
                "ev": None,
                "capabilities": {}
                }
            try:
                device.update(_ID_PATTERN.match(line).groupdict())
            except AttributeError, e:
                raise e
            device.update(_NAME_PATTERN.match(devfile.next()).groupdict())
            device.update(_PHYS_PATTERN.match(devfile.next()).groupdict())
            device.update(_SYSFS_PATTERN.match(devfile.next()).groupdict())
            device.update(_UNIQ_PATTERN.match(devfile.next()).groupdict())
            device.update(_HANDLERS_PATTERN.match(devfile.next()).groupdict())
            device["handlers"] = device["handlers"].split()
            device.update(_EV_PATTERN.match(devfile.next()).groupdict())
            device["ev"] = evdaemon.utils.hexline_to_int(device["ev"])
            while True:
                line = devfile.next()
                if _BLOCK_END_PATTERN.match(line):
                    break
                capability_dict = _CAPABILITY_PATTERN.match(line).groupdict()
                capability = capability_dict["capability"].lower()
                value = evdaemon.utils.hexline_to_int(capability_dict["value"])
                device["capabilities"][capability] = value
            result[device["name"]] = device
    return result
