#!/usr/bin/python
#coding: utf-8

""" System """
import threading
import time

class SihdThread(threading.Thread):
 
    def __init__(self, name="SihdThread", step=None,
                    frequency=50, timeout=None, max_iter=None,
                    on_start=None, on_stop=None, on_err=None,
                    daemon=True, args=(), *targs, **kwtargs):
        super().__init__(daemon=daemon, name=name, *targs, **kwtargs)
        self.__stopped = True
        self.__paused = False
        self.daemon = daemon
        self.__args = args
        """ Callables """
        self.set_callbacks(on_start, on_stop, on_err)
        self.set_step_method(step)
        """ Step properties """
        if not self.set_max_iter(max_iter):
            self.__max_iter = None
        if not self.set_timeout(timeout):
            self._timeout = None
        if not self.set_frequency(frequency):
            self.__sleep = 0.05
        self.__pause_cdt = threading.Condition(lock=None)

    @staticmethod
    def is_main_thread():
        return isinstance(threading.current_thread(),
                            threading._MainThread)

    @staticmethod
    def find_method_by_str(instance, fun_name):
        """ Setting step method by string """
        try:
            method = getattr(instance, fun_name)
            return method
        except AttributeError as e:
            raise NotImplementedError("Class `{}` does not implement `{}`"\
                    .format(instance.__class__.__name__, fun_name))
        return None

    """ Getter/Setter """

    def set_callbacks(self, on_start, on_stop, on_err):
        if on_start and not callable(on_start):
            raise ValueError("Start method is not callable")
        if on_stop and not callable(on_stop):
            raise ValueError("Stop method is not callable")
        if on_err and not callable(on_err):
            raise ValueError("Error method is not callable")
        self.__on_start = on_start
        self.__on_stop = on_stop
        self.__on_error = on_err

    def is_running(self):
        return self.__stopped == False

    def get_step_method(self):
        return self.step

    def set_step_method(self, method):
        if not callable(method):
            raise ValueError("Not a callable")
        self.step = method

    def set_max_iter(self, max_iter):
        """ Setting max step before exiting """
        if not isinstance(max_iter, int) or max_iter <= 0:
            return False
        self.__max_iter = max_iter
        return True

    def set_frequency(self, frequency):
        """ Setting frequency in Hz for step execution """
        if not isinstance(frequency, int) or frequency <= 0:
            return False
        self.__sleep = float(1. / int(frequency))
        return True

    def set_timeout(self, timeout):
        """ Set a timeout for exiting after certain time """
        if not isinstance(timeout, (int, float)) or timeout <= 0.0:
            return False
        self._timeout = timeout
        return True

    """ Life cycle """

    def stop(self):
        self.resume()
        self.__stopped = True

    def pause(self):
        self.__paused = True

    def resume(self):
        cdt = self.__pause_cdt
        with cdt:
            cdt.notify(1)
        self.__paused = False

    """ Loop """

    def run(self):
        if self.step is None:
            raise RuntimeError("No function to execute")
        #state
        self.__stopped = False
        #time
        get_now = time.time
        sleep = time.sleep
        start = get_now()
        sleep_time = self.__sleep
        timeout = self._timeout
        #step
        step = self.step
        i = 0
        max_iter = self.__max_iter
        ret = None
        condition = self.__pause_cdt
        if self.__on_start:
            self.__on_start(self, *self.__args)
        while not self.__stopped:
            while self.__paused:
                #Pause wait
                with condition:
                    condition.wait(timeout=1)
                if self.__stopped:
                    break
            now = get_now()
            # Check timeout
            if timeout is not None:
                if ((start + timeout) <= now):
                    return
            # Execution
            try:
                ret = step()
            except Exception as e:
                self.stop()
                if self.__on_error:
                    self.__on_error(self, i, e)
                else:
                    raise
            if ret is False:
                break
            # Iterations
            i += 1
            if max_iter is not None and i >= max_iter:
                break
            # Pause
            end = get_now()
            pause = sleep_time - (end - now)
            if pause > 0.0:
                sleep(pause)
        if self.__on_stop:
            self.__on_stop(self, i)
