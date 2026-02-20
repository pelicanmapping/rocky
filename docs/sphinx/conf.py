# Configuration file for the Sphinx documentation builder.
# Rocky SDK Documentation

import os
import sys
from pathlib import Path

# -- Project information -----------------------------------------------------

project = 'Rocky SDK'
copyright = '2025, Pelican Mapping'
author = 'Pelican Mapping'
version = '0.9.8'
release = '0.9.8'

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',              # Doxygen integration
    'exhale',               # Auto-generate API docs
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'sphinx.ext.coverage',
    'sphinx.ext.mathjax',
    'sphinx.ext.viewcode',
    'sphinx.ext.graphviz',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
html_logo = None
html_favicon = None

html_theme_options = {
    'canonical_url': '',
    'analytics_id': '',
    'logo_only': False,
    'display_version': True,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    'vcs_pageview_mode': '',
    'style_nav_header_background': '#2980B9',
    # Toc options
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False
}

# Add custom CSS for better code rendering
html_css_files = [
    'custom.css',
]

# -- Breathe configuration ---------------------------------------------------
# Breathe reads Doxygen XML and makes it available to Sphinx

breathe_projects = {
    "Rocky": "../doxygen/xml/"
}
breathe_default_project = "Rocky"
breathe_default_members = ('members', 'undoc-members')
breathe_domain_by_extension = {
    "h": "cpp",
    "hpp": "cpp",
    "cpp": "cpp"
}
breathe_show_define_initializer = True
breathe_show_enumvalue_initializer = True

# -- Exhale configuration ----------------------------------------------------
# Exhale auto-generates Sphinx pages for the entire API tree

from exhale import utils

exhale_args = {
    # Required arguments
    "containmentFolder":     "./api",
    "rootFileName":          "library_root.rst",
    "rootFileTitle":         "API Reference",
    "doxygenStripFromPath":  "../../src",

    # Suggested optional arguments
    "createTreeView":        True,

    # We run Doxygen separately in the CI/CD pipeline
    "exhaleExecutesDoxygen": False,

    # Use our Doxyfile
    "exhaleUseDoxyfile":     True,

    # Mapping of files to custom page names
    "customSpecificationsMapping": utils.makeCustomSpecificationsMapping(
        lambda file_name: file_name.replace("_8h", "")
    ),

    # SELECTIVE DOCUMENTATION: Control what Exhale generates
    # These are the kinds of things we want to show in the API tree
    "unabridgedOrphanKinds": {"namespace", "class", "struct"},
    "kindsWithContainmentFolders": ["namespace", "class", "struct"],

    # Exclude detail/internal namespaces from listings
    "listingExclude": [r".*detail.*", r".*internal.*"],

    # File organization
    "fullToctreeMaxDepth": 3,
    "contentsDirectives": True,

    # API organization
    "includeTemplateParamOrderList": True,

    # Description to show at the top of the API index
    "afterTitleDescription": """
    The Rocky SDK provides a complete C++ framework for rendering maps and globes with real geospatial data.

    **Main Modules:**

    - **rocky** - Core map and layer functionality
    - **rocky::ecs** - Entity Component System for annotations
    - **rocky::vsg** - Vulkan SceneGraph rendering backend
    """,
}

# Tell sphinx what the primary language being documented is.
primary_domain = 'cpp'

# Tell sphinx what the pygments highlight language should be.
highlight_language = 'cpp'

# -- Extension configuration -------------------------------------------------

# Configure todo extension
todo_include_todos = True

# -- Intersphinx configuration -----------------------------------------------
# Link to external documentation

intersphinx_mapping = {
    'python': ('https://docs.python.org/3', None),
    'cpp': ('https://en.cppreference.com/w/', None),
}

# -- Custom setup ------------------------------------------------------------

def setup(app):
    app.add_css_file('custom.css')
