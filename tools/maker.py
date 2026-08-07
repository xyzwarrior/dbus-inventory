#!/usr/bin/env python3
import mako.lookup
import argparse
import sys
import os
from pathlib import Path

def main():
    #parser = argparse.ArgumentParser(description='Process maker file.')
    #parser.add_argument('-t', '--templatedir', dest='templatedir',
    #                    default=Path(__file__).resolve().parent,
    #                    type=str, help='Location of templates files.')
    #args = parser.parse_args()
    lookup = mako.lookup.TemplateLookup(directories=[Path(__file__).resolve().parent])
    template = lookup.get_template("maker.mako.cpp")
    # define data dictinary
    makodata = {}
    makodata["interfacelist"] = sys.argv[1]
    print(template.render(**makodata))

if __name__ == '__main__':
    #print(sys.argv[1])
    main()
