#!/usr/bin/python

import os
import sys
import os.path
import glob
import shutil
import tarfile
import tempfile
import getopt

class FileExtractException(Exception):
    def __init__(self, info):
        self.info = info

    def __str__(self):
        return self.info

def extract_file_from_archive(filename, dest, tar=None):
    try:
        if not os.path.isdir(dest):
            os.makedirs(dest)

        temp = tempfile.mkdtemp()

        if tar == None:
            tf = tarfile.open(glob.glob(os.path.join(os.getcwd(), '*.bz2'))[0], 'r')
        else:
            tf = tarfile.open(tar, 'r')
        tf.extract(filename, temp)
        shutil.copyfile(os.path.join(temp, filename), os.path.join(dest, os.path.basename(filename)))

    except IndexError:
        raise FileExtractException("No valid tar file found")

    except KeyError:
        errstr = "File " + filename + " not found in archive"
        raise FileExtractException(errstr)

    except IOError as e:
        filename = ": '" + e.filename + "'" if e.filename != None else ""
        errstr = "IOError occurred whilst extracting file from archive: [Errno: " + str(e.errno) + "] " + e.strerror + filename
        raise FileExtractException(errstr)

    except:
        raise FileExtractException("Unexpected error")

    finally:
        if temp:
            shutil.rmtree(temp)

if __name__ == '__main__':
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'd:o:')
    except getopt.GetoptError, err:
        print str(err)
        exit(1)

    cwd = os.getcwd()
    dest = cwd
    tarball = None

    for o, a in opts:
        if o == "-d":
            dest = os.path.abspath(a)
        elif o == "-o":
            tarball = os.path.abspath(a)

    if len(args) != 1:
        print >> sys.stderr, "Need to specify one file"
        exit(1)

    filename = args[0]

    if os.path.exists(os.path.join(dest, filename)):
        exit(0)

    if filename.startswith('/'):
        print >> sys.stderr, "Input filename shouldn't be absolute"
        exit(1)

    if not os.path.isdir(dest):
        os.makedirs(dest)

    if tarball == None:
        try:
            extract_file_from_archive(filename, dest)

        except FileExtractException as e:
            print >> sys.stderr, str(e)
            exit(1)

        except:
            print >> sys.stderr, "Unexpected error"
            exit(1)
    else:
        try:
            temp = tempfile.mkdtemp()

            tb = tarfile.open(tarball, 'r')
            names = tb.getnames()

            if os.path.join(names[0], filename) in names:
                tb.extract(os.path.join(names[0], filename), temp)
                shutil.copyfile(os.path.join(temp, names[0], filename), os.path.join(dest, filename))
            else:
                bz2file = None
                for name in names:
                    (root, ext) = os.path.splitext(name)
                    if ext == '.bz2':
                        bz2file = name
                        break
                if bz2file:
                    tb.extract(bz2file, temp)
                else:
                    print >> sys.stderr, "File not found and no valid embedded tar file found in source tarball"
                    exit(1)

                extract_file_from_archive(filename, dest, os.path.join(temp, bz2file))

        except FileExtractException as e:
            print >> sys.stderr, str(e)
            exit(1)

        finally:
            if temp:
                shutil.rmtree(temp)
