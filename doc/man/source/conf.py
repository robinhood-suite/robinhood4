# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'RobinHood v4'
copyright = '2025, CEA/DAM'
author = 'CEA/DAM'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

language = 'en'

extensions = []

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This patterns also effect to html_static_path and html_extra_path
exclude_patterns = []

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
source_suffix = '.rst'
source_encoding = 'utf-8-sig'
master_doc = 'robinhood4'

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = False

# -- Options for manual page output ---------------------------------------

rst_prolog = ''

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    ('rbh-sync', 'rbh-sync', 'rbh-sync tool', [author], 1),
    ('rbh-find', 'rbh-find', 'rbh-find tool', [author], 1),
    ('rbh-fsevents', 'rbh-fsevents', 'rbh-fsevents tool', [author], 1),
    ('rbh-report', 'rbh-report', 'rbh-report tool', [author], 1),
    ('rbh-info', 'rbh-info', 'rbh-info tool', [author], 1),
    ('rbh-undelete', 'rbh-undelete', 'rbh-undelete tool', [author], 1),
    ('robinhood4',  'robinhood4', 'general RobinHood4 knowledge', [author], 7),
    ('robinhood4-conf',  'robinhood4', 'RobinHood4\'s configuration file',
     [author], 5),
    ('rbh-update-path', 'rbh-update-path', 'rbh-update-path tool', [author], 1),
]

smartquotes = False
