# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'RobinHood v4'
copyright = '2026, CEA/DAM. The RobinHood4 tool suite is distributed under the GNU Lesser General Public License v3.0 or later.'
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
    ('rbh-sync', 'rbh-sync',
     'synchronize metadata between RobinHood4 backends - RobinHood4 tool suite',
     [author], 1),
    ('rbh-find', 'rbh-find',
     'query and filter metadata from RobinHood4 backends - RobinHood4 tool suite',
     [author], 1),
    ('rbh-fsevents', 'rbh-fsevents',
     'process and manage storage system events - RobinHood4 tool suite',
     [author], 1),
    ('rbh-report', 'rbh-report',
     'generate aggregated reports from Robinhood4 backends - RobinHood4 tool suite',
     [author], 1),
    ('rbh-info', 'rbh-info',
     'display information about RobinHood4 backends and their metadata - RobinHood4 tool suite',
     [author], 1),
    ('rbh-undelete', 'rbh-undelete',
     'restore deleted entries using archived metadata in RobinHood4 backends - RobinHood4 tool suite',
     [author], 1),
    ('robinhood4', 'robinhood4',
     'general knowledge about the RobinHood4 tool suite',
     [author], 7),
    ('robinhood4-conf', 'robinhood4',
     'configuration file for the RobinHood4 tool suite',
     [author], 5),
    ('rbh-update-path', 'rbh-update-path', 'rbh-update-path tool', [author], 1),
]

smartquotes = False
