import os
import sys
import getopt
import math
import shutil
from multiprocessing import Pool
from argparse import ArgumentParser
from argparse import ArgumentTypeError

ROOT = "/mnt/lustre/"
ROOT_NAME = "benchmark"
PREFIX_DIR = "dir_"
PREFIX_FILE = "file_"
PREFIX_MDT = "mdt"

total_inode_count = 0
inode_count = 0

file_per_dir = 0
file_only_root = 0
depth = 1
nb_dir = 0
nb_mdt = 1
nb_inode_mdt = 0
verbose = False

def create_file(path, nb_file):
    global total_inode_count , inode_count

    for i in range(1, nb_file + 1):
        file_path = os.path.join(path, PREFIX_FILE + str(i))
        open(file_path, "a")
        inode_count += 1
        total_inode_count += 1

def create_dir(path, nb_dir):
    global total_inode_count, inode_count

    for i in range(1, nb_dir + 1):
        dir_path = os.path.join(path, PREFIX_DIR + str(i))
        os.mkdir(dir_path)
        inode_count += 1
        total_inode_count += 1

def create_tree(path, current_depth):
    total_file = file_per_dir
    if current_depth == 0:
        total_file += file_only_root
    create_file(path, total_file)

    if current_depth + 1 < depth:
        create_dir(path, nb_dir)
        current_depth += 1
        if current_depth < depth:
            for d in range(1, nb_dir + 1):
                new_path = os.path.join(path, PREFIX_DIR + str(d))
                create_tree(new_path, current_depth)

def task_create_tree(i):
    global inode_count

    verb_print("####     START MDT"+str(i)+"     ####")
    clean_tree(i)
    root_path = os.path.join(LUSTRE_DIR, PREFIX_MDT + str(i + 1))
    inode_count = 0
    create_tree(root_path, 0)
    verb_print("Total inode created: ", inode_count, "/", nb_inode_mdt)
    verb_print("####     END MDT"+str(i)+"     ####")

    return inode_count

def verb_print(string):
    if verbose:
        print(f"{string}")

def default_int_values(value):
    if not value.isnumeric():
        raise ArgumentTypeError(f"'{value}' should be an integer")
    if int(value) < 1:
        raise ArgumentTypeError(f"'{value}' should be larger than 1")

    return value

def main():
    global nb_inode_mdt, total_inode_count, inode_count, verbose, depth, file_per_dir, file_only_root, nb_dir,usage, nb_mdt, ROOT

    parser = ArgumentParser(prog='generate_tree')
    parser.add_argument('inodes', type=int,
                        help='the number of inodes to create')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='show additional information')
    parser.add_argument('-c', '--clean', action='store_true',
                        help='clean the tree file and exit')
    parser.add_argument('-n', '--nbdir', type=default_int_values, default=1,
                        help=('the number of directories to create in each'
                              'directory, by default: 0'))
    parser.add_argument('-d', '--depth', type=default_int_values, default=1,
                        help=('the depth of the file tree, by default: 1'))
    parser.add_argument('-m', '--mdt', type=default_int_values, default=1,
                        help=('the number of mdt in which create the file'
                              'tree, by default: 1'))
    parser.add_argument('-r', '--root', type=str, default="/tmp",
                        help=('where to create the file tree,'
                              'by default: /tmp'))

    args = parser.parse_args()

    depth = int(args.depth)
    nb_dir = int(args.nbdir)
    nb_mdt = int(args.mdt)
    clean = args.clean
    verbose = args.verbose
    ROOT = os.path.abspath(args.root)

    nb_inode = args.inodes

    root_path = os.path.join(ROOT, ROOT_NAME)
    if os.path.isdir(root_path):
        shutil.rmtree(root_path)

    if clean:
        sys.exit()

    verb_print(f"Number of inodes to create: {nb_inode}")
    verb_print(f"Depth of the tree: {depth}")
    verb_print(f"Number of directories in each directory: {nb_dir}")

    # Number total of dir to create in the tree
    total_nb_dirs = 1

    for i in range(1, depth):
        total_nb_dirs += int(math.pow(nb_dir, i))

    nb_inode_mdt = int(nb_inode / nb_mdt)
    verb_print(f"Number of inodes to create in each mdt: '{nb_inode_mdt}'")

    if total_nb_dirs > nb_inode_mdt:
        print("Number of inodes too small.")
        print(f"We need to create at least '{total_nb_dirs}' inodes per mdt.")
        sys.exit()

    # Number of file to create in a dir to respect the number of inode to create"
    file_per_dir = int((nb_inode_mdt - total_nb_dirs) // total_nb_dirs)
    # Number of file to adds in the root in case it's not a multiple
    file_only_root = int((nb_inode_mdt - total_nb_dirs) % total_nb_dirs)

    if nb_mdt > 1:
        file_only_root += 1

    verb_print(f"Number of directories to create in each mdt: '{total_nb_dirs}'")
    verb_print(f"Number of files to create in each directory: '{file_per_dir}'")
    verb_print(f"Number of additional files to create in the root mdt: '{file_only_root}'")

    print("#####     START     #####")

    if nb_mdt == 1:
        os.mkdir(root_path)
        total_inode_count += 1
        create_tree(root_path, 0)
    else:
        with Pool() as pool:
            for result in pool.map(task_create_tree, range(nb_mdt)):
                total_inode_count += result

    print("Total inode created: ", total_inode_count, "/", nb_inode)
    print("#####     END     #####")


if __name__ == "__main__":
    main()
