#!/usr/bin/python
# coding: utf-8

""" System """
import os
import time
import unittest
import json
import utils
import sihd
logger = sihd.log.setup('info')

is_internet = sihd.network.ip.is_internet()

class TestSihdApi(unittest.TestCase):

    def setUp(self):
        print()

    def tearDown(self):
        pass

    #
    # Strings
    #

    def test_strings(self):
        logger.info("Testing strings")
        current_lang = sihd.strings.get_lang()
        logger.info("Current language: " + current_lang)
        self.assertTrue(sihd.strings.is_lang(current_lang))
        self.assertFalse(sihd.strings.is_lang("fake"))
        self.assertFalse(sihd.strings.is_key('error'))
        self.assertFalse(sihd.strings.is_key('info'))
        # Loading in current language
        dic = {
            "error": "This is an error code",
            "info": "This is an info",
        }
        sihd.strings.load(dic)
        self.assertTrue(sihd.strings.is_key('error'))
        self.assertTrue(sihd.strings.is_key('info'))
        logger.info(sihd.strings.get('error'))
        logger.info(sihd.strings.get('info'))
        # Loading in a fake language
        sihd.strings.load(dic, lang='fake')
        self.assertTrue(sihd.strings.is_key('error', lang='fake'))
        self.assertTrue(sihd.strings.is_key('info', lang='fake'))
        # Setting current language as fake
        sihd.strings.set_lang('fake')
        # Adding for current language
        sihd.strings.add("warning", "This is a warning code")
        # Should not be in old language
        self.assertFalse(sihd.strings.is_key('warning', lang=current_lang))
        self.assertTrue(sihd.strings.is_key('warning'))
        self.assertTrue(sihd.strings.is_key('warning', lang='fake'))
        logger.info(sihd.strings.get('warning'))


    #
    # Resources
    #

    def test_resources(self):
        logger.info("Test resources")
        self.assertTrue(sihd.resources.get('test_sihd_api.py') is None)
        self.assertTrue(sihd.resources.get('logs') is None)
        sihd.resources.add(sihd.resources.get('tests'))
        filename = sihd.resources.get('test_sihd_api.py')
        dirname = sihd.resources.get('output')
        logger.info(filename)
        logger.info(dirname)
        self.assertTrue(filename)
        self.assertTrue(dirname)
        self.assertTrue(sihd.resources.get('output', fileonly=True) is None)

    #
    # Memory
    #

    def test_memory_usage(self):
        logger.info('Test memory usage')
        byte_before = sihd.sys.memory.usage_bytes()
        logger.info(sihd.sys.memory.usage_format(byte_before))
        lst = [i for i in range(1000000)]
        byte_after = sihd.sys.memory.usage_bytes()
        logger.info(sihd.sys.memory.usage_format(byte_after))
        self.assertTrue(byte_after > byte_before)

    #
    # Ping
    #

    @unittest.skipIf(os.getuid() != 0, "Not root")
    def test_ping(self):
        logger.info("Test ping")
        delay = sihd.network.ip.ping('google.com')
        logger.info("Delay: {} ms".format(round(delay, 4)))
        self.assertTrue(delay > 0.0)

    #
    # UDP
    #

    def udp_read(self, readable):
        sock = readable[0]
        data = sock.recv(4096).decode()
        logger.info("Data = " + data)
        self.data = data

    def test_ip_udp(self):
        logger.info("Testing UDP")
        self.data = ""
        self.serv = sihd.network.ip.Server(4200, 'datagram', 'ipv4', on_read=self.udp_read)
        self.assertTrue(sihd.network.ip.send_udp('localhost', 4200, "1234"))
        self.serv.select(timeout=1)
        self.serv.close()
        self.assertEqual(self.data, '1234')

    #
    # TCP
    #

    def tcp_read(self, readable):
        logger.info("Reading")
        for sock in readable:
            if sock == self.serv.socket:
                co, addr = self.serv.accept()
                logger.info("Accepting " + str(addr))
                self.serv.inputs.append(co)
            else:
                data = sock.recv(4096).decode()
                logger.info("Data = " + data)
                self.data = data

    def test_ip_tcp(self):
        logger.info("Testing TCP")
        self.data = ""
        self.serv = sihd.network.ip.Server(4200, 'stream', 'ipv4', on_read=self.tcp_read)
        self.socket = sihd.network.ip.make_tcp_socket('localhost', 4200)
        self.assertTrue(self.socket is not None)
        self.serv.select(timeout=1)
        self.socket.sendall(b'test')
        self.socket.close()
        self.serv.select(timeout=1)
        self.serv.close()
        self.assertEqual(self.data, 'test')

    def test_ip_tcp2(self):
        logger.info("Testing TCP2")
        self.data = ""
        self.serv = sihd.network.ip.Server(4200, 'stream', 'ipv4', on_read=self.tcp_read)
        self.assertTrue(sihd.network.ip.send_tcp('localhost', 4200, 'test2'))
        self.serv.select(timeout=1, loop=2)
        self.serv.close()
        self.assertEqual(self.data, 'test2')

    #
    # HTTP
    #

    def test_http_server(self):
        logger.info("Testing http server")
        serv = sihd.network.http.server('.', port=8080)
        serv.start()
        time.sleep(0.2)
        html = sihd.network.http.get('http://localhost:8080')
        self.assertTrue(html)
        html = html.decode()
        print(html)
        self.assertTrue(html.find('setup.py') > 0)
        serv.stop()

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_http_wrong_url(self):
        logger.info("Testing wrong url")
        url = "http://www.a@E2e2esasfzzzzzzzzzzz.com"
        try:
            html = sihd.network.http.get(url).decode()
        except RuntimeError:
            html = None
        self.assertTrue(html is None)

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_http_error_404(self):
        logger.info("Testing error 404")
        url = "http://httpbin.org/status/404"
        try:
            html = sihd.network.http.get(url).decode()
        except RuntimeError:
            html = None
        self.assertTrue(html is None)

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_http_get(self):
        logger.info("Testing GET")
        url = "http://httpbin.org/get"
        json_data = sihd.network.http.get(url, {'get': 'cookies'}).decode()
        print(json_data)
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["url"], "{}?get=cookies".format(url))
        self.assertEqual(dic["args"]['get'], 'cookies')

    @unittest.skipIf(is_internet is False, "Need internet")
    def test_http_post(self):
        logger.info("Testing POST")
        url = "http://httpbin.org/post"
        json_data = sihd.network.http.post(url, {'hello': 'world'}).decode()
        self.assertTrue(json_data is not None)
        dic = json.loads(json_data)
        self.assertEqual(dic["form"]['hello'], 'world')

if __name__ == '__main__':
    unittest.main(verbosity=2)
