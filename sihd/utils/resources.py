#!/usr/bin/python
# coding: utf-8

import atexit
import os
from weakref import WeakSet

pid = os.getpid()
services = WeakSet()
threads = WeakSet()
processes = WeakSet()

def register_service(service):
    services.add(service)

def register_thread(thread):
    threads.add(thread)

def register_process(proc):
    processes.add(proc)

def stop_services():
    for service in services:
        if service.is_running():
            try:
                service.stop()
            except:
                pass

def end_threads():
    pass

def end_processes():
    pass

def end_program():
    stop_services()
    end_threads()
    end_processes()

atexit.register(stop_services)
