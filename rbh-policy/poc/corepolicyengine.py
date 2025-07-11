import importlib.util
import argparse
import types
import sys
import os

class ConfigCondition:
    def __init__(self, key, op_str=None):
        self.key = key  # string or lambda
        self.op_str = op_str  # for printing
    def _wrap(self, func, op_str):
        return ConfigCondition(func, op_str)
    def _get(self, data):
        return self.key(data) if callable(self.key) else data[self.key]
    def evaluate(self, data):
        return self._get(data) if callable(self.key) else data[self.key]
    def __eq__(self, other):
        return self._wrap(lambda data: self._get(data) == other,
                          f"({self} == {other})")
    def __gt__(self, other):
        return self._wrap(lambda data: self._get(data) > other,
                          f"({self} > {other})")
    def __lt__(self, other):
        return self._wrap(lambda data: self._get(data) < other,
                          f"({self} < {other})")
    def __ge__(self, other):
        return self._wrap(lambda data: self._get(data) >= other,
                          f"({self} >= {other})")
    def __le__(self, other):
        return self._wrap(lambda data: self._get(data) <= other,
                          f"({self} <= {other})")
    def __and__(self, other):
        return self._wrap(lambda data: self.evaluate(data) and
                          other.evaluate(data), f"({self} & {other})")
    def __or__(self, other):
        return self._wrap(lambda data: self.evaluate(data) or
                          other.evaluate(data), f"({self} | {other})")
    def __invert__(self):
        return self._wrap(lambda data: not self.evaluate(data), f"({self})")
    def IN(self, *values):
        return self._wrap(
            lambda data: self._get(data) in values,
            f"({self} IN {values})"
        )
    def __repr__(self):
        return self.op_str if self.op_str else f"Condition({self.key})"

class FileClassCondition:
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition
    def evaluate(self, data):
        return self.condition.evaluate(data)
    def __and__(self, other):
        return ConfigCondition(lambda data: self.evaluate(data) and
                               other.evaluate(data))
    def __or__(self, other):
        return ConfigCondition(lambda data: self.evaluate(data) or
                               other.evaluate(data))
    def __invert__(self):
        return ConfigCondition(lambda data: not self.evaluate(data))
    def __repr__(self):
        return f"FileClass({self.name})"

fileclasses = {}
policies = {}

def declare_fileclass(name, condition):
    fc = FileClassCondition(name, condition)
    # Register the fileclass in the global dictionary for later reference.
    fileclasses[name] = fc
    # Retrieve the global namespace of the module that called this function.
    caller_module = sys._getframe(1).f_globals
    # Inject the fileclass instance into the caller's global namespace
    # This allows the fileclass to be used as a global variable in the config.
    caller_module[name] = fc
    return fc

def declare_policy(name, target, action, parameters=None, trigger=None,
                   rules=None):
    policies[name] = {
        "name": name,
        "target": target,
        "action": action,
        "parameters": parameters or {},
        "trigger": trigger,
        "rules": rules or []
    }

Size = ConfigCondition("Size")
LastAccess = ConfigCondition("LastAccess")
LastModification = ConfigCondition("LastModification")
User = ConfigCondition("User")

def archive_files(**kwargs):
    print(f"[ACTION] archive_files with {kwargs}")

def delete_files(**kwargs):
    print(f"[ACTION] delete_files with {kwargs}")
