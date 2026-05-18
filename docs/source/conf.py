# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'AMD Zephyr EC'
copyright = '2023, Advanced Micro Devices, Inc. All rights reserved.'
author = 'Advanced Micro Devices, Inc.'
# Revision of conf.py
release = '0.01'

# Revision of AMD Zephyr OS Embedded Controller User Guide
version = '0.01'

from pathlib import Path

# Helper function:


# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration


templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    "logo_only": False,
    "prev_next_buttons_location": None
}

html_title = "AMD Zephyr EC Documentation"
html_logo = "_static/AMD_Logo.svg"
html_static_path = ['_static']
html_show_sourcelink = False
html_show_sphinx = False

# -- Options for Sphinx Confluence Builder -----------------------------------
# https://sphinxcontrib-confluencebuilder.readthedocs.io/en/stable/configuration/
 
