#!/usr/bin/python
# coding: utf-8

""" System """
import os
import sys
import time
import unittest

import utils
import sihd
logger = sihd.log.setup('info')

from sihd.gui.flask.AFlaskGui import AFlaskGui
from sihd.app.SihdApp import SihdApp
from sihd.readers.AReader import AReader

try:
    from markupsafe import escape
    from flask import (
        render_template,
        session, redirect, url_for, request,
        make_response, abort, Blueprint, g, flash
    )
    imprt = None
except ImportError as imprt:
    pass

class SimpleFlask(AFlaskGui):

    def __init__(self, name="SimpleFlask", app=None):
        super(SimpleFlask, self).__init__(app=app, name=name)
        self.render_ctx['sihd_gui'] = self
        self.render_ctx['channels'] = {}

    def handle(self, channel):
        self.render_ctx['channels'][channel.get_path()] = channel.read()

    #
    # Base
    #

    def hello(self):
        return "Hello to you too"

    def index(self):
        flash("Testing errors")
        flash("Adding error")
        flash("Third flash's the flash")
        return render_template("index.html")

    #
    # Errors
    #

    def not_found(self, error):
        resp = make_response('''Self made error 404''', 404)
        resp.headers['X-Something'] = 'A value'
        return resp

    #
    # Tests
    #

    def show_user_profile(self, username):
        # show the user profile for that user
        return 'User %s' % escape(username)

    def show_post(self, post_id):
        # show the post with the given id, the id is an integer
        return 'Post %d' % post_id

    def show_subpath(self, subpath):
        # show the subpath after /path/
        return 'Subpath %s' % escape(subpath)

    def do_template(self):
        return '''
            <div>
                <button>This is a string template</button>
            </div>
        '''

    #
    # Auth
    #

    def load_user(self):
        user_id = session.get('username')
        if user_id is None:
            g.user = None
        else:
            self.log_info("Loading user", session['username'])
            g.user = {'username': session['username']}

    def register(self):
        if request.method == "POST":
            session['username'] = request.form['username']
            session['password'] = request.form['password']
            return redirect(url_for('auth.login'))
        else:
            return render_template("register.html")

    def login(self):
        if request.method == "POST":
            if session['username'] == request.form['username']\
                and session['password'] == request.form['password']:
                return redirect(url_for('index'))
        return render_template("login.html")

    def logout(self):
        session.clear()
        return redirect(url_for('index'))

    def build_routes(self, app):
        # Base
        self.route("hello", "/hello")
        self.route("index", "/")
        # Declaring and filling a blueprint
        auth_bp = Blueprint('auth', self.configuration.get('web_path'), url_prefix='/auth')
        self.route("register", "/register", methods=["POST", "GET"], base=auth_bp)
        self.route("login", "/login", methods=["POST", "GET"], base=auth_bp)
        self.route("logout", "/logout", methods=["POST", "GET"], base=auth_bp)
        self.load_user = auth_bp.before_app_request(self.load_user)
        app.register_blueprint(auth_bp)
        # Tests
        self.route("do_template", "/template")
        self.route("show_user_profile", '/user/<username>')
        self.route("show_post", '/post/<int:post_id>')
        self.route("show_subpath", '/path/<path:subpath>')
        # Errors
        self.errorhandler("not_found", 404)
        # Loading a blueprint from elsewhere
        from resources.gui.flask.blueprint import notebook
        # Can change path this way if need be
        notebook.root_path = self.configuration.get('web_path')
        # Can add value in templating
        self.add_context_processor(notebook, method=lambda: {"VALUE": 42})
        app.register_blueprint(notebook)

class SimpleApp(SihdApp):

    def __init__(self, name):
        super(SimpleApp, self).__init__(name)
        dirname = os.path.dirname
        join = os.path.join
        self.set_app_path(sihd.resources.get('tests', 'output'))

    def service_state_changed(self, service, stopped, paused):
        if self.is_gui(service) is False:
            return
        if stopped:
            self.stop()

    def build_services(self):
        self.gui = SimpleFlask(app=self)
        self.gui.configuration.load({
            "web_path": sihd.resources.get("www"),
            "flask": {
                "TESTING": True,
            },
        })
        return True

    def on_init(self):
        self.add_state_observer(self.gui)

class InfiniteReader(AReader):

    def __init__(self, *args, **kwargs):
        super(InfiniteReader, self).__init__(*args, **kwargs)
        self.configuration.set("runnable_frequency", 10)
        self.add_channel('output', type='counter')

    def on_step(self):
        self.output.write()
        return True

class TestFlaskGui(unittest.TestCase):

    def setUp(self):
        sihd.resources.add("tests", "resources", "gui", "flask")
        print()

    def tearDown(self):
        pass

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui(self):
        print()
        provider = InfiniteReader(name='provider')
        gui = SimpleFlask()
        gui.configuration.load({
            "web_path": sihd.resources.get("www"),
            "flask": {
                "TESTING": True,
            },
        })
        self.assertTrue(gui.setup())
        self.assertTrue(provider.start())
        channel = gui.find('provider.output')
        channel.add_observer(gui)
        print(gui.flask_app.url_map)
        self.assertTrue(gui.start())
        try:
            while gui.is_running():
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
        self.assertTrue(gui.stop())
        self.assertTrue(provider.stop())

    @unittest.skipIf(not sys.stdin or not sys.stdin.isatty(), "Not interactive test")
    def test_gui_app(self):
        print()
        app = SimpleApp("SomeApp")
        if app.setup_app() is False:
            return
        print(app.gui.flask_app.url_map)
        app.start()
        logger.warning("Timeout is 5 seconds")
        app.loop(timeout=5)
        print(app.get_conf())
        app.stop()

if __name__ == '__main__':
    if imprt is None:
        unittest.main(verbosity=2)
    else:
        logger.error("Requirements not installed: {}".format(imprt))
