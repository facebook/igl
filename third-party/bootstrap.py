#!/usr/bin/python3

#
# https://github.com/corporateshark/bootstrapping.git
# sk@linderdaum.com
#
# The MIT License (MIT)
# Copyright (c) 2016-2023, Sergey Kosarevsky
#
# ---
# Based on https://bitbucket.org/blippar/bootstrapping-external-libs
#
# The MIT License (MIT)
# Copyright (c) 2016 Blippar.com Ltd
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

from __future__ import print_function
import platform
import os
import sys
import io
import shutil
import subprocess
import zipfile
import tarfile
import hashlib
import json
import getopt
import traceback
import urllib
import ssl

ssl._create_default_https_context = ssl._create_unverified_context

try:
    from urllib.request import urlparse
    from urllib.request import urlunparse
    from urllib.request import quote
except ImportError:
    from urlparse import urlparse
    from urlparse import urlunparse
    from urllib import URLopener
    from urllib import quote

try:
    import paramiko
    import scp
    scp_available = True
except:
    scp_available = False
    print("WARNING: Please install the Python packages [paramiko, scp] for full script operation.")

try:
    import lzma
    lzma_available = True
except:
    print("WARNING: Python lzma library not available; extraction of .tar.xz files may not be supported.")
    print("Installation on Ubuntu:")
    print("> apt-get install python-lzma")
    print("Installation on Mac OS X:")
    print("> brew install xz")
    print("> pip install pyliblzma")
    lzma_available = False

SRC_DIR_BASE = "src"
ARCHIVE_DIR_BASE = "archives"
SNAPSHOT_DIR_BASE = "snapshots"

BASE_DIR = os.getcwd()
SRC_DIR = os.path.join(BASE_DIR, SRC_DIR_BASE)
ARCHIVE_DIR = os.path.join(BASE_DIR, ARCHIVE_DIR_BASE)
SNAPSHOT_DIR = os.path.join(BASE_DIR, SNAPSHOT_DIR_BASE)

DEFAULT_PNUM = 3
DEBUG_OUTPUT = False
FALLBACK_URL = ""

USE_TAR = False
USE_UNZIP = False

TOOL_COMMAND_PYTHON = sys.executable if not " " in sys.executable else '"{}"'.format(sys.executable)
TOOL_COMMAND_GIT = "git"
TOOL_COMMAND_HG = "hg"
TOOL_COMMAND_SVN = "svn"
TOOL_COMMAND_PATCH = "patch"
TOOL_COMMAND_TAR = "tar"
TOOL_COMMAND_UNZIP = "unzip"

if platform.system() == "Windows":
    os.environ['CYGWIN'] = "nodosfilewarning"

if not sys.version_info[0] >= 3:
    raise ValueError("I require Python 3.0 or a later version")

def log(string):
    print("--- " + string)


def dlog(string):
    if DEBUG_OUTPUT:
        print("*** " + string)

def executeCommand(command, printCommand = False, quiet = False):

    printCommand = printCommand or DEBUG_OUTPUT
    out = None
    err = None

    if quiet:
        out = open(os.devnull, 'w')
        err = subprocess.STDOUT

    if printCommand:
        if DEBUG_OUTPUT:
            dlog(">>> " + command)
        else:
            log(">>> " + command)

    return subprocess.call(command, shell = True, stdout=out, stderr=err);


def dieIfNonZero(res):
    if res != 0:
        raise ValueError("Command returned non-zero status: " + str(res));

def escapifyPath(path):
    if path.find(" ") == -1:
        return path
    if platform.system() == "Windows":
        return "\"" + path + "\""
    return path.replace("\\ ", " ")

def cloneRepository(type, url, target_name, revision = None, try_only_local_operations = False):
    target_dir = escapifyPath(os.path.join(SRC_DIR, target_name))
    target_dir_exists = os.path.exists(target_dir)
    log("Cloning " + url + " to " + target_dir)

    if type == "hg":
        repo_exists = os.path.exists(os.path.join(target_dir, ".hg"))

        if not repo_exists:
            if try_only_local_operations:
                raise RuntimeError("Repository for " + target_name + " not found; cannot execute local operations only")
            if target_dir_exists:
                dlog("Removing directory " + target_dir + " before cloning")
                shutil.rmtree(target_dir)
            dieIfNonZero(executeCommand(TOOL_COMMAND_HG + " clone " + url + " " + target_dir))
        elif not try_only_local_operations:
            log("Repository " + target_dir + " already exists; pulling instead of cloning")
            dieIfNonZero(executeCommand(TOOL_COMMAND_HG + " pull -R " + target_dir))

        if revision is None:
            revision = ""
        dieIfNonZero(executeCommand(TOOL_COMMAND_HG + " update -R " + target_dir + " -C " + revision))
        dieIfNonZero(executeCommand(TOOL_COMMAND_HG + " purge -R " + target_dir + " --config extensions.purge="))

    elif type == "git":
        repo_exists = os.path.exists(os.path.join(target_dir, ".git"))

        if not repo_exists:
            if try_only_local_operations:
                raise RuntimeError("Repository for " + target_name + " not found; cannot execute local operations only")
            if target_dir_exists:
                dlog("Removing directory " + target_dir + " before cloning")
                shutil.rmtree(target_dir)
            dieIfNonZero(executeCommand(TOOL_COMMAND_GIT + " clone --recursive " + url + " " + target_dir))
        elif not try_only_local_operations:
            log("Repository " + target_dir + " already exists; fetching instead of cloning")
            dieIfNonZero(executeCommand(TOOL_COMMAND_GIT + " -C " + target_dir + " fetch --recurse-submodules"))

        if revision is None:
            revision = "HEAD"
        dieIfNonZero(executeCommand(TOOL_COMMAND_GIT + " -C " + target_dir + " reset --hard " + revision))
        dieIfNonZero(executeCommand(TOOL_COMMAND_GIT + " -C " + target_dir + " clean -fxd"))

    elif type == "svn":
        if not try_only_local_operations: # we can't do much without a server connection when dealing with SVN
            if target_dir_exists:
                dlog("Removing directory " + target_dir + " before cloning")
                shutil.rmtree(target_dir)
            dieIfNonZero(executeCommand(TOOL_COMMAND_SVN + " checkout " + url + " " + target_dir))

        if revision is not None and revision != "":
            raise RuntimeError("Updating to revision not implemented for SVN.")

    else:
        raise ValueError("Cloning " + type + " repositories not implemented.")


def decompressTarXZFile(src_filename, dst_filename):
    if not lzma_available:
        raise RuntimeError("lzma extraction not available; please install package lzma (pyliblzma) and try again")

    try:
        fs = open(src_filename, "rb")
        if not fs:
            raise RuntimeError("Opening file " + src_filename + " failed")
        fd = open(dst_filename, "wb")
        if not fd:
            raise RuntimeError("Opening file " + dst_filename + " failed")

        decompressed = lzma.decompress(fs.read())
        fd.write(decompressed)
    finally:
        fs.close()
        fd.close()



def extractFile(filename, target_dir):
    if os.path.exists(target_dir):
        shutil.rmtree(target_dir)

    log("Extracting file " + filename)
    stem, extension = os.path.splitext(os.path.basename(filename))

    if extension == ".zip" or extension == "":
        zfile = zipfile.ZipFile(filename)
        extract_dir = os.path.commonprefix(zfile.namelist())
        hasFolder = False
        for fname in zfile.namelist():
            if fname.find('/') != -1:
                hasFolder = True
        extract_dir_local = ""
        if not hasFolder: # special case, there are no folders in the archive
            extract_dir = ""
        if extract_dir == "":  # deal with stupid zip files that don't contain a base directory
            extract_dir, extension2 = os.path.splitext(os.path.basename(filename))
            extract_dir_local = extract_dir
        extract_dir_abs = os.path.join(SRC_DIR, extract_dir_local)

        try:
            os.mkdirs(extract_dir_abs)
        except:
            pass

        if not USE_UNZIP:
            zfile.extractall(extract_dir_abs)
            zfile.close()
        else:
            zfile.close()
            dieIfNonZero(executeCommand(TOOL_COMMAND_UNZIP + " " + filename + " -d " + extract_dir_abs))

    elif extension == ".tar" or extension == ".gz" or extension == ".bz2" or extension == ".xz":

        if extension == ".xz":# and not lzma_available:
            stem2, extension2 = os.path.splitext(os.path.basename(stem))
            if extension2 == ".tar":
                # we extract the .tar.xz file to a .tar file before we uncompress that
                tar_filename = os.path.join(os.path.dirname(filename), stem)
                decompressTarXZFile(filename, tar_filename)
                filename = tar_filename
            else:
                raise RuntimeError("Unable to extract .xz file that is not a .tar.xz file.")

        tfile = tarfile.open(filename)
        extract_dir = os.path.commonprefix(tfile.getnames())
        extract_dir_local = ""
        if extract_dir == "":  # deal with stupid tar files that don't contain a base directory
            extract_dir, extension2 = os.path.splitext(os.path.basename(filename))
            extract_dir_local = extract_dir
        extract_dir_abs = os.path.join(SRC_DIR, extract_dir_local)

        try:
            os.mkdirs(extract_dir_abs)
        except:
            pass

        if not USE_TAR:
            tfile.extractall(extract_dir_abs)
            tfile.close()
        else:
            tfile.close()
            dieIfNonZero(executeCommand(TOOL_COMMAND_TAR + " -x -f " + filename + " -C " + extract_dir_abs))

    else:
        raise RuntimeError("Unknown compressed file format " + extension)

    if platform.system() == "Windows":
        extract_dir = extract_dir.replace( '/', '\\' )
        target_dir = target_dir.replace( '/', '\\' )
        if extract_dir[-1::] == '\\':
            extract_dir = extract_dir[:-1]

    # rename extracted folder to target_dir
    extract_dir_abs = os.path.join(SRC_DIR, extract_dir)

    needRename = True

    if platform.system() == "Windows":
       needRename = extract_dir_abs.lower() != target_dir.lower()

    if needRename: os.rename(extract_dir_abs, target_dir)


def createArchiveFromDirectory(src_dir_name, archive_name, delete_existing_archive = False):
    if delete_existing_archive and os.path.exists(archive_name):
        dlog("Removing snapshot file " + archive_name + " before creating new one")
        os.remove(archive_name)

    archive_dir = os.path.dirname(archive_name)
    if not os.path.isdir(archive_dir):
        os.mkdir(archive_dir)

    with tarfile.open(archive_name, "w:gz") as tar:
        tar.add(src_dir_name, arcname = os.path.basename(src_dir_name))


def downloadSCP(hostname, username, path, target_dir):
    if not scp_available:
        log("ERROR: missing Python packages [paramiko, scp]; cannot continue.")
        raise RuntimeError("Missing Python packages [paramiko, scp]; cannot continue.")
    ssh = paramiko.SSHClient()
    ssh.load_system_host_keys()
    ssh.connect(hostname = hostname, username = username)
    scpc = scp.SCPClient(ssh.get_transport())
    scpc.get(path, local_path = target_dir);

def downloadProgress(cur_size, total_size):
    percent = int((cur_size / total_size)*100)
    print("[", end = "")
    for i in range(int(percent/2)):
        print("*", end = "")
    for i in range(int(percent/2), 50):
        print(".", end = "")
    print("] " + str(percent) + "% --- ", end = "")
    print("%.2f" % (cur_size / (1024*1024)), "Mb", end = "\r")

def computeFileHash(filename):
    blocksize = 65536
    hasher = hashlib.sha1()
    with open(filename, 'rb') as afile:
        buf = afile.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = afile.read(blocksize)
    return hasher.hexdigest()

def downloadFile(url, download_dir, target_dir_name, sha1_hash = None, force_download = False, user_agent = None):
    if not os.path.isdir(download_dir):
        os.mkdir(download_dir)

    p = urlparse(url)
    url = urlunparse([p[0], p[1], quote(p[2]), p[3], p[4], p[5]]) # replace special characters in the URL path

    filename_rel = os.path.split(p.path)[1] # get original filename
    target_filename = os.path.join(download_dir, filename_rel)

    # check SHA1 hash, if file already exists
    if os.path.exists(target_filename) and sha1_hash is not None and sha1_hash != "":
        hash_file = computeFileHash(target_filename)
        if hash_file != sha1_hash:
            log("Hash of " + target_filename + " (" + hash_file + ") does not match expected hash (" + sha1_hash + "); forcing download")
            force_download = True

    # download file
    if (not os.path.exists(target_filename)) or force_download:
        log("Downloading " + url + " to " + target_filename)
        if p.scheme == "ssh":
            downloadSCP(p.hostname, p.username, p.path, download_dir)
        else:
            opener = urllib.request.build_opener()
            if user_agent is not None:
                opener.addheaders = [('User-agent', user_agent)]
            f = open(target_filename, 'wb')
            with opener.open(url) as response:
                Length = response.getheader('content-length')
                BlockSize = 128*1024 # default value
                if Length:
                    Length = int(Length)
                    BlockSize = max(BlockSize, Length // 1000)
                    Size = 0
                    while True:
                        Buffer = response.read(BlockSize)
                        if not Buffer:
                            break
                        f.write(Buffer)
                        Size += len(Buffer)
                        downloadProgress(Size, Length)
                    print();
                else:
                    f.write(response.read())
            f.close()
    else:
        log("Skipping download of " + url + "; already downloaded")

    # check SHA1 hash
    if sha1_hash is not None and sha1_hash != "":
        hash_file = computeFileHash(target_filename)
        if hash_file != sha1_hash:
            errorStr = "Hash of " + target_filename + " (" + hash_file + ") differs from expected hash (" + sha1_hash + ")"
            log(errorStr)
            raise RuntimeError(errorStr)

    return target_filename


def downloadAndExtractFile(url, download_dir, target_dir_name, sha1_hash = None, force_download = False, user_agent = None):
    target_filename = downloadFile(url, download_dir, target_dir_name, sha1_hash, force_download, user_agent)
    extractFile(target_filename, os.path.join(SRC_DIR, target_dir_name))


def applyPatchFile(patch_name, dir_name, pnum):
    # we're assuming the patch was applied like in this example:
    # diff --exclude=".git" --exclude=".hg" -rupN ./src/AGAST/ ./src/AGAST_patched/ > ./patches/agast.patch
    # where the first given location is the unpatched directory, and the second location is the patched directory.
    log("Applying patch to " + dir_name)
    patch_dir = os.path.join(BASE_DIR, "patches")
    arguments = "-d " + os.path.join(SRC_DIR, dir_name) + " -p" + str(pnum) + " < " + os.path.join(patch_dir, patch_name)
    argumentsBinary = "-d " + os.path.join(SRC_DIR, dir_name) + " -p" + str(pnum) + " --binary < " + os.path.join(patch_dir, patch_name)
    res = executeCommand(TOOL_COMMAND_PATCH + " --dry-run " + arguments, quiet = True)
    if res != 0:
        arguments = argumentsBinary
        res = executeCommand(TOOL_COMMAND_PATCH + " --dry-run " + arguments, quiet = True)
    if res != 0:
        log("ERROR: patch application failure; has this patch already been applied?")
        executeCommand(TOOL_COMMAND_PATCH + " --dry-run " + arguments, printCommand = True)
        exit(255)
    else:
        dieIfNonZero(executeCommand(TOOL_COMMAND_PATCH + " " + arguments, quiet = True))


def runPythonScript(script_name):
    log("Running Python script " + script_name)
    patch_dir = os.path.join(BASE_DIR, "patches")
    filename = os.path.join(patch_dir, script_name)
    dieIfNonZero(executeCommand(TOOL_COMMAND_PYTHON + " " + escapifyPath(filename), False));


def findToolCommand(command, paths_to_search, required = False):
    command_res = command
    found = False

    for path in paths_to_search:
        command_abs = os.path.join(path, command)
        if os.path.exists(command_abs):
            command_res = command_abs
            found = True
            break;

    if required and not found:
        log("WARNING: command " + command + " not found, but required by script")

    dlog("Found '" + command + "' as " + command_res)
    return command_res


def readJSONData(filename):
    try:
        json_data = open(filename).read()
    except:
        log("ERROR: Could not read JSON file: " + filename)
        return None

    try:
        data = json.loads(json_data)
    except json.JSONDecodeError as e:
        log("ERROR: Could not parse JSON document: {}\n    {} (line {}:{})\n".format(filename, e.msg, e.lineno, e.colno))
        return None
    except:
        log("ERROR: Could not parse JSON document: " + filename)
        return None

    return data


def writeJSONData(data, filename):
    with open(filename, 'w') as outfile:
        json.dump(data, outfile)


def listLibraries(data):
    for library in data:
        name = library.get('name', None)
        if name is not None:
            print(name)


def printOptions():
        print("--------------------------------------------------------------------------------")
        print("Downloads external libraries, and applies patches or scripts if necessary.")
        print("If the --name argument is not provided, all available libraries will be")
        print("downloaded.")
        print("")
        print("Options:")
        print("  --list, -l              List all available libraries")
        print("  --name, -n              Specifies the name of a single library to be")
        print("                          downloaded")
        print("  --name-file, -N         Specifies a file that contains a (sub)set of libraries")
        print("                          to be downloaded. One library name per line; lines")
        print("                          starting with '#' are considered comments.")
        print("  --skip                  Specifies a name of a single library to be skipped")
        print("  --clean, -c             Remove library directory before obtaining library")
        print("  --clean-all, -C         Implies --clean, and also forces re-download of cached")
        print("                          archive files")
        print("  --base-dir, -b          Base directory, if script is called from outside of")
        print("                          its directory")
        print("  --bootstrap-file        Specifies the file containing the canonical bootstrap")
        print("                          JSON data (default: bootstrap.json)")
        print("  --local-bootstrap-file  Specifies the file containing local bootstrap JSON")
        print("                          data (e.g. for a particular project). The data in this")
        print("                          file will have higher precedence than the data from")
        print("                          the canonical bootstrap file.")
        print("  --use-tar               Use 'tar' command instead of Python standard library")
        print("                          to extract tar archives")
        print("  --use-unzip             Use 'unzip' command instead of Python standard library")
        print("                          to extract zip archives")
        print("  --repo-snapshots        Create a snapshot archive of a repository when its")
        print("                          state changes, e.g. on a fallback location")
        print("  --fallback-url          Fallback URL that points to an existing and already")
        print("                          bootstrapped `external` repository that may be used to")
        print("                          retrieve otherwise unobtainable archives or")
        print("                          repositories. The --repo-snapshots option must be")
        print("                          active on the fallback server. Allowed URL schemes are")
        print("                          file://, ssh://, http://, https://, ftp://.")
        print("  --force-fallback        Force using the fallback URL instead of the original")
        print("                          sources")
        print("  --debug-output          Enables extra debugging output")
        print("  --break-on-first-error  Terminate script once the first error is encountered")
        print("--------------------------------------------------------------------------------")


def main(argv):
    global BASE_DIR, SRC_DIR, ARCHIVE_DIR, DEBUG_OUTPUT, FALLBACK_URL, USE_TAR, USE_UNZIP
    global TOOL_COMMAND_PYTHON, TOOL_COMMAND_GIT, TOOL_COMMAND_HG, TOOL_COMMAND_SVN, TOOL_COMMAND_PATCH, TOOL_COMMAND_TAR, TOOL_COMMAND_UNZIP

    try:
        opts, args = getopt.getopt(
            argv,
            "ln:N:cCb:h",
            ["list", "name=", "name-file=", "skip=", "clean", "clean-all", "base-dir", "bootstrap-file=",
             "local-bootstrap-file=", "use-tar", "use-unzip", "repo-snapshots", "fallback-url=",
             "force-fallback", "debug-output", "help", "break-on-first-error"])
    except getopt.GetoptError:
        printOptions()
        return 0

    opt_names = []
    name_files = []
    skip_libs = []
    opt_clean = False
    opt_clean_archives = False
    list_libraries = False

    default_bootstrap_filename = "bootstrap.json"
    bootstrap_filename = os.path.abspath(os.path.join(BASE_DIR, default_bootstrap_filename))
    local_bootstrap_filename = ""
    create_repo_snapshots = False
    force_fallback = False
    break_on_first_error = False

    base_dir_path = ""

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            printOptions()
            return 0
        if opt in ("-l", "--list"):
            list_libraries = True
        if opt in ("-n", "--name"):
            opt_names.append(arg)
        if opt in ("-N", "--name-file"):
            name_files.append(os.path.abspath(arg))
        if opt in ("--skip",):
            skip_libs.append(arg)
        if opt in ("-c", "--clean"):
            opt_clean = True
        if opt in ("-C", "--clean-all"):
            opt_clean = True
            opt_clean_archives = True
        if opt in ("-b", "--base-dir"):
            base_dir_path = os.path.abspath(arg)
            BASE_DIR = base_dir_path
            SRC_DIR = os.path.join(BASE_DIR, SRC_DIR_BASE)
            ARCHIVE_DIR = os.path.join(BASE_DIR, ARCHIVE_DIR_BASE)
            bootstrap_filename = os.path.join(BASE_DIR, default_bootstrap_filename)
            log("Using " + arg + " as base directory")
        if opt in ("--bootstrap-file",):
            bootstrap_filename = os.path.abspath(arg)
            log("Using main bootstrap file " + bootstrap_filename)
        if opt in ("--local-bootstrap-file",):
            local_bootstrap_filename = os.path.abspath(arg)
            log("Using local bootstrap file " + local_bootstrap_filename)
        if opt in ("--use-tar",):
            USE_TAR = True
        if opt in ("--use-unzip",):
            USE_UNZIP = True
        if opt in ("--repo-snapshots",):
            create_repo_snapshots = True
            log("Will create repository snapshots")
        if opt in ("--fallback-url",):
            FALLBACK_URL = arg
        if opt in ("--force-fallback",):
            force_fallback = True
            log("Using fallback URL to fetch all libraries")
        if opt in ("--break-on-first-error",):
            break_on_first_error = True
        if opt in ("--debug-output",):
            DEBUG_OUTPUT = True

    if platform.system() != "Windows":
        # Unfortunately some IDEs do not have a proper PATH environment variable set,
        # so we search manually for the required tools in some obvious locations.
        paths_to_search = os.environ["PATH"].split(":") + ["/usr/local/bin", "/opt/local/bin", "/usr/bin"]
        TOOL_COMMAND_PYTHON = findToolCommand(TOOL_COMMAND_PYTHON, paths_to_search, required = True)
        TOOL_COMMAND_GIT = findToolCommand(TOOL_COMMAND_GIT, paths_to_search, required = True)
        TOOL_COMMAND_HG = findToolCommand(TOOL_COMMAND_HG, paths_to_search, required = True)
        TOOL_COMMAND_SVN = findToolCommand(TOOL_COMMAND_SVN, paths_to_search, required = True)
        TOOL_COMMAND_PATCH = findToolCommand(TOOL_COMMAND_PATCH, paths_to_search, required = True)
        TOOL_COMMAND_TAR = findToolCommand(TOOL_COMMAND_TAR, paths_to_search, required = USE_TAR)
        TOOL_COMMAND_UNZIP = findToolCommand(TOOL_COMMAND_UNZIP, paths_to_search, required = USE_UNZIP)

    if base_dir_path:
        os.chdir(base_dir_path)

    if name_files:
        for name_file in name_files:
            try:
                with open(name_file) as f:
                    opt_names_local = [l for l in (line.strip() for line in f) if l]
                    opt_names_local = [l for l in opt_names_local if l[0] != '#']
                    opt_names += opt_names_local
                    dlog("Name file contains: " + ", ".join(opt_names_local))
            except:
                log("ERROR: cannot parse name file " + name_file)
                return -1

    if force_fallback and not FALLBACK_URL:
        log("Error: cannot force usage of the fallback location without specifying a fallback URL")
        return -1;

    state_filename = os.path.join(os.path.dirname(os.path.splitext(bootstrap_filename)[0]), \
                                  "." + os.path.basename(os.path.splitext(bootstrap_filename)[0])) \
                     + os.path.splitext(bootstrap_filename)[1]

    dlog("bootstrap_filename = " + bootstrap_filename)
    dlog("state_filename     = " + state_filename)

    # read canonical libraries data
    data = readJSONData(bootstrap_filename)
    if data is None:
        return -1;

    # some sanity checking
    for library in data:
        if library.get('name', None) is None:
            log("ERROR: Invalid schema: library object does not have a 'name'")
            return -1

    # read local libraries data, if available
    local_data = None
    if local_bootstrap_filename:
        local_data = readJSONData(local_bootstrap_filename)

        if local_data is None:
            return -1;

        # some sanity checking
        for local_library in local_data:
            if local_library.get('name', None) is None:
                log("ERROR: Invalid schema: local library object does not have a 'name'")
                return -1

    # merge canonical and local library data, if applicable; local libraries take precedence
    if local_data is not None:
        for local_library in local_data:
            local_name = local_library.get('name', None)
            found_canonical_library = False
            for n, library in enumerate(data):
                name = library.get('name', None)
                if local_name == name:
                    data[n] = local_library # overwrite library
                    found_canonical_library = True
            if not found_canonical_library:
                data.append(local_library)

    if list_libraries:
        listLibraries(data)
        return 0

    sdata = []
    if os.path.exists(state_filename):
        sdata = readJSONData(state_filename)

    # create source directory
    if not os.path.isdir(SRC_DIR):
        log("Creating directory " + SRC_DIR)
        os.mkdir(SRC_DIR)

    # create archive files directory
    if not os.path.isdir(ARCHIVE_DIR):
        log("Creating directory " + ARCHIVE_DIR)
        os.mkdir(ARCHIVE_DIR)

    failed_libraries = []

    for library in data:
        name = library.get('name', None)
        source = library.get('source', None)
        post = library.get('postprocess', None)

        if (skip_libs) and (name in skip_libs):
            continue

        if (opt_names) and (not name in opt_names):
            continue

        lib_dir = os.path.join(SRC_DIR, name)
        lib_dir = lib_dir.replace(os.path.sep, '/')

        dlog("********** LIBRARY " + name + " **********")
        dlog("lib_dir = " + lib_dir + ")")

        # compare against cached state
        cached_state_ok = False
        if not opt_clean:
            for slibrary in sdata:
                sname = slibrary.get('name', None)
                if sname is not None and sname == name and slibrary == library and os.path.exists(lib_dir):
                    cached_state_ok = True
                    break

        if cached_state_ok:
            log("Cached state for " + name + " equals expected state; skipping library")
            continue
        else:
            # remove cached state for library
            sdata[:] = [s for s in sdata if not (lambda s, name : s.get('name', None) is not None and s['name'] == name)(s, name)]

        # create library directory, if necessary
        if opt_clean:
            log("Cleaning directory for " + name)
            if os.path.exists(lib_dir):
                shutil.rmtree(lib_dir)
        if not os.path.exists(lib_dir):
            os.makedirs(lib_dir)

        try:
            # download source
            if source is not None:
                if 'type' not in source:
                    log("ERROR: Invalid schema for " + name + ": 'source' object must have a 'type'")
                    return -1
                if 'url' not in source:
                    log("ERROR: Invalid schema for " + name + ": 'source' object must have a 'url'")
                    return -1
                src_type = source['type']
                src_url = source['url']

                if src_type == "sourcefile":
                    sha1 = source.get('sha1', None)
                    user_agent = source.get('user-agent', None)
                    try:
                        if force_fallback:
                            raise RuntimeError
                        downloadFile(src_url, ARCHIVE_DIR, name, sha1, force_download = opt_clean_archives, user_agent = user_agent)
                        filename_rel = os.path.basename(src_url)
                        shutil.copyfile( os.path.join(ARCHIVE_DIR, filename_rel), os.path.join(lib_dir, filename_rel) )
                    except:
                        if FALLBACK_URL:
                            if not force_fallback:
                                log("WARNING: Downloading of file " + src_url + " failed; trying fallback")

                            p = urlparse(src_url)
                            filename_rel = os.path.split(p.path)[1] # get original filename
                            p = urlparse(FALLBACK_URL)
                            fallback_src_url = urlunparse([p[0], p[1], p[2] + "/" + ARCHIVE_DIR_BASE + "/" + filename_rel, p[3], p[4], p[5]])
                            downloadFile(fallback_src_url, ARCHIVE_DIR, name, sha1, force_download = True)
                            shutil.copyfile( os.path.join(ARCHIVE_DIR, filename_rel), os.path.join(lib_dir, filename_rel) )
                        else:
                            shutil.rmtree(lib_dir)
                            raise
                elif src_type == "archive":
                    sha1 = source.get('sha1', None)
                    user_agent = source.get('user-agent', None)
                    try:
                        if force_fallback:
                            raise RuntimeError
                        downloadAndExtractFile(src_url, ARCHIVE_DIR, name, sha1, force_download = opt_clean_archives, user_agent = user_agent)
                    except:
                        if FALLBACK_URL:
                            if not force_fallback:
                                log("WARNING: Downloading of file " + src_url + " failed; trying fallback")

                            p = urlparse(src_url)
                            filename_rel = os.path.split(p.path)[1] # get original filename
                            p = urlparse(FALLBACK_URL)
                            fallback_src_url = urlunparse([p[0], p[1], p[2] + "/" + ARCHIVE_DIR_BASE + "/" + filename_rel, p[3], p[4], p[5]])
                            downloadAndExtractFile(fallback_src_url, ARCHIVE_DIR, name, sha1, force_download = True)
                        else:
                            raise

                else:
                    revision = source.get('revision', None)

                    archive_name = name + ".tar.gz" # for reading or writing of snapshot archives
                    if revision is not None:
                        archive_name = name + "_" + revision + ".tar.gz"

                    try:
                        if force_fallback:
                            raise RuntimeError
                        cloneRepository(src_type, src_url, name, revision)

                        if create_repo_snapshots:
                            log("Creating snapshot of library repository " + name)
                            repo_dir = os.path.join(SRC_DIR, name)
                            archive_filename = os.path.join(SNAPSHOT_DIR, archive_name)

                            dlog("Snapshot will be saved as " + archive_filename)
                            createArchiveFromDirectory(repo_dir, archive_filename, revision is None)

                    except:
                        if FALLBACK_URL:
                            if not force_fallback:
                                log("WARNING: Cloning of repository " + src_url + " failed; trying fallback")

                            # copy archived snapshot from fallback location
                            p = urlparse(FALLBACK_URL)
                            fallback_src_url = urlunparse([p[0], p[1], p[2] + "/" + SNAPSHOT_DIR_BASE + "/" + archive_name, p[3], p[4], p[5]])
                            dlog("Looking for snapshot " + fallback_src_url + " of library repository " + name)

                            # create snapshots files directory
                            downloadAndExtractFile(fallback_src_url, SNAPSHOT_DIR, name, force_download = True)

                            # reset repository state to particular revision (only using local operations inside the function)
                            cloneRepository(src_type, src_url, name, revision, True)
                        else:
                            raise
            else:
                # set up clean directory for potential patch application
                shutil.rmtree(lib_dir)
                os.mkdir(lib_dir)

            # post-processing
            if post is not None:
                if 'type' not in post:
                    log("ERROR: Invalid schema for " + name + ": 'postprocess' object must have a 'type'")
                    return -1
                if 'file' not in post:
                    log("ERROR: Invalid schema for " + name + ": 'postprocess' object must have a 'file'")
                    return -1
                post_type = post['type']
                post_file = post['file']

                if post_type == "patch":
                    applyPatchFile(post_file, name, post.get('pnum', DEFAULT_PNUM))
                elif post_type == "script":
                    runPythonScript(post_file)
                else:
                    log("ERROR: Unknown post-processing type '" + post_type + "' for " + name)
                    return -1

            # add to cached state
            sdata.append(library)

            # write out cached state
            writeJSONData(sdata, state_filename)
        except urllib.error.URLError as e:
            log("ERROR: Failure to bootstrap library " + name + " (urllib.error.URLError: reason " + str(e.reason) + ")")
            if break_on_first_error:
                exit(-1)
            traceback.print_exc()
            failed_libraries.append(name)
        except:
            log("ERROR: Failure to bootstrap library " + name + " (reason: " + str(sys.exc_info()[0]) + ")")
            if break_on_first_error:
                exit(-1)
            traceback.print_exc()
            failed_libraries.append(name)

    if failed_libraries:
        log("***************************************")
        log("FAILURE to bootstrap the following libraries:")
        log(', '.join(failed_libraries))
        log("***************************************")
        return -1

    log("Finished")

    # touch the state cache file
    os.utime(state_filename, None);

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
