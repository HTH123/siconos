#!/usr/bin/env python

import numpy as np
import h5py
import os, sys, argparse, re

parser = argparse.ArgumentParser(
    description = 'Copy a Siconos HDF5 simulation file, filtering the contents.')
parser.add_argument('fns_in', metavar='input', type=str, nargs='+',
                    help = 'input file(s) (HDF5)')
parser.add_argument('fn_out', metavar='output', type=str, nargs=1,
                    help = 'output file (HDF5)')
parser.add_argument('--start', metavar='time', type=float,
                    help = 'time in seconds to cut the start of the simulation')
parser.add_argument('--end', metavar='time', type=float,
                    help = 'time in seconds to cut the end of the simulation')
parser.add_argument('--interval', metavar='time', type=float,
                    help = 'time between frames to preserve')
parser.add_argument('--gzip', action = 'store_true',
                    help = 'enable compression in copy')
parser.add_argument('--single', action = 'store_true',
                    help = 'use single-precision floats in copy')
parser.add_argument('--exclude', type=str,
                    help = 'specify objects to exclude from copy (regexp)')

class CopyVisitor(object):
    """The CopyVisitor is called for each group and dataset in the HDF5
       file, and is responsible for copying the structure to the new
       HDF5 file."""
    def __init__(self, time_filter = None, object_filter = None):
        self.time_filter = None
        if time_filter is not None:
            self.time_filter = np.vectorize(time_filter)
        self.object_filter = object_filter
        self.time_idx = None
        self.excluded_objects = None

    def visitor(self, path, obj):
        gr = io_out
        names = path.split('/')

        # Fill list of excluded objects if it has not been done yet
        if self.excluded_objects is None and self.object_filter is not None:
            if self.object_filter is not None:
                inp = obj.file['data/input']
                self.excluded_objects = [x.attrs['id'] for name,x in inp.items()
                                         if not self.object_filter(name, inp[name])]

        # If we are copying an excluded object, return early
        if self.excluded_objects is not None and 'data/input/' in path:
            if 'id' not in obj.attrs:
                id = obj.parent.attrs['id']
            else:
                id = obj.attrs['id']
            if id in self.excluded_objects:
                return

        # Get indexes of filtered times in data/dynamic
        if (self.time_filter is not None
              and (self.time_idx is None or self.times is None)):
            dyn = obj.file['data/dynamic']
            self.time_idx = self.time_filter(dyn[:,0]).nonzero()[0]
            self.times = dyn[self.time_idx, 0]

        # Create parent groups
        if len(names) > 1:
            for i,name in enumerate(names[:-1]):
                if name in gr:
                    gr = gr[name]
                else:
                    gr = gr.create_group(name)
                    gr_in = io_in['/'.join(names[:i+1])]
                    for a in gr_in.attrs:
                        gr.attrs[a] = gr_in.attrs[a]

        # Determine chunks argument
        if obj.__class__ == h5py.Dataset:
            chunks = obj.chunks
            if np.any(np.array(chunks) > np.array(obj.shape)):
                chunks = None

            shape = obj.shape
            time_idx = None

            # Filter current indexes
            if path in ['data/cf', 'data/dynamic', 'data/velocities', 'data/static']:
                if self.time_idx is not None:
                    # Get indexes of corresponding times in current dataset
                    if path == 'data/dynamic':
                        time_idx = self.time_idx
                    # Time-filter all but static objects
                    elif path != 'data/static':
                        time_idx = np.in1d(obj[:,0], self.times).nonzero()[0]

                # Additionally remove any lines referencing excluded objects
                if self.excluded_objects is not None:
                    exclude = np.vectorize(lambda i: i in self.excluded_objects)
                    ex_idx = exclude(obj[:,1]).nonzero()[0]
                    if time_idx is None:
                        time_idx = np.arange(obj.shape[0])
                    time_idx = np.setdiff1d(time_idx, ex_idx)

            # Shape of filtered dataset
            if time_idx is not None:
                shape = (len(time_idx),) + tuple(shape[1:])

            # Create the dataset, supply compression and dtype
            # arguments, possibly overridden by command line arguments
            comp = ((obj.compression is True or args.gzip)
                    and chunks is not None)
            if comp:
                chunks = (4000,) + tuple(obj.shape[1:])
            ds = gr.create_dataset(obj.name,
                                   dtype = [obj.dtype,'f4'][args.single],
                                   shape = shape,
                                   maxshape = obj.maxshape,
                                   chunks = chunks,
                                   compression = [obj.compression, True][comp],
                                   compression_opts = [obj.compression_opts,9][comp],
                                   shuffle = comp,
                                   fletcher32 = obj.fletcher32)
            for a in obj.attrs:
                ds.attrs[a] = obj.attrs[a]

            # Copy the filtered or unfiltered dataset
            if time_idx is not None:
                if len(time_idx)==0:
                    pass
                elif len(time_idx)==1:
                    ds[0,:] = obj[time_idx[0],:]
                else:
                    ds[xrange(len(time_idx)),:] = obj[time_idx,:]
            else:
                ds[:] = obj

        # Some groups might be empty but we copy them anyway for their attributes
        elif obj.__class__ == h5py.Group:
            if path not in io_out:
                gr = io_out.create_group(path)
                for a in obj.attrs:
                    gr.attrs[a] = obj.attrs[a]

        else:
            print('Unknown type "{0}": {1}'.format(path, str(obj.__class__)))

if __name__ == '__main__':
    args = parser.parse_args()
    if os.path.exists(args.fn_out[0]):
        print('Output file "{0}" already exists!'.format(args.fn_out[0]))
        sys.exit(1)

    re_exclude = lambda _: False
    if args.exclude is not None:
        #re_exclude = re.compile(args.exclude).match
        ex = args.exclude.split(',')
        re_exclude = lambda x: x in ex

    class TimeFilter(object):
        def __init__(self):
            self.marker = None
            self.last = None
        def __call__(self, t):
            res = True
            if args.end is not None:
                res = res and t <= args.end
            if args.start is not None:
                res = res and t >= args.start

            # Interval filter is true only for every arg.interval time index
            if res is True and self.marker is None:
                self.marker = t
            if args.interval is not None and self.marker is not None:
                if self.last is None:
                    if t >= self.marker:
                        self.last = t
                        self.marker += args.interval
                    else:
                        res = False
                else:
                    if t > self.last:
                        res = False
                        self.last = None
            return res

    with h5py.File(args.fns_in[0], mode='r') as io_in:
        with h5py.File(args.fn_out[0], mode='w') as io_out:
            io_in.visititems(CopyVisitor(
                time_filter = TimeFilter(),
                object_filter = lambda name,obj: not re_exclude(name)
            ).visitor)
