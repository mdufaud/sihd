from flask import (
    Blueprint, flash, g, redirect, render_template,
    render_template_string, request, session, url_for
)

notebook = Blueprint("notebook", __name__, url_prefix='/notebook')

@notebook.route('/page/<int:idx>')
def page(idx):
    return '''Seems good -> %d ''' % idx

@notebook.route('/')
def index():
    return render_template_string('''
        <div>Getting app name {{ sihd_gui.get_name() }}</div>
        <div>Injected value (should be 42): {{ VALUE }}</div>
        <div><a href="{{ url_for('notebook.page', idx=4) }}">Check page 4</a></div>
    ''')
