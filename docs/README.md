<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/facebook/igl/blob/main/.github/igl-full-color-white.svg?raw=true">
    <source media="(prefers-color-scheme: light)" srcset="https://github.com/facebook/igl/blob/main/.github/igl-full-color-black.svg?raw=true">
    <img alt="IGL Logo" src=".github/igl-full-color-black.svg" width="500">
  </picture>

  [![Build Status](https://github.com/facebook/igl/actions/workflows/doc-build.yml/badge.svg)](https://github.com/facebook/igl/actions)

</div>

IGL Documentation Source
========================

This is the documentation source for [IGL](https://www.github.com/facebook/igl)

## Building the documentation

The documentation template is based on [Sphinx](https://www.sphinx-doc.org/) and [Furo](https://github.com/pradyunsg/furo)
and requires [Python](https://www.python.org/) and [virtualenv](https://virtualenv.pypa.io/en/latest/).

The process described below works on the Mac. Building the documentation on Windows may require significant more work to adjust the version
of the packages listed in the [requirements.txt](https://github.com/facebook/igl/blob/main/doc/requirements.txt) file.
The process of building the documentation on Linux has not been tested.

1. Set up a Python virtual environment using `virtualenv` and activate it:
```shell-script
virtualenv venv
source ./venv/bin/activate
```

2. Once in the virtual environment, install the required Python packages:
```shell-script
pip install -r requirements.txt
```

3. Build the website using make.
```shell-script
make html
```

The generated website will be output to [docs/build/html](https://github.com/facebook/igl/tree/gh-pages).
